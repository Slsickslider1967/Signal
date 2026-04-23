#include "MDU/FileWatcher.h"

#include <system_error>

namespace
{
    // Normalize a path to an absolute, stable string form when possible.
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
    // Replace watch roots with a normalized list of paths.
    void FileWatcher::SetWatchPaths(const std::vector<std::string>& Paths)
    {
        WatchPaths.clear();
        WatchPaths.reserve(Paths.size());

        for (const auto& Path : Paths)
        {
            WatchPaths.push_back(MakeAbsolutePath(Path));
        }
    }

    // Add one more normalized watch root.
    void FileWatcher::AddWatchPath(const std::string& Path)
    {
        WatchPaths.push_back(MakeAbsolutePath(Path));
    }

    // Return the current watch root list.
    const std::vector<std::string>& FileWatcher::GetWatchPaths() const
    {
        return WatchPaths;
    }

    // Set the file extension filter used during scans.
    void FileWatcher::SetExtensionFilter(const std::string& Extension)
    {
        ExtensionFilter = Extension;
    }

    // Return the active extension filter.
    const std::string& FileWatcher::GetExtensionFilter() const
    {
        return ExtensionFilter;
    }

    // Prime the baseline snapshot so future polls report deltas.
    void FileWatcher::PrimeSnapshot()
    {
        KnownFiles = BuildSnapshot();
    }

    // Compare current files against the previous snapshot and emit add/modify/remove events.
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

    // Walk every watched folder and capture modification times for matching files.
    std::unordered_map<std::string, std::filesystem::file_time_type> FileWatcher::BuildSnapshot() const
    {
        std::unordered_map<std::string, std::filesystem::file_time_type> snapshot;

        // Each watch path contributes files into one merged snapshot map.
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