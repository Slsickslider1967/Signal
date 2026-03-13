#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace MDU
{
    enum class FileChangeType
    {
        Added,
        Modified,
        Removed
    };

    struct FileChange
    {
        std::string Path;
        FileChangeType Type;
    };

    class FileWatcher
    {
    public:
        FileWatcher() = default;

        void SetWatchPaths(const std::vector<std::string>& paths);
        void AddWatchPath(const std::string& path);
        const std::vector<std::string>& GetWatchPaths() const;

        void SetExtensionFilter(const std::string& extension);
        const std::string& GetExtensionFilter() const;

        void PrimeSnapshot();
        std::vector<FileChange> PollChanges();

    private:
        std::vector<std::string> WatchPaths;
        std::string ExtensionFilter = ".mdu";

        std::unordered_map<std::string, std::filesystem::file_time_type> KnownFiles;

        std::unordered_map<std::string, std::filesystem::file_time_type> BuildSnapshot() const;
    };
}
#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace MDU
{
    enum class FileChangeType 
    {
        Added,
        Modified,
        Removed
    };

    struct FileChange
    {
        std::string Path;
        FileChangeType Type;
    };

    class FileWatcher
    {
        public:
            FileWatcher() = default;

            void SetWatchPaths(const std::vector<std::string>& paths);
            void AddWatchPath(const std::string& path);
            const std::vector<std::string>& GetWatchPaths() const;

            void SetExtensionFilter(const std::string& extension);
            const std::string& GetExtensionFilter() const;

            void PrimeSnapshot();
            std::vector<FileChange> DetectChanges();

        private:
            std::vector<std::string> WatchPaths;
            std::string ExtensionFilter = ".mdu";

            std::unordered_map<std::string, std::filesystem::file_time_type> KnownFiles;
            std::unordered_map<std::string, std::filesystem::file_time_type> BuildSnapshot() const;
    };
}
