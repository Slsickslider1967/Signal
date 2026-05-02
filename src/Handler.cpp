#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <limits.h>
#include <system_error>
#include <iostream>
#include <list>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

// Platform-specific includes for module loading and process handling
#if defined(__linux__) || defined(__unix__) || defined(__APPLE__)
#include <unistd.h>
#endif

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include "imgui.h"
#include "Audio/Audio.h"
#include "Draw/Draw.h"
#include "Draw/ImGuiUtil.h"
#include "Draw/Window.h"
#include "MDU/FileWatcher.h"
#include "Functions/ConsoleHandling.h"
#include "HandlerShared.h"


// -- Global State --

int NextRackID = 1;
int NextModuleID = 1;
int NextLinkID = 1;
int SelectedModuleID = -1;

std::list<int> SelectedRackIDs;
std::list<int> SelectedModuleIDs;
std::list<Rack> Racks;

MDU::ModuleLoader GlobalModuleLoader;
static MDU::FileWatcher GlobalFileWatcher;

std::mutex GlobalRackMutex;

std::string GlobalLastMduError;

std::map<int, std::vector<float>> GlobaloduleScopeInputs;
std::map<int, std::vector<float>> GlobalModuleScopeOutputs;

static bool GlobalFileWatcherInitialized = false;
bool GlobalShowDebugConsole = false;
bool ShowModuleDetails = false;
bool IsRecording = false;

// K meaning constant
static constexpr int kScopeSampleCount = 512;

// -- Function prototypes --

void SetupAudioHandling();
void ShutdownAudioHandling();
void UpdateAudioWaveFormsFromRacks();
void AudioFilterCallback(float *buffer, int numSamples, void *userData);

Rack *CreateRack(const std::string &name);
void DeleteRack(int rackID);
bool AddDynamicModuleToRack(Rack &rack, const std::string &sourcePath, std::string *errorOut);
void RemoveDynamicModuleFromRack(DynamicModule &module);
void RemoveNode(int nodeID);
void LaunchDefaultFileManager(const std::filesystem::path &path);
std::filesystem::path GetDefaultTemplatePath();

std::vector<std::string> BuildMduRuntimePaths();
std::vector<std::string> BuildMduPathsFromEnvironment();
void RemoveLinksForModule(Rack &rack, int moduleID);
void RemoveDynamicModulesFromAllRacksBySourcePath(const std::string &sourcePath);
void ProcessMduFileChanges();


// Capture a fixed-size snapshot of samples for module scope visualization.
// Missing samples are padded with zeros to keep the buffer shape stable.
void CaptureScopeSamples(std::map<int, std::vector<float>> &scopeMap,
                         int moduleID,
                         const float *source,
                         int numSamples)
{
    std::vector<float> &scope = scopeMap[moduleID];

    // Ensure the scope vector has exactly kScopeSampleCount samples
    if (scope.size() != static_cast<size_t>(kScopeSampleCount))
    {
        scope.assign(kScopeSampleCount, 0.0f);
    }

    // If source is null or numSamples is non-positive fill the scope with zeros and return
    if (source == nullptr || numSamples <= 0)
    {
        std::fill(scope.begin(), scope.end(), 0.0f);
        return;
    }

    // else copy samples from source to scope, up to kScopeSampleCount and fill the rest with zeros if needed
    int copyCount = std::min(kScopeSampleCount, numSamples);
    std::copy(source, source + copyCount, scope.begin());
    if (copyCount < kScopeSampleCount)
    {
        std::fill(scope.begin() + copyCount, scope.end(), 0.0f);
    }
}

// Build a dependency-aware execution order for active modules in one rack.
// Upstream modules run first when links define a clear dependency chain.
std::vector<DynamicModule *> BuildProcessingOrder(Rack &rack)
{
    std::vector<DynamicModule *> ordered;
    std::map<int, DynamicModule *> moduleByID;
    std::set<int> activeModuleIDs;

    for (auto &module : rack.DynamicModules)
    {
        if (!module.Active || module.Instance == nullptr)
        {
            continue;
        }
        moduleByID[module.ID] = &module;
        activeModuleIDs.insert(module.ID);
    }

    std::set<int> processed;
    while (processed.size() < moduleByID.size())
    {
        bool progressed = false;

        for (auto &[moduleID, modulePtr] : moduleByID)
        {
            if (processed.find(moduleID) != processed.end())
            {
                continue;
            }

            bool ready = true;
            for (const auto &link : rack.Links)
            {
                if (link.EndModuleID != moduleID)
                {
                    continue;
                }

                if (activeModuleIDs.find(link.StartModuleID) == activeModuleIDs.end())
                {
                    continue;
                }

                if (processed.find(link.StartModuleID) == processed.end())
                {
                    ready = false;
                    break;
                }
            }

            if (ready)
            {
                ordered.push_back(modulePtr);
                processed.insert(moduleID);
                progressed = true;
            }
        }

        if (!progressed)
        {
            // If dependency resolution stalls, process the remaining modules anyway.
            for (auto &[moduleID, modulePtr] : moduleByID)
            {
                if (processed.find(moduleID) == processed.end())
                {
                    ordered.push_back(modulePtr);
                    processed.insert(moduleID);
                }
            }
        }
    }

    return ordered;
}

// Return the connected output buffer for a specific destination pin.
// Returns nullptr when no valid link or source buffer exists.
const float *FindLinkedOutputBuffer(const Rack &rack,
                                    int endModuleID,
                                    int endPinIndex,
                                    const std::map<std::pair<int, int>, std::vector<float>> &outputBuffers)
{
    for (const auto &link : rack.Links)
    {
        if (link.EndModuleID != endModuleID || link.EndPinIndex != endPinIndex)
        {
            continue;
        }

        auto it = outputBuffers.find({link.StartModuleID, link.StartPinIndex});
        if (it == outputBuffers.end())
        {
            return nullptr;
        }

        if (it->second.empty())
        {
            return nullptr;
        }

        return it->second.data();
    }

    return nullptr;
}

// Start the app loop and keep UI, rendering, and module hot-reload ticking.
// Exits when the window requests shutdown.
int main()
{
    GlobalModuleLoader.SetTemplatePath(GetDefaultTemplatePath().string());
    Draw::MainWindow();

    while (!Window::ShouldClose())
    {
        ImGuiUtil::Begin();

        std::lock_guard<std::mutex> rackLock(GlobalRackMutex);

        Draw::DrawTopBar();

        ImGuiViewport *mainViewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(mainViewport->WorkPos);
        ImGui::SetNextWindowSize(mainViewport->WorkSize);
        #if defined(__linux__) || defined(__unix__) || defined(__APPLE__)
        #ifdef IMGUI_HAS_VIEWPORT
            ImGui::SetNextWindowViewport(mainViewport->ID);
        #endif
        #endif

        ImGuiWindowFlags rackManagerFlags =
            // ImGuiWindowFlags_NoDocking |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoBringToFrontOnFocus |
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoNavFocus;

        ImGui::SetNextWindowCollapsed(false, ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(1.0f);
        ImGui::Begin("Rack Manager", nullptr, rackManagerFlags);

        for (auto rackIt = Racks.begin(); rackIt != Racks.end();)
        {
            Rack &rack = *rackIt;
            ImGui::PushID(rack.ID);

            std::string rackHeaderLabel = "Rack #" + std::to_string(rack.ID) + ": " + rack.Name + "###RackHeader" + std::to_string(rack.ID);
            bool rackOpen = ImGui::TreeNodeEx(rackHeaderLabel.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
            if (ImGui::IsItemClicked())
            {
                auto selectedIt = std::find(SelectedRackIDs.begin(), SelectedRackIDs.end(), rack.ID);
                if (selectedIt == SelectedRackIDs.end())
                {
                    SelectedRackIDs.push_back(rack.ID);
                }
                else
                {
                    SelectedRackIDs.erase(selectedIt);
                }
            }

            if (rackOpen)
            {
                Draw::DrawRackEditor(rack);
                if (Draw::PopUpTool(rack))
                {
                    ImGui::TreePop();
                    int rackIDToDelete = rack.ID;
                    auto nextRackIt = std::next(rackIt);
                    DeleteRack(rackIDToDelete);
                    rackIt = nextRackIt;
                    ImGui::PopID();
                    continue;
                }
                ImGui::TreePop();
            }

            ImGui::PopID();
            ++rackIt;
        }

        ImGui::End();


        if (SelectedModuleID != -1 && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
        {
            ShowModuleDetails = true;
        }

        if (ShowModuleDetails)
        {
            Draw::DrawModuleDetails();
        }

        ImGuiUtil::End();

        Draw::Render();

        UpdateAudioWaveFormsFromRacks();
        ProcessMduFileChanges();

        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    Draw::CleanUp();
    return 0;
}

// --Audio Handling--

// Initialize the audio backend and register the main processing callback.
void SetupAudioHandling()
{
    Audio::Init();
    Audio::SetFilterCallback(AudioFilterCallback, nullptr);
}

// Keep legacy oscillator playback disabled while module processing drives audio.
void UpdateAudioWaveFormsFromRacks()
{
    // Keep legacy oscillator list empty so only the MDU callback writes audio.
    static bool initialized = false;
    if (!initialized)
    {
        std::vector<WaveForm> empty;
        Audio::SetWaveForms(empty);
        initialized = true;
    }
}

// Run one audio block: process modules in order and mix output modules into the final buffer.
// Also updates scope input/output snapshots used by the UI.
void AudioFilterCallback(float *buffer, int numSamples, void *userData)
{
    (void)userData;

    std::lock_guard<std::mutex> rackLock(GlobalRackMutex);

    if (buffer == nullptr || numSamples <= 0)
    {
        return;
    }

    for (int i = 0; i < numSamples; ++i)
    {
        buffer[i] = 0.0f;
    }

    for (auto &rack : Racks)
    {
        if (!rack.Enabled || rack.DynamicModules.empty())
        {
            continue;
        }

        // Keyed by {moduleID, outputPinIndex} so links can fetch upstream audio quickly.
        std::map<std::pair<int, int>, std::vector<float>> outputBuffers;
        std::vector<DynamicModule *> processingOrder = BuildProcessingOrder(rack);

        for (DynamicModule *module : processingOrder)
        {
            if (module == nullptr || !module->Active || module->Instance == nullptr)
            {
                continue;
            }

            std::vector<const float *> inputPins(module->InPins, nullptr);
            for (int pinIndex = 0; pinIndex < module->InPins; ++pinIndex)
            {
                inputPins[pinIndex] = FindLinkedOutputBuffer(rack, module->ID, pinIndex, outputBuffers);
            }

            std::vector<float *> outputPins(module->OutPins, nullptr);
            for (int pinIndex = 0; pinIndex < module->OutPins; ++pinIndex)
            {
                auto &pinBuffer = outputBuffers[{module->ID, pinIndex}];
                pinBuffer.assign(numSamples, 0.0f);
                outputPins[pinIndex] = pinBuffer.data();
            }

            const float *firstInput = inputPins.empty() ? nullptr : inputPins[0];
            CaptureScopeSamples(GlobaloduleScopeInputs, module->ID, firstInput, numSamples);

            MDU::BufferView bufferView;
            bufferView.InputPins.assign(inputPins.begin(), inputPins.end());
            bufferView.OutputPins.assign(outputPins.begin(), outputPins.end());
            bufferView.NumberOfSamples = static_cast<size_t>(numSamples);
            bufferView.SampleRate = 44100;
            bufferView.VoltageRange = rack.VoltageRange;

            if (module->Instance != nullptr)
            {
                module->Instance->Process(bufferView, 0.0f);
            }

            const float *firstOutput = outputPins.empty() ? nullptr : outputPins[0];
            CaptureScopeSamples(GlobalModuleScopeOutputs, module->ID, firstOutput, numSamples);
        }

        for (const auto &module : rack.DynamicModules)
        {
            if (!module.Active)
            {
                continue;
            }

            bool isOutputModule = (module.Metadata.ModuleType == "Output" || module.Metadata.ModuleName == "Output");
            if (!isOutputModule)
            {
                continue;
            }

            auto outIt = outputBuffers.find({module.ID, 0});
            if (outIt == outputBuffers.end())
            {
                continue;
            }

            const auto &outBuffer = outIt->second;
            // Debug: print sum of output buffer
            float sum = 0.0f;
            for (int i = 0; i < numSamples && i < static_cast<int>(outBuffer.size()); ++i)
            {
                sum += outBuffer[i];
                float mixed = buffer[i] + outBuffer[i];
                // Clamp to avoid hard clipping beyond normalized output range.
                if (mixed > 1.0f)
                    mixed = 1.0f;
                if (mixed < -1.0f)
                    mixed = -1.0f;
                buffer[i] = mixed;
            }
            if (sum != 0.0f) {
                Console::AppendConsoleLine("[debug] Output module " + std::to_string(module.ID) + " buffer sum: " + std::to_string(sum));
            } else {
                Console::AppendConsoleLine("[debug] Output module " + std::to_string(module.ID) + " buffer is silent");
            }
        }
    }
}

// Shut down audio resources cleanly.
void ShutdownAudioHandling()
{
    Audio::Close();
}

// --Tools--

// Create a new rack, assign an ID, and return it.
Rack *CreateRack(const std::string &name)
{
    Rack newRack;
    newRack.ID = NextRackID++;
    newRack.Name = name;
    newRack.Enabled = true;
    Racks.push_back(newRack);
    Console::AppendConsoleLine("Rack created: '" + newRack.Name + "' (ID #" + std::to_string(newRack.ID) + ")");
    return &Racks.back();
}

// Remove a rack and destroy all dynamic module instances inside it.
void DeleteRack(int rackID)
{
    for (auto it = Racks.begin(); it != Racks.end(); ++it)
    {
        if (it->ID != rackID)
        {
            continue;
        }

        for (auto &module : it->DynamicModules)
        {
            RemoveDynamicModuleFromRack(module);
        }

        SelectedRackIDs.remove(rackID);
        Racks.erase(it);
        return;
    }
}

// Instantiate a loaded MDU module and add it to the target rack.
// Returns false with an error message when any load or factory step fails.
bool AddDynamicModuleToRack(Rack &rack, const std::string &sourcePath, std::string *errorOut)
{
    const auto &loadedMap = GlobalModuleLoader.GetLoadedModules();
    auto it = loadedMap.find(sourcePath);
    if (it == loadedMap.end())
    {
        const std::string message = "Loaded module not found for path: " + sourcePath;
        if (errorOut)
        {
            *errorOut = message;
        }
        Console::AppendConsoleLine("[error] " + message);
        return false;
    }

    const auto &loaded = it->second;
    if (loaded.Create == nullptr || loaded.Destroy == nullptr)
    {
        const std::string message = "Factory functions missing for: " + sourcePath;
        if (errorOut)
        {
            *errorOut = message;
        }
        Console::AppendConsoleLine("[error] " + message);
        return false;
    }

    MDU::Module *instance = loaded.Create();
    if (instance == nullptr)
    {
        const std::string message = "Create returned null for: " + sourcePath;
        if (errorOut)
        {
            *errorOut = message;
        }
        Console::AppendConsoleLine("[error] " + message);
        return false;
    }

    DynamicModule dynamicModule;
    dynamicModule.ID = NextModuleID++;
    dynamicModule.SourcePath = sourcePath;
    dynamicModule.Metadata = loaded.Metadata;
    dynamicModule.Name = loaded.Metadata.ModuleName.empty() ? "MDU Module" : loaded.Metadata.ModuleName;
    dynamicModule.Instance = instance;
    dynamicModule.Destroy = loaded.Destroy;
    dynamicModule.InPins = static_cast<int>(loaded.Metadata.InputPins.size());
    dynamicModule.OutPins = static_cast<int>(loaded.Metadata.OutputPins.size());

    Console::AppendConsoleLine("Creating dynamic module: '" + dynamicModule.Name + "' (ID #" + std::to_string(dynamicModule.ID) + ")");
    rack.DynamicModules.push_back(dynamicModule);
    Console::AppendConsoleLine("Module '" + dynamicModule.Name + "' added to rack '" + rack.Name + "' (Rack #" + std::to_string(rack.ID) + ")");
    return true;
}

// Destroy the runtime instance behind a dynamic module and clear function pointers.
void RemoveDynamicModuleFromRack(DynamicModule &module)
{
    if (module.Instance != nullptr && module.Destroy != nullptr)
    {
        module.Destroy(module.Instance);
    }
    module.Instance = nullptr;
    module.Destroy = nullptr;
}

// Remove one module node from every rack and delete all links that reference it.
void RemoveNode(int nodeID)
{
    SelectedModuleIDs.remove(nodeID);
    if (SelectedModuleID == nodeID)
    {
        SelectedModuleID = -1;
    }

    for (auto &rack : Racks)
    {
        rack.DynamicModules.remove_if([nodeID](DynamicModule &module)
                                      {
                                          if (module.ID != nodeID)
                                          {
                                              return false;
                                          }

                                          RemoveDynamicModuleFromRack(module);
                                          return true; });

        rack.Links.erase(
            std::remove_if(rack.Links.begin(), rack.Links.end(),
                           [nodeID](const Link &link)
                           { return link.StartModuleID == nodeID || link.EndModuleID == nodeID; }),
            rack.Links.end());
    }
}

// Open the platform file manager at the provided folder (or nearest valid parent).
void LaunchDefaultFileManager(const std::filesystem::path &path)
{
    std::filesystem::path target = path;
    if (target.empty())
    {
        target = std::filesystem::current_path();
    }

    std::error_code statusError;
    if (!std::filesystem::is_directory(target, statusError) || statusError)
    {
        target = target.parent_path();
    }

    if (target.empty())
    {
        return;
    }

    std::string command;
#if defined(_WIN32)
    command = "explorer \"" + target.string() + "\"";
#elif defined(__APPLE__)
    command = "open \"" + target.string() + "\"";
#else
    command = "xdg-open \"" + target.string() + "\"";
#endif

    std::system(command.c_str());
}

// Build the default template path under ~/Documents/Signal/Modules when possible.
// Falls back to the current working directory if needed.
std::filesystem::path GetDefaultTemplatePath()
{
    const char *home = std::getenv("HOME");
    if (home != nullptr && *home != '\0')
    {
        std::filesystem::path documents = std::filesystem::path(home) / "Documents" / "Signal" / "Modules";
        std::error_code mkdirError;
        std::filesystem::create_directories(documents, mkdirError);

        std::error_code existsError;
        if (!std::filesystem::exists(documents, existsError) || existsError)
        {
            return std::filesystem::current_path() / "TemplateModule.mdu";
        }

        return documents / "TemplateModule.mdu";
    }

    return std::filesystem::current_path() / "TemplateModule.mdu";
}

// --mdu handling--

// Parse SIGNAL_MDU_PATHS from the environment and keep only existing absolute paths.
std::vector<std::string> BuildMduPathsFromEnvironment()
{
    const char *envValue = std::getenv("SIGNAL_MDU_PATHS");
    if (envValue == nullptr || *envValue == '\0')
    {
        return {};
    }

    std::vector<std::string> rawPaths;
    std::string current;
    for (char Character : std::string(envValue))
    {
        if (Character == ':' || Character == ';')
        {
            if (!current.empty())
            {
                rawPaths.push_back(current);
                current.clear();
            }
        }
        else
        {
            current.push_back(Character);
        }
    }
    if (!current.empty())
    {
        rawPaths.push_back(current);
    }

    std::vector<std::string> resolved;
    resolved.reserve(rawPaths.size());

    for (const auto &path : rawPaths)
    {
        if (path.empty())
        {
            continue;
        }

        std::error_code absError;
        const std::filesystem::path absolute = std::filesystem::absolute(path, absError);
        if (absError)
        {
            continue;
        }

        std::error_code existsError;
        if (std::filesystem::exists(absolute, existsError) && !existsError)
        {
            resolved.push_back(absolute.lexically_normal().string());
        }
    }

    return resolved;
}

// Build the final module search path list from settings, env vars, and sensible defaults.
// Persists auto-discovered paths when no explicit settings exist yet.
std::vector<std::string> BuildMduRuntimePaths()
{
    auto AppendUniquePaths = [](std::vector<std::string> &destination, const std::vector<std::string> &source)
    {
        for (const auto &path : source)
        {
            if (path.empty())
            {
                continue;
            }

            if (std::find(destination.begin(), destination.end(), path) == destination.end())
            {
                destination.push_back(path);
            }
        }
    };

    const auto settingsPaths = MDU::LoadMduSearchPathsFromSettingsFile();
    const auto environmentPaths = BuildMduPathsFromEnvironment();
    auto FirstExisting = [](const std::vector<std::filesystem::path> &candidates) -> std::vector<std::string>
    {
        // This returns a single preferred path as soon as one valid candidate exists.
        for (const auto &candidate : candidates)
        {
            std::error_code errorCode;
            const std::filesystem::path absolutePath = std::filesystem::absolute(candidate, errorCode);
            if (errorCode)
            {
                continue;
            }

            std::error_code existsError;
            if (!std::filesystem::exists(absolutePath, existsError) || existsError)
            {
                continue;
            }

            return {absolutePath.lexically_normal().string()};
        }

        return {};
    };

    std::vector<std::filesystem::path> bundledCandidates;
    std::vector<std::filesystem::path> sourceCandidates;

#if defined(__linux__) || defined(__unix__) || (defined(__APPLE__))
    {
        std::vector<char> executablePath(static_cast<size_t>(PATH_MAX) + 1u, '\0');
        ssize_t read = ::readlink("/proc/self/exe", executablePath.data(), executablePath.size() - 1u);
        if (read > 0)
        {
            executablePath[static_cast<size_t>(read)] = '\0';
            std::filesystem::path executableDir = std::filesystem::path(executablePath.data()).parent_path();
            bundledCandidates.push_back(executableDir / "modules");
            bundledCandidates.push_back(executableDir / "../share/Signal/modules");
        }
    }
#endif

#if defined(_WIN32)
    {
        std::vector<char> executablePath(static_cast<size_t>(MAX_PATH) + 1u, '\0');
        DWORD length = ::GetModuleFileNameA(nullptr, executablePath.data(), static_cast<DWORD>(executablePath.size()));
        if (length > 0 && length < executablePath.size())
        {
            executablePath[static_cast<size_t>(length)] = '\0';
            std::filesystem::path executableDir = std::filesystem::path(executablePath.data()).parent_path();
            bundledCandidates.push_back(executableDir / "modules");
            bundledCandidates.push_back(executableDir / ".." / "share" / "Signal" / "modules");
        }
    }
#endif

    sourceCandidates.push_back("src/Modules");
    sourceCandidates.push_back("modules");
    sourceCandidates.push_back("../src/Modules");
    sourceCandidates.push_back("../modules");

    std::vector<std::string> defaultPaths = FirstExisting(bundledCandidates);
    if (defaultPaths.empty())
    {
        defaultPaths = FirstExisting(sourceCandidates);
    }

    std::vector<std::string> runtimePaths;

    if (!settingsPaths.empty())
    {
        runtimePaths = settingsPaths;
    }
    else if (!environmentPaths.empty())
    {
        runtimePaths = environmentPaths;
        MDU::SaveMduSearchPathsToSettingsFile(environmentPaths);
    }

    AppendUniquePaths(runtimePaths, defaultPaths);

    if (runtimePaths.empty())
    {
        runtimePaths = defaultPaths;
    }

    if (settingsPaths.empty() && environmentPaths.empty() && !runtimePaths.empty())
    {
        MDU::SaveMduSearchPathsToSettingsFile(runtimePaths);
    }

    return runtimePaths;
}

// Add one MDU search path to loader/settings if it is valid and not already present.
void AddMduSearchPathAndPersist(const std::string &path)
{
    const std::vector<std::string> normalizedNewPath = MDU::NormalizeAndUniquePaths({path});
    if (normalizedNewPath.empty())
    {
        Console::AppendConsoleLine("[warning] Cannot add empty path to MDU search paths.");
        return;
    }

    const std::string &newPath = normalizedNewPath.front();
    std::vector<std::string> currentPaths = GlobalModuleLoader.GetSearchPaths();

    if (std::find(currentPaths.begin(), currentPaths.end(), newPath) == currentPaths.end())
    {
        GlobalModuleLoader.AddSearchPath(newPath);
        GlobalFileWatcher.AddWatchPath(newPath);
        currentPaths.push_back(newPath);
    }

    MDU::SaveMduSearchPathsToSettingsFile(currentPaths);
}

// Remove every rack link connected to a module ID.
void RemoveLinksForModule(Rack &rack, int moduleID)
{
    rack.Links.erase(
        std::remove_if(rack.Links.begin(), rack.Links.end(),
                       [moduleID](const Link &link)
                       { return link.StartModuleID == moduleID || link.EndModuleID == moduleID; }),
        rack.Links.end());
}

// Unload all module instances created from a specific source path across all racks.
void RemoveDynamicModulesFromAllRacksBySourcePath(const std::string &sourcePath)
{
    for (auto &rack : Racks)
    {
        rack.DynamicModules.remove_if([&rack, &sourcePath](DynamicModule &module)
                                      {
                                          if (module.SourcePath != sourcePath)
                                          {
                                              return false;
                                          }

                                          int removedModuleID = module.ID;
                                          RemoveDynamicModuleFromRack(module);
                                          RemoveLinksForModule(rack, removedModuleID);
                                          return true; });
    }
}

// Poll watched MDU files and hot-reload modules as files are added, changed, or removed.
// Performs one-time watcher/loader initialization on first call.
void ProcessMduFileChanges()
{
    if (!GlobalFileWatcherInitialized)
    {
        // One-time startup path wiring and initial module scan.
        const auto runtimePaths = BuildMduRuntimePaths();
        GlobalFileWatcher.SetWatchPaths(runtimePaths);
        GlobalModuleLoader.SetSearchPaths(runtimePaths);

        std::string loadAllError;
        if (!GlobalModuleLoader.ScanAndLoadAll(&loadAllError) && !loadAllError.empty())
        {
            GlobalLastMduError = loadAllError;
            std::cerr << loadAllError << std::endl;
            Console::AppendConsoleLine("[error] " + loadAllError);
        }
        else
        {
            GlobalLastMduError.clear();
        }

        GlobalFileWatcher.PrimeSnapshot();
        GlobalFileWatcherInitialized = true;
    }

    for (const auto &change : GlobalFileWatcher.PollChanges())
    {
        if (change.Type == MDU::FileChangeType::Added || change.Type == MDU::FileChangeType::Modified)
        {
            if (change.Type == MDU::FileChangeType::Modified)
            {
                RemoveDynamicModulesFromAllRacksBySourcePath(change.Path);
            }

            std::string error;
            GlobalModuleLoader.LoadFromMduFile(change.Path, &error);
            if (!error.empty())
            {
                GlobalLastMduError = error;
                std::cerr << error << std::endl;
                Console::AppendConsoleLine("[error] " + error + " (loading " + change.Path + ")");
            }
            else
            {
                GlobalLastMduError.clear();
            }
        }
        else if (change.Type == MDU::FileChangeType::Removed)
        {
            RemoveDynamicModulesFromAllRacksBySourcePath(change.Path);

            std::string error;
            GlobalModuleLoader.UnloadByPath(change.Path, &error);
            if (!error.empty())
            {
                GlobalLastMduError = error;
                std::cerr << error << std::endl;
                Console::AppendConsoleLine("[error] " + error + " (unloading " + change.Path + ")");
            }
            else
            {
                GlobalLastMduError.clear();
            }
        }
    }
}
