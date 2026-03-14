#include "MDU/FileWatcher.h"

#include <system_error>

namespace
{
    std::string MakeAbsolutePath(const std::string& Path)
    {
        std::error_code ErrorCode;
        const std::filesystem::path AbsolutePath = std::filesystem::absolute(Path, ErrorCode);
        if (ErrorCode)
        {
            return Path;
        }
        return AbsolutePath.lexically_normal().string();
    }
}

namespace MDU
{
    void FileWatcher::SetWatchPaths(const std::vector<std::string>& Paths)
    {
        WatchPaths.clear();
        WatchPaths.reserve(Paths.size());

        for (const auto& Path : Paths)
        {
            WatchPaths.push_back(MakeAbsolutePath(Path));
        }
    }

    void FileWatcher::AddWatchPath(const std::string& Path)
    {
        WatchPaths.push_back(MakeAbsolutePath(Path));
    }

    const std::vector<std::string>& FileWatcher::GetWatchPaths() const
    {
        return WatchPaths;
    }

    void FileWatcher::SetExtensionFilter(const std::string& Extension)
    {
        ExtensionFilter = Extension;
    }

    const std::string& FileWatcher::GetExtensionFilter() const
    {
        return ExtensionFilter;
    }

    void FileWatcher::PrimeSnapshot()
    {
        KnownFiles = BuildSnapshot();
    }

    std::vector<FileChange> FileWatcher::PollChanges()
    {
        std::vector<FileChange> Changes;

        const auto CurrentSnapshot = BuildSnapshot();

        for (const auto& [Path, Time] : CurrentSnapshot)
        {
            if (KnownFiles.find(Path) == KnownFiles.end())
            {
                Changes.push_back({Path, FileChangeType::Added});
            }
            else if (KnownFiles[Path] != Time)
            {
                Changes.push_back({Path, FileChangeType::Modified});
            }
        }

        for (const auto& [Path, Time] : KnownFiles)
        {
            if (CurrentSnapshot.find(Path) == CurrentSnapshot.end())
            {
                Changes.push_back({Path, FileChangeType::Removed});
            }
        }

        KnownFiles = std::move(CurrentSnapshot);
        return Changes;
    }

    std::unordered_map<std::string, std::filesystem::file_time_type> FileWatcher::BuildSnapshot() const
    {
        std::unordered_map<std::string, std::filesystem::file_time_type> snapshot;

        for (const auto& watchPath : WatchPaths)
        {
            std::error_code ErrorCode;
            if (!std::filesystem::exists(watchPath, ErrorCode) || ErrorCode)
            {
                continue;
            }

            for (const auto& entry : std::filesystem::recursive_directory_iterator(
                     watchPath,
                     std::filesystem::directory_options::skip_permission_denied,
                     ErrorCode))
            {
                if (ErrorCode)
                {
                    break;
                }

                if (!entry.is_regular_file())
                {
                    continue;
                }

                if (entry.path().extension() != ExtensionFilter)
                {
                    continue;
                }

                std::error_code timeError;
                const auto lastWriteTime = std::filesystem::last_write_time(entry.path(), timeError);
                if (timeError)
                {
                    continue;
                }

                snapshot[entry.path().lexically_normal().string()] = lastWriteTime;
            }
        }
        return snapshot;
    }
}