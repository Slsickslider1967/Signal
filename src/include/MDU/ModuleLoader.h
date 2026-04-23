#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

#include "MDU/mduParser.h"

namespace MDU
{
	std::string TrimString(const std::string& value);
	std::filesystem::path GetMduSearchPathSettingsFile();
	std::vector<std::string> NormalizeAndUniquePaths(const std::vector<std::string>& paths);
	std::vector<std::string> LoadMduSearchPathsFromSettingsFile();
	bool SaveMduSearchPathsToSettingsFile(const std::vector<std::string>& paths);
	void RemoveMduSearchPathFromSettingsFile(const std::string& pathToRemove);

	struct LoadedModule
	{
		std::string SourcePath;
		std::string SharedObjectPath;

		void* SharedObjectHandle = nullptr;

		MetaData Metadata;
		CreateFn Create = nullptr;
		DestroyFn Destroy = nullptr;
	};

	class ModuleLoader
	{
	public:
		explicit ModuleLoader(std::string cacheDirectory = "build/mdu_cache");
		~ModuleLoader();

		void SetSearchPaths(const std::vector<std::string>& paths);
		void AddSearchPath(const std::string& path);
		const std::vector<std::string>& GetSearchPaths() const;
		void SetTemplatePath(const std::string& path);
		std::filesystem::path GetTemplatePath() const;

		std::vector<std::string> DiscoverMduFiles() const;

		bool ScanAndLoadAll(std::string* errorOut = nullptr);
		bool LoadFromMduFile(const std::string& mduPath, std::string* errorOut = nullptr);
		bool UnloadByPath(const std::string& mduPath, std::string* errorOut = nullptr);
		void UnloadAll();

		const std::unordered_map<std::string, LoadedModule>& GetLoadedModules() const;
		std::vector<MetaData> GetAvailableMetaData() const;

	private:
		std::string CacheDirectory;
		std::vector<std::string> SearchPaths;
		std::unordered_map<std::string, LoadedModule> LoadedModulesByPath;
		std::filesystem::path TemplatePath;

		bool EnsureCacheDirectory(std::string* errorOut) const;
		std::string BuildSharedObjectPath(const std::string& mduPath) const;
		bool CompileMduToSharedObject(const std::string& mduPath, const std::string& soPath, std::string* errorOut) const;
		
	};
}
