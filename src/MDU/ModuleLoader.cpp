#include <algorithm>
#include "MDU/ModuleLoader.h"

#include <cstdlib>
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#endif
#include <filesystem>
#include <fstream>
#include <sstream>
#include <cstdio> // for _popen
#include <system_error>
#include <iostream>


#if defined(_WIN32)
// Utility to convert all backslashes to forward slashes in a path
inline std::string ToForwardSlashes(const std::string& path) {
	std::string result = path;
	std::replace(result.begin(), result.end(), '\\', '/');
	return result;
}
#else
inline std::string ToForwardSlashes(const std::string& path) { return path; }
#endif

namespace {
#if defined(_WIN32)
	// Use double quotes for Windows shell
	std::string EscapeForShell(const std::string &Text)
	{
		std::string Escaped = Text;
		// Escape embedded double quotes
		size_t pos = 0;
		while ((pos = Escaped.find('"', pos)) != std::string::npos) {
			Escaped.insert(pos, "\\");
			pos += 2;
		}
		return "\"" + Escaped + "\"";
	}
#else
	// Use single quotes for Linux shell
	std::string EscapeForShell(const std::string &Text)
	{
		std::string Escaped;
		Escaped.reserve(Text.size() + 8);
		for (char Character : Text)
		{
			if (Character == '\'')
			{
				Escaped += "'\\''";
			}
			else
			{
				Escaped.push_back(Character);
			}
		}
		return "'" + Escaped + "'";
	}
#endif

	std::string MakeAbsolutePath(const std::string &maybeRelative)
	{
		std::error_code ErrorCode;
		const std::filesystem::path abs = std::filesystem::absolute(maybeRelative, ErrorCode);
		if (ErrorCode)
		{
			return maybeRelative;
		}
		return abs.lexically_normal().string();
	}
}

namespace MDU
{
	ModuleLoader::ModuleLoader(std::string cacheDirectory)
		: CacheDirectory(std::move(cacheDirectory))
	{
	}

	ModuleLoader::~ModuleLoader()
	{
		UnloadAll();
	}

	void ModuleLoader::SetSearchPaths(const std::vector<std::string> &Paths)
	{
		SearchPaths.clear();
		SearchPaths.reserve(Paths.size());

		for (const auto &Path : Paths)
		{
			SearchPaths.push_back(MakeAbsolutePath(Path));
		}
	}

	void ModuleLoader::AddSearchPath(const std::string &Path)
	{
		SearchPaths.push_back(MakeAbsolutePath(Path));
	}

	const std::vector<std::string> &ModuleLoader::GetSearchPaths() const
	{
		return SearchPaths;
	}

	std::vector<std::string> ModuleLoader::DiscoverMduFiles() const
	{
		std::vector<std::string> Files;

		for (const auto &basePath : SearchPaths)
		{
			std::error_code ErrorCode;
			if (!std::filesystem::exists(basePath, ErrorCode) || ErrorCode)
			{
				continue;
			}

			for (const auto &entry : std::filesystem::recursive_directory_iterator(basePath, std::filesystem::directory_options::skip_permission_denied, ErrorCode))
			{
				if (ErrorCode)
				{
					break;
				}

				if (!entry.is_regular_file())
				{
					continue;
				}

				if (entry.path().extension() == ".mdu")
				{
					Files.push_back(entry.path().lexically_normal().string());
				}
			}
		}

		return Files;
	}

	bool ModuleLoader::ScanAndLoadAll(std::string *ErrorOut)
	{
		const auto Files = DiscoverMduFiles();

		for (const auto &File : Files)
		{
			if (!LoadFromMduFile(File, ErrorOut))
			{
				return false;
			}
		}

		return true;
	}

	bool ModuleLoader::LoadFromMduFile(const std::string &mduPath, std::string *ErrorOut)
	{
		const std::string canonicalPath = MakeAbsolutePath(mduPath);

		if (!EnsureCacheDirectory(ErrorOut))
		{
			return false;
		}

		const auto parse = ParseMDUFile(canonicalPath);
		if (!parse.ValidMDUFile)
		{
			if (ErrorOut)
			{
				*ErrorOut = "[MLD001] MDU parse failed for " + canonicalPath + ": " + parse.Error;
			}
			return false;
		}

		std::string unloadError;
		UnloadByPath(canonicalPath, &unloadError);

		const std::string sharedObjectPath = BuildSharedObjectPath(canonicalPath);
		if (!CompileMduToSharedObject(canonicalPath, sharedObjectPath, ErrorOut))
		{
			return false;
		}

	#if defined(_WIN32)
		   HMODULE handle = LoadLibraryA(sharedObjectPath.c_str());
		   if (!handle) {
			   if (ErrorOut) {
				   *ErrorOut = std::string("[MLD003] LoadLibrary failed for ") + sharedObjectPath;
			   }
			   return false;
		   }
		   auto createFn = reinterpret_cast<CreateFn>(GetProcAddress(handle, "mdu_create"));
		   auto destroyFn = reinterpret_cast<DestroyFn>(GetProcAddress(handle, "mdu_destroy"));
		   auto getMetadataFn = reinterpret_cast<GetMetaDataFn>(GetProcAddress(handle, "mdu_get_metadata"));
		   if (!createFn || !destroyFn || !getMetadataFn) {
			   if (ErrorOut) {
				   *ErrorOut = std::string("[MLD004] GetProcAddress failed for ") + sharedObjectPath + ": expected symbols mdu_create/mdu_destroy/mdu_get_metadata";
			   }
			   FreeLibrary(handle);
			   return false;
		   }
	#else
		   dlerror();
		   void *handle = dlopen(sharedObjectPath.c_str(), RTLD_NOW | RTLD_LOCAL);
		   if (!handle)
		   {
			   const char *dlOpenError = dlerror();
			   if (ErrorOut)
			   {
				   *ErrorOut = "[MLD003] dlopen failed for " + sharedObjectPath + ": " + std::string(dlOpenError ? dlOpenError : "unknown error");
			   }
			   return false;
		   }
		   auto createFn = reinterpret_cast<CreateFn>(dlsym(handle, "mdu_create"));
		   auto destroyFn = reinterpret_cast<DestroyFn>(dlsym(handle, "mdu_destroy"));
		   auto getMetadataFn = reinterpret_cast<GetMetaDataFn>(dlsym(handle, "mdu_get_metadata"));
		   const char *symbolError = dlerror();
		   if (symbolError != nullptr || createFn == nullptr || destroyFn == nullptr || getMetadataFn == nullptr)
		   {
			   if (ErrorOut)
			   {
				   *ErrorOut = "[MLD004] dlsym failed for " + sharedObjectPath + ": expected symbols mdu_create/mdu_destroy/mdu_get_metadata";
				   if (symbolError != nullptr)
				   {
					   *ErrorOut += std::string(" (") + symbolError + ")";
				   }
			   }
			   dlclose(handle);
			   return false;
		   }
	#endif
		   LoadedModule module;
		   module.SourcePath = canonicalPath;
		   module.SharedObjectPath = sharedObjectPath;
		   module.SharedObjectHandle = handle;
		   module.Create = createFn;
		   module.Destroy = destroyFn;
		   module.GetMetaData = getMetadataFn;
		   module.Metadata = parse.metadata;
		   LoadedModulesByPath[canonicalPath] = module;
		   return true;
	}

	bool ModuleLoader::UnloadByPath(const std::string &mduPath, std::string *errorOut)
	{
		const std::string canonicalPath = MakeAbsolutePath(mduPath);
		const auto Item = LoadedModulesByPath.find(canonicalPath);
		if (Item == LoadedModulesByPath.end())
		{
			return true;
		}

		#if defined(_WIN32)
			   if (Item->second.SharedObjectHandle != nullptr)
			   {
				   if (!FreeLibrary((HMODULE)Item->second.SharedObjectHandle))
				   {
					   if (errorOut)
					   {
						   *errorOut = "[MLD005] FreeLibrary failed for " + Item->second.SharedObjectPath;
					   }
					   return false;
				   }
			   }
		#else
			   if (Item->second.SharedObjectHandle != nullptr)
			   {
				   if (dlclose(Item->second.SharedObjectHandle) != 0)
				   {
					   if (errorOut)
					   {
						   *errorOut = "[MLD005] dlclose failed for " + Item->second.SharedObjectPath + ": " + (dlerror() ? dlerror() : "unknown error");
					   }
					   return false;
				   }
			   }
		#endif

		LoadedModulesByPath.erase(Item);
		return true;
	}

	void ModuleLoader::UnloadAll()
	{
		#if defined(_WIN32)
			   for (auto Item = LoadedModulesByPath.begin(); Item != LoadedModulesByPath.end();)
			   {
				   if (Item->second.SharedObjectHandle != nullptr)
				   {
					   FreeLibrary((HMODULE)Item->second.SharedObjectHandle);
				   }
				   Item = LoadedModulesByPath.erase(Item);
			   }
		#else
			   for (auto Item = LoadedModulesByPath.begin(); Item != LoadedModulesByPath.end();)
			   {
				   if (Item->second.SharedObjectHandle != nullptr)
				   {
					   dlclose(Item->second.SharedObjectHandle);
				   }
				   Item = LoadedModulesByPath.erase(Item);
			   }
		#endif
	}

	const std::unordered_map<std::string, LoadedModule> &ModuleLoader::GetLoadedModules() const
	{
		return LoadedModulesByPath;
	}

	std::vector<MetaData> ModuleLoader::GetAvailableMetaData() const
	{
		std::vector<MetaData> metadata;
		metadata.reserve(LoadedModulesByPath.size());

		for (const auto &pair : LoadedModulesByPath)
		{
			metadata.push_back(pair.second.Metadata);
		}

		return metadata;
	}

	bool ModuleLoader::EnsureCacheDirectory(std::string *errorOut) const
	{
		std::error_code ErrorCode;
		std::filesystem::create_directories(CacheDirectory, ErrorCode);
		if (ErrorCode)
		{
			if (errorOut)
			{
				*errorOut = "[MLD006] Failed creating cache directory '" + CacheDirectory + "': " + ErrorCode.message();
			}
			return false;
		}
		return true;
	}

	std::string ModuleLoader::BuildSharedObjectPath(const std::string &mduPath) const
	{
		std::filesystem::path sourcePath(mduPath);
		std::filesystem::path fileName = sourcePath.filename();
	#if defined(_WIN32)
		fileName += ".dll";
	#else
		fileName += ".so";
	#endif

		std::filesystem::path outPath(CacheDirectory);
		outPath /= fileName;

		return outPath.lexically_normal().string();
	}

	bool ModuleLoader::CompileMduToSharedObject(const std::string &mduPath, const std::string &sharedObjectPath, std::string *errorOut) const
	{
		std::ostringstream cmd;

	#ifdef _WIN32
		// Always preprocess .mdu to strip metadata block on Windows (MSVC or MinGW)
		std::ifstream inFile(mduPath);
		if (!inFile.is_open()) {
			if (errorOut) *errorOut = std::string("[MLD002] Failed to open ") + mduPath;
			return false;
		}
		std::string line;
		bool inHeader = false;
		std::ostringstream cppContent;
		while (std::getline(inFile, line)) {
			if (!inHeader && line.find("/*! Module") != std::string::npos) {
				inHeader = true;
				continue;
			}
			if (inHeader && line.find("*/") != std::string::npos) {
				inHeader = false;
				continue;
			}
			if (!inHeader) {
				cppContent << line << "\n";
			}
		}
		inFile.close();
		// Write to temp .cpp file
		std::string tempCpp = sharedObjectPath + ".cpp";
		std::ofstream outFile(tempCpp);
		if (!outFile.is_open()) {
			if (errorOut) *errorOut = std::string("[MLD002] Failed to write temp cpp: ") + tempCpp;
			return false;
		}
		outFile << cppContent.str();
		outFile.close();

		// Always use g++ on Windows
		cmd << "g++"
			<< " -shared -std=c++17"
			<< " -Iinclude -Iinclude/MDU -I../include -I../include/MDU"
			<< " -Iexternal/imgui -I../external/imgui -Ibuild/_deps/imgui-src -I../build/_deps/imgui-src -I_deps/imgui-src"
			<< " -Iexternal/imgui_knobs -I../external/imgui_knobs -Ibuild/_deps/imgui_knobs-src -I../build/_deps/imgui_knobs-src -I_deps/imgui_knobs-src"
			<< " " << tempCpp
			<< " -o " << sharedObjectPath;
	#else
		// Linux/macOS: use g++ for .so
		cmd << "g++"
			<< " -shared -fPIC -std=c++17"
			<< " -I" << EscapeForShell("include")
			<< " -I" << EscapeForShell("include/MDU")
			<< " -I" << EscapeForShell("../include")
			<< " -I" << EscapeForShell("../include/MDU")
			<< " -I" << EscapeForShell("external/imgui")
			<< " -I" << EscapeForShell("../external/imgui")
			<< " -I" << EscapeForShell("build/_deps/imgui-src")
			<< " -I" << EscapeForShell("../build/_deps/imgui-src")
			<< " -I" << EscapeForShell("_deps/imgui-src")
			<< " -I" << EscapeForShell("external/imgui_knobs")
			<< " -I" << EscapeForShell("../external/imgui_knobs")
			<< " -I" << EscapeForShell("build/_deps/imgui_knobs-src")
			<< " -I" << EscapeForShell("../build/_deps/imgui_knobs-src")
			<< " -I" << EscapeForShell("_deps/imgui_knobs-src")
			<< " -x c++ " << EscapeForShell(mduPath)
			<< " -o " << EscapeForShell(sharedObjectPath);
	#endif
		std::string command = cmd.str();
		std::string output;
		int ReturnCode = 0;
	#if defined(_WIN32)
			// Use _popen to capture output
			std::cerr << "[MDU DEBUG] Compiling module with command: " << command << std::endl;
			FILE* pipe = _popen(command.c_str(), "r");
			if (!pipe) ReturnCode = -1;
			else {
				char buffer[256];
				while (fgets(buffer, sizeof(buffer), pipe)) {
					output += buffer;
					std::cerr << "[MDU DEBUG] " << buffer;
				}
				ReturnCode = _pclose(pipe);
			}
	#else
			std::cerr << "[MDU DEBUG] Compiling module with command: " << command << std::endl;
			output = "";
			ReturnCode = std::system(command.c_str());
	#endif
			if (ReturnCode != 0) {
				if (errorOut) {
					*errorOut = "[MLD002] Failed compiling module: " + mduPath + " (exit code " + std::to_string(ReturnCode) + ")\nCommand: " + command + "\nOutput:\n" + output;
				}
				std::cerr << "[MDU DEBUG] Module compile failed!" << std::endl;
				return false;
			}
			std::cerr << "[MDU DEBUG] Module compile succeeded." << std::endl;
			return true;
	}

	void ModuleLoader::SetTemplatePath(const std::string &path)
	{
		std::error_code errorCode;
		const std::filesystem::path absolutePath = std::filesystem::absolute(path, errorCode);
		if (errorCode)
		{
			TemplatePath = std::filesystem::path(path);
		}
		else
		{
			TemplatePath = absolutePath.lexically_normal();
		}
	}

	std::filesystem::path ModuleLoader::GetTemplatePath() const
	{
		return TemplatePath;
	}
}
