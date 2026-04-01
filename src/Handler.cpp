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

int NextRackID = 1;
int NextModuleID = 1;
int NextLinkID = 1;
int SelectedModuleID = -1;
int SelectedRackID = -1;

std::list<Rack> Racks;

MDU::ModuleLoader GModuleLoader;
static MDU::FileWatcher GFileWatcher;

std::mutex GRackMutex;

std::string GLastMduError;

std::map<int, std::vector<float>> GModuleScopeInputs;
std::map<int, std::vector<float>> GModuleScopeOutputs;

static bool GFileWatcherInitialized = false;
bool GShowDebugConsole = false;
bool IsRecording = false;
// boolording = false;

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

static constexpr int kScopeSampleCount = 512;

void CaptureScopeSamples(std::map<int, std::vector<float>> &scopeMap,
                         int moduleID,
                         const float *source,
                         int numSamples)
{
    std::vector<float> &scope = scopeMap[moduleID];
    if (scope.size() != static_cast<size_t>(kScopeSampleCount))
    {
        scope.assign(kScopeSampleCount, 0.0f);
    }

    if (source == nullptr || numSamples <= 0)
    {
        std::fill(scope.begin(), scope.end(), 0.0f);
        return;
    }

    int copyCount = std::min(kScopeSampleCount, numSamples);
    std::copy(source, source + copyCount, scope.begin());
    if (copyCount < kScopeSampleCount)
    {
        std::fill(scope.begin() + copyCount, scope.end(), 0.0f);
    }
}


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

int main()
{
    GModuleLoader.SetTemplatePath(GetDefaultTemplatePath().string());
    Draw::MainWindow();

    while (!Window::ShouldClose())
    {
        ImGuiUtil::Begin();

        std::lock_guard<std::mutex> rackLock(GRackMutex);

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
                SelectedRackID = rack.ID;
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

        if (SelectedModuleID != -1)
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

void SetupAudioHandling()
{
    Audio::Init();
    Audio::SetFilterCallback(AudioFilterCallback, nullptr);
}

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

void AudioFilterCallback(float *buffer, int numSamples, void *userData)
{
    (void)userData;

    std::lock_guard<std::mutex> rackLock(GRackMutex);

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
            CaptureScopeSamples(GModuleScopeInputs, module->ID, firstInput, numSamples);

            MDU::BufferView bufferView;
            bufferView.InputPins.assign(inputPins.begin(), inputPins.end());
            bufferView.OutputPins.assign(outputPins.begin(), outputPins.end());
            bufferView.NumberOfSamples = static_cast<size_t>(numSamples);
            bufferView.SampleRate = 44100;

            if (module->Instance != nullptr)
            {
                module->Instance->Process(bufferView, 0.0f);
            }

            const float *firstOutput = outputPins.empty() ? nullptr : outputPins[0];
            CaptureScopeSamples(GModuleScopeOutputs, module->ID, firstOutput, numSamples);
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

void ShutdownAudioHandling()
{
    Audio::Close();
}

// --Tools--

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

        Racks.erase(it);
        return;
    }
}

bool AddDynamicModuleToRack(Rack &rack, const std::string &sourcePath, std::string *errorOut)
{
    const auto &loadedMap = GModuleLoader.GetLoadedModules();
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

void RemoveDynamicModuleFromRack(DynamicModule &module)
{
    if (module.Instance != nullptr && module.Destroy != nullptr)
    {
        module.Destroy(module.Instance);
    }
    module.Instance = nullptr;
    module.Destroy = nullptr;
}

void RemoveNode(int nodeID)
{
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

std::vector<std::string> BuildMduRuntimePaths()
{
    const auto environmentPaths = BuildMduPathsFromEnvironment();
    if (!environmentPaths.empty())
    {
        return environmentPaths;
    }

    auto FirstExisting = [](const std::vector<std::filesystem::path> &candidates) -> std::vector<std::string>
    {
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

    const auto bundledPaths = FirstExisting(bundledCandidates);
    if (!bundledPaths.empty())
    {
        return bundledPaths;
    }

    return FirstExisting(sourceCandidates);
}

void RemoveLinksForModule(Rack &rack, int moduleID)
{
    rack.Links.erase(
        std::remove_if(rack.Links.begin(), rack.Links.end(),
                       [moduleID](const Link &link)
                       { return link.StartModuleID == moduleID || link.EndModuleID == moduleID; }),
        rack.Links.end());
}

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

void ProcessMduFileChanges()
{
    if (!GFileWatcherInitialized)
    {
        const auto runtimePaths = BuildMduRuntimePaths();
        GFileWatcher.SetWatchPaths(runtimePaths);
        GModuleLoader.SetSearchPaths(runtimePaths);

        std::string loadAllError;
        if (!GModuleLoader.ScanAndLoadAll(&loadAllError) && !loadAllError.empty())
        {
            GLastMduError = loadAllError;
            std::cerr << loadAllError << std::endl;
            Console::AppendConsoleLine("[error] " + loadAllError);
        }
        else
        {
            GLastMduError.clear();
        }

        GFileWatcher.PrimeSnapshot();
        GFileWatcherInitialized = true;
    }

    for (const auto &change : GFileWatcher.PollChanges())
    {
        if (change.Type == MDU::FileChangeType::Added || change.Type == MDU::FileChangeType::Modified)
        {
            if (change.Type == MDU::FileChangeType::Modified)
            {
                RemoveDynamicModulesFromAllRacksBySourcePath(change.Path);
            }

            std::string error;
            GModuleLoader.LoadFromMduFile(change.Path, &error);
            if (!error.empty())
            {
                GLastMduError = error;
                std::cerr << error << std::endl;
                Console::AppendConsoleLine("[error] " + error + " (loading " + change.Path + ")");
            }
            else
            {
                GLastMduError.clear();
            }
        }
        else if (change.Type == MDU::FileChangeType::Removed)
        {
            RemoveDynamicModulesFromAllRacksBySourcePath(change.Path);

            std::string error;
            GModuleLoader.UnloadByPath(change.Path, &error);
            if (!error.empty())
            {
                GLastMduError = error;
                std::cerr << error << std::endl;
                Console::AppendConsoleLine("[error] " + error + " (unloading " + change.Path + ")");
            }
            else
            {
                GLastMduError.clear();
            }
        }
    }
}
