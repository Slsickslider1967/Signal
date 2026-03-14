#include <algorithm>
#include <chrono>
#include <cstdio>
#include <iostream>
#include <list>
#include <map>
#include <string>
#include <thread>
#include <vector>

#include "imgui.h"
#include "imgui_stdlib.h"
#include "imnodes.h"

#include "../include/Functions/Audio.h"
#include "../include/Functions/ImGuiUtil.h"
#include "../include/Functions/Module.h"
#include "../include/Functions/Window.h"
#include "../include/LowFrequencyOscillator.h"
#include "../include/MDU/FileWatcher.h"
#include "../include/MDU/ModuleLoader.h"
#include "../include/MDU/mduParser.h"
#include "../include/ModuleEditor.h"
#include "../include/Output.h"
#include "../include/Voltage-ControlledAmplifier.h"
#include "../include/VoltageControllFilter.h"
#include "../include/WaveForm.h"

struct Rack
{
    int ID;
    std::string Name;
    std::list<Module> Modules;
    std::list<DynamicModule> DynamicModules;
    std::vector<Link> Links;
    bool Enabled = true;
};

static int NextRackID = 1;
static int NextModuleID = 1;
static int NextLinkID = 1;
std::list<Rack> Racks;
static int SelectedModuleID = -1;
static int SelectedRackID = -1;

static MDU::ModuleLoader GModuleLoader;
static MDU::FileWatcher GFileWatcher;
static bool GFileWatcherInitialized = false;

namespace SpeedManipulation
{
    void MainImgui();
}

void MainWindow();
void CleanUp();

void ImGuiFuncGen();
void Render();

void DrawRackEditor(Rack &rack);
void CreateLinks(Rack &rack);
void DrawLinks(Rack &rack);
void AddPins(const Module &module);
void AddPins(const DynamicModule &module);
void AddPinsWithLabels(int moduleID, int inputPins, int outputPins,
                       const std::vector<std::string> *inputLabels = nullptr,
                       const std::vector<std::string> *outputLabels = nullptr);
int MakeInputAttributeID(int moduleID, int pinIndex);
int MakeOutputAttributeID(int moduleID, int pinIndex);

void ChildNodeWindow();
void DrawModuleDetails();

void AudioFilterCallback(float *buffer, int numSamples, void *userData);
void SetupAudioHandling();
void ShutdownAudioHandling();
void UpdateAudioWaveFormsFromRacks();

Rack *CreateRack(const std::string &name);
void DeleteRack(int rackID);
void AddModuleToRack(Rack &rack, ModuleType type, const std::string &name);
bool AddDynamicModuleToRack(Rack &rack, const std::string &sourcePath, std::string *errorOut);
void RemoveModuleFromRack(Rack &rack, int moduleID);
void RemoveDynamicModuleFromRack(DynamicModule &module);
void RemoveNode(int nodeID);
const char *ModuleTypeToString(ModuleType type);
const char *FilterTypeToString(FilterType type);
void AddRackTool();
bool PopUpTool(Rack &rack);
void DrawAvailableModulesChild(Rack &rack);
Rack *FindRackByID(int rackID);

void RemoveLinksForModule(Rack &rack, int moduleID);
void RemoveDynamicModulesFromAllRacksBySourcePath(const std::string &sourcePath);
void ProcessMduFileChanges();

int main()
{
    MainWindow();

    while (!Window::ShouldClose())
    {
        ImGuiUtil::Begin();

        bool showDemoWindow = false;
        if (showDemoWindow)
            ImGui::ShowDemoWindow(&showDemoWindow);

        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("Rack"))
            {
                if (ImGui::MenuItem("Add Rack"))
                {
                    CreateRack(std::string("Rack ") + std::to_string(NextRackID));
                }
                if (ImGui::BeginMenu("Remove Rack"))
                {
                    if (Racks.empty())
                    {
                        ImGui::TextDisabled("No racks to remove");
                    }
                    else
                    {
                        int rackIDToDelete = -1;
                        for (const auto &rack : Racks)
                        {
                            std::string label = "Rack #" + std::to_string(rack.ID) + ": " + rack.Name;
                            if (ImGui::MenuItem(label.c_str()))
                                rackIDToDelete = rack.ID;
                        }
                        if (rackIDToDelete != -1)
                            DeleteRack(rackIDToDelete);
                    }
                    ImGui::EndMenu();
                }
                ImGui::Separator();

                if (ImGui::BeginMenu("Save Rack"))
                {
                    ImGui::Text("Save functionality not implemented yet");
                    for (const auto &rack : Racks)
                    {
                        std::string label = "Save " + rack.Name;
                        (void)label;
                    }
                    ImGui::EndMenu();
                }

                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Modules"))
            {
                if (ImGui::BeginMenu("Add Module to Selected Rack"))
                {
                    Rack *selectedRack = FindRackByID(SelectedRackID);
                    if (selectedRack == nullptr)
                    {
                        ImGui::TextDisabled("Select a Rack First");
                    }
                    else
                    {
                        if (ImGui::MenuItem("VCO"))
                            AddModuleToRack(*selectedRack, MODULE_VCO, "VCO-1");
                        if (ImGui::MenuItem("LFO"))
                            AddModuleToRack(*selectedRack, MODULE_LFO, "LFO-1");
                        if (ImGui::MenuItem("VCF"))
                            AddModuleToRack(*selectedRack, MODULE_VCF, "VCF-1");
                        if (ImGui::MenuItem("VCA"))
                            AddModuleToRack(*selectedRack, MODULE_VCA, "VCA-1");
                        if (ImGui::MenuItem("Output"))
                            AddModuleToRack(*selectedRack, MODULE_OUTPUT, "Output");

                        if (ImGui::BeginMenu("MDU Loaded Modules"))
                        {
                            const auto &loadedModules = GModuleLoader.GetLoadedModules();
                            if (loadedModules.empty())
                            {
                                ImGui::TextDisabled("No loaded .mdu modules");
                            }
                            else
                            {
                                for (const auto &entry : loadedModules)
                                {
                                    const std::string &sourcePath = entry.first;
                                    const auto &loadedModule = entry.second;

                                    std::string label = loadedModule.Metadata.ModuleName.empty()
                                                            ? sourcePath
                                                            : loadedModule.Metadata.ModuleName;

                                    if (ImGui::MenuItem(label.c_str()))
                                    {
                                        std::string error;
                                        AddDynamicModuleToRack(*selectedRack, sourcePath, &error);
                                        if (!error.empty())
                                        {
                                            std::cerr << error << std::endl;
                                        }
                                    }
                                }
                            }

                            ImGui::EndMenu();
                        }
                    }

                    ImGui::EndMenu();
                }

                ImGui::Separator();

                if (ImGui::MenuItem("Import Module From File"))
                {
                    ImGui::Text("Import functionality not implemented yet");
                }
                if (ImGui::MenuItem("Select Module File Directory"))
                {
                    ImGui::Text("Directory selection not implemented yet");
                }

                ImGui::EndMenu();
            }

            ImGui::EndMainMenuBar();
        }

        ImGuiViewport *mainViewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(mainViewport->WorkPos);
        ImGui::SetNextWindowSize(mainViewport->WorkSize);
        ImGui::SetNextWindowViewport(mainViewport->ID);

        ImGuiWindowFlags rackManagerFlags =
            ImGuiWindowFlags_NoDocking |
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

            ImGui::SameLine();
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 4.0f);
            bool shouldDelete = false;

            if (ImGui::Button("Del##rack", ImVec2(50, 0)))
            {
                shouldDelete = true;
            }

            if (shouldDelete)
            {
                if (rackOpen)
                    ImGui::TreePop();
                int rackIDToDelete = rack.ID;
                auto nextRackIt = std::next(rackIt);
                DeleteRack(rackIDToDelete);
                rackIt = nextRackIt;
                ImGui::PopID();
                continue;
            }

            if (rackOpen)
            {
                ImGui::Text("Signal Chain - Node Editor:");

                DrawRackEditor(rack);
                bool deleteRequested = PopUpTool(rack);

                if (deleteRequested)
                {
                    ImGui::TreePop();
                    int rackIDToDelete = rack.ID;
                    auto nextRackIt = std::next(rackIt);
                    DeleteRack(rackIDToDelete);
                    rackIt = nextRackIt;
                    ImGui::PopID();
                    continue;
                }

                ImGui::Unindent();
                ImGui::TreePop();
            }

            ImGui::PopID();
            ++rackIt;
        }

        ImGui::End();

        if (SelectedModuleID != -1)
        {
            DrawModuleDetails();
        }

        ImGuiUtil::End();

        Render();

        UpdateAudioWaveFormsFromRacks();
        ProcessMduFileChanges();

        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    CleanUp();
    return 0;
}

// --Draw--

void MainWindow()
{
    setenv("PREFER_X11", "1", 1);

    Window::CreateWindow(1280, 720, "Signal Handler");
    SetupAudioHandling();

    Window::PollEvents();
}

void CleanUp()
{
    Window::DestroyWindow();
    ShutdownAudioHandling();
}

void Render()
{
    Window::ClearColor(0.08f, 0.08f, 0.10f, 1.0f);
    ImGuiUtil::Render();
    Window::SwapBuffers();
    Window::PollEvents();
}

int MakeInputAttributeID(int moduleID, int pinIndex)
{
    return moduleID * 1000 + 100 + pinIndex;
}

int MakeOutputAttributeID(int moduleID, int pinIndex)
{
    return moduleID * 1000 + pinIndex;
}

void AddPinsWithLabels(int moduleID, int inputPins, int outputPins,
                       const std::vector<std::string> *inputLabels,
                       const std::vector<std::string> *outputLabels)
{
    for (int i = 0; i < inputPins; i++)
    {
        const char *label = "In";
        if (inputLabels != nullptr && i < static_cast<int>(inputLabels->size()) && !(*inputLabels)[i].empty())
        {
            label = (*inputLabels)[i].c_str();
        }

        ImNodes::BeginInputAttribute(MakeInputAttributeID(moduleID, i));
        ImGui::Text("%s", label);
        ImNodes::EndInputAttribute();
    }

    for (int i = 0; i < outputPins; i++)
    {
        const char *label = "Out";
        if (outputLabels != nullptr && i < static_cast<int>(outputLabels->size()) && !(*outputLabels)[i].empty())
        {
            label = (*outputLabels)[i].c_str();
        }

        ImNodes::BeginOutputAttribute(MakeOutputAttributeID(moduleID, i));
        ImGui::Text("%s", label);
        ImNodes::EndOutputAttribute();
    }
}

void DrawRackEditor(Rack &rack)
{
    ImNodes::BeginNodeEditor();
    if (ImNodes::IsEditorHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        SelectedRackID = rack.ID;
        SelectedModuleID = -1;
    }
    bool openContextMenu = ImNodes::IsEditorHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right);

    for (auto moduleIt = rack.Modules.begin(); moduleIt != rack.Modules.end(); ++moduleIt)
    {
        Module &module = *moduleIt;

        ImNodes::PushAttributeFlag(ImNodesAttributeFlags_EnableLinkDetachWithDragClick);
        ImNodes::BeginNode(module.ID);

        AddPins(module);

        ImGui::Text("%s: %s", ModuleTypeToString(module.Type), module.Name.c_str());

        if (module.Type == MODULE_VCF)
        {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "[%s]", FilterTypeToString(module.vcfConfig.filterType));
        }

        ImNodes::EndNode();
        ImNodes::PopAttributeFlag();

        if (ImNodes::IsNodeSelected(module.ID))
        {
            SelectedModuleID = module.ID;
        }
    }

    for (auto dynamicModuleIt = rack.DynamicModules.begin(); dynamicModuleIt != rack.DynamicModules.end(); ++dynamicModuleIt)
    {
        DynamicModule &dynamicModule = *dynamicModuleIt;

        ImNodes::PushAttributeFlag(ImNodesAttributeFlags_EnableLinkDetachWithDragClick);
        ImNodes::BeginNode(dynamicModule.ID);

        AddPins(dynamicModule);

        ImGui::Text("MDU: %s", dynamicModule.Name.c_str());
        if (!dynamicModule.Metadata.Author.empty())
        {
            ImGui::TextDisabled("by %s", dynamicModule.Metadata.Author.c_str());
        }

        ImNodes::EndNode();
        ImNodes::PopAttributeFlag();

        if (ImNodes::IsNodeSelected(dynamicModule.ID))
        {
            SelectedModuleID = dynamicModule.ID;
        }
    }

    DrawLinks(rack);
    ImNodes::EndNodeEditor();

    if (openContextMenu)
        ImGui::OpenPopup("RackContextMenu");

    CreateLinks(rack);

    int deletedLinkID;
    if (ImNodes::IsLinkDestroyed(&deletedLinkID))
    {
        rack.Links.erase(
            std::remove_if(rack.Links.begin(), rack.Links.end(),
                           [deletedLinkID](const Link &link)
                           { return link.ID == deletedLinkID; }),
            rack.Links.end());
    }
}

void CreateLinks(Rack &rack)
{
    int startAttr;
    int endAttr;
    if (ImNodes::IsLinkCreated(&startAttr, &endAttr))
    {
        int startModuleID = startAttr / 1000;
        int endModuleID = endAttr / 1000;

        int startPin = startAttr % 1000;
        int endPin = endAttr % 1000;

        bool isStartOutput = (startPin < 100);
        bool isEndInput = (endPin >= 100);

        if (isStartOutput && isEndInput)
        {
            bool linkExists = false;
            for (const auto &link : rack.Links)
            {
                if (link.StartModuleID == startModuleID && link.EndModuleID == endModuleID)
                {
                    linkExists = true;
                    break;
                }
            }

            if (!linkExists)
            {
                Link newLink;
                newLink.ID = NextLinkID++;
                newLink.StartModuleID = startModuleID;
                newLink.EndModuleID = endModuleID;
                newLink.StartPinIndex = startPin;
                newLink.EndPinIndex = endPin - 100;
                rack.Links.push_back(newLink);
            }
        }
    }
}

void DrawLinks(Rack &rack)
{
    for (const auto &link : rack.Links)
    {
        ImNodes::Link(link.ID, MakeOutputAttributeID(link.StartModuleID, link.StartPinIndex),
                      MakeInputAttributeID(link.EndModuleID, link.EndPinIndex));
    }
}

void AddPins(const Module &module)
{
    AddPinsWithLabels(module.ID, module.InPins, module.OutPins);
}

void AddPins(const DynamicModule &module)
{
    std::vector<std::string> inputLabels;
    inputLabels.reserve(module.Metadata.InputPins.size());
    for (const auto &pin : module.Metadata.InputPins)
    {
        inputLabels.push_back(pin.label.empty() ? pin.ID : pin.label);
    }

    std::vector<std::string> outputLabels;
    outputLabels.reserve(module.Metadata.OutputPins.size());
    for (const auto &pin : module.Metadata.OutputPins)
    {
        outputLabels.push_back(pin.label.empty() ? pin.ID : pin.label);
    }

    AddPinsWithLabels(module.ID, module.InPins, module.OutPins, &inputLabels, &outputLabels);
}

void ChildNodeWindow()
{
    ImGui::BeginChild("ChildNodeWindow", ImVec2(0, 200), true);
    ImGui::Text("This is a child window inside the node editor.");
    ImGui::EndChild();
}

void DrawModuleDetails()
{
    Module *selectedModule = nullptr;
    for (auto &rack : Racks)
    {
        for (auto &module : rack.Modules)
        {
            if (module.ID == SelectedModuleID)
            {
                selectedModule = &module;
                break;
            }
        }
        if (selectedModule)
            break;
    }

    if (selectedModule)
    {
        bool windowOpen = true;
        bool requestRemove = false;
        bool drewWindow = DrawModuleEditorWindow(*selectedModule, windowOpen, requestRemove);

        if (requestRemove)
        {
            RemoveNode(selectedModule->ID);
            SelectedModuleID = -1;
            return;
        }

        if (!windowOpen)
        {
            SelectedModuleID = -1;
            return;
        }

        if (!drewWindow)
        {
            SelectedModuleID = -1;
        }
    }
    else
    {
        SelectedModuleID = -1;
    }
}

// --Audio Handling--

void SetupAudioHandling()
{
    Audio::Init();
    Audio::SetFilterCallback(AudioFilterCallback, nullptr);
}

void UpdateAudioWaveFormsFromRacks()
{
    std::vector<WaveForm> activeWaves;
    for (auto &rack : Racks)
    {
        if (!rack.Enabled)
            continue;

        int outputModuleID = -1;
        bool hasOutput = false;
        for (const auto &module : rack.Modules)
        {
            if (module.Type == MODULE_OUTPUT && module.Active)
            {
                hasOutput = true;
                outputModuleID = module.ID;
                break;
            }
        }

        if (!hasOutput)
            continue;

        bool outputIsLinked = false;
        for (const auto &link : rack.Links)
        {
            if (link.EndModuleID == outputModuleID)
            {
                outputIsLinked = true;
                break;
            }
        }

        if (!outputIsLinked)
            continue;

        const int numSamplesForCV = 1;
        auto lfoOutputs = WaveFormGen::GenerateLFOOutputs(rack.Modules, numSamplesForCV);
        auto normalizedCVInputs = WaveFormGen::BuildNormalizedCVInputs(rack.Modules, rack.Links, lfoOutputs);

        for (auto &module : rack.Modules)
        {
            if (module.Type == MODULE_VCO && module.Active && module.vcoConfig.waveform.Enabled)
            {
                WaveForm wave = module.vcoConfig.waveform;

                auto cvIt = normalizedCVInputs.find(module.ID);
                if (cvIt != normalizedCVInputs.end() && !cvIt->second.empty())
                {
                    float cvValue = cvIt->second[0];

                    wave.vOctCV = cvValue;

                    if (wave.fmDepth > 0.0f)
                    {
                        wave.linearFMCV = cvValue;
                    }
                }

                activeWaves.push_back(wave);
            }
        }
    }

    Audio::SetWaveForms(activeWaves);
}

void AudioFilterCallback(float *buffer, int numSamples, void *userData)
{
    (void)userData;

    for (auto &rack : Racks)
    {
        if (!rack.Enabled)
            continue;

        int outputModuleID = -1;
        bool hasOutput = false;
        for (const auto &module : rack.Modules)
        {
            if (module.Type == MODULE_OUTPUT && module.Active)
            {
                hasOutput = true;
                outputModuleID = module.ID;
                break;
            }
        }

        if (!hasOutput)
            continue;

        bool outputIsLinked = false;
        for (const auto &link : rack.Links)
        {
            if (link.EndModuleID == outputModuleID)
            {
                outputIsLinked = true;
                break;
            }
        }

        if (!outputIsLinked)
            continue;

        std::vector<float> tempBuffer(numSamples);
        float *currentInput = buffer;
        float *currentOutput = tempBuffer.data();
        bool needsSwap = false;

        auto lfoOutputs = WaveFormGen::GenerateLFOOutputs(rack.Modules, numSamples);
        auto normalizedCVInputs = WaveFormGen::BuildNormalizedCVInputs(rack.Modules, rack.Links, lfoOutputs);

        for (auto &module : rack.Modules)
        {
            if (!module.Active)
                continue;

            switch (module.Type)
            {
            case MODULE_VCO:
                break;

            case MODULE_LFO:
                break;

            case MODULE_VCF:
                VCF::ProcessAudio(currentInput, currentOutput, numSamples, static_cast<int>(module.vcfConfig.filterType));
                std::swap(currentInput, currentOutput);
                needsSwap = !needsSwap;
                break;

            case MODULE_VCA:
            {
                const float *externalCVBuffer = nullptr;
                int externalCVBufferSize = 0;

                auto cvIt = normalizedCVInputs.find(module.ID);
                if (cvIt != normalizedCVInputs.end())
                {
                    externalCVBuffer = cvIt->second.data();
                    externalCVBufferSize = static_cast<int>(cvIt->second.size());
                }

                VCA::SetCVInput(module.vcaConfig.cvInput);
                VCA::ProcessAudioWithCVBuffer(currentInput, currentOutput, numSamples, externalCVBuffer, externalCVBufferSize);
            }
                std::swap(currentInput, currentOutput);
                needsSwap = !needsSwap;
                break;

            case MODULE_OUTPUT:
                Output::ProcessAudio(currentInput, numSamples);
                break;
            }
        }

        if (needsSwap)
        {
            std::copy(currentInput, currentInput + numSamples, buffer);
        }

        break;
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

        for (auto &dynamicModule : it->DynamicModules)
        {
            RemoveDynamicModuleFromRack(dynamicModule);
        }

        Racks.erase(it);
        return;
    }
}

void AddModuleToRack(Rack &rack, ModuleType type, const std::string &name)
{
    Module module(type);
    module.ID = NextModuleID++;
    module.Name = name;
    module.Active = true;

    if (type == MODULE_VCO)
    {
        WaveFormGen::InitializeVCOWaveForm(module.vcoConfig.waveform, module.ID);
    }
    else if (type == MODULE_LFO)
    {
        LFO::InitializeLFOWaveForm(module.lfoConfig.waveform, module.ID);
    }

    rack.Modules.push_back(module);
}

bool AddDynamicModuleToRack(Rack &rack, const std::string &sourcePath, std::string *errorOut)
{
    const auto &loadedMap = GModuleLoader.GetLoadedModules();
    auto it = loadedMap.find(sourcePath);
    if (it == loadedMap.end())
    {
        if (errorOut)
            *errorOut = "Loaded module not found for path: " + sourcePath;
        return false;
    }

    const auto &loaded = it->second;
    if (loaded.Create == nullptr || loaded.Destroy == nullptr)
    {
        if (errorOut)
            *errorOut = "Factory functions missing for: " + sourcePath;
        return false;
    }

    MDU::Module *instance = loaded.Create();
    if (instance == nullptr)
    {
        if (errorOut)
            *errorOut = "Create returned null for: " + sourcePath;
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

    rack.DynamicModules.push_back(dynamicModule);
    return true;
}

void RemoveModuleFromRack(Rack &rack, int moduleID)
{
    rack.Modules.remove_if([moduleID](const Module &module)
                           { return module.ID == moduleID; });
}

void RemoveDynamicModuleFromRack(DynamicModule &module)
{
    if (module.Instance && module.Destroy)
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
        RemoveModuleFromRack(rack, nodeID);

        rack.DynamicModules.remove_if([nodeID](DynamicModule &module)
                                      {
                                          if (module.ID != nodeID)
                                          {
                                              return false;
                                          }

                                          RemoveDynamicModuleFromRack(module);
                                          return true;
                                      });

        rack.Links.erase(
            std::remove_if(rack.Links.begin(), rack.Links.end(),
                           [nodeID](const Link &link)
                           { return link.StartModuleID == nodeID || link.EndModuleID == nodeID; }),
            rack.Links.end());
    }
}

const char *ModuleTypeToString(ModuleType type)
{
    switch (type)
    {
    case MODULE_VCO:
        return "VCO";
    case MODULE_LFO:
        return "LFO";
    case MODULE_VCF:
        return "VCF";
    case MODULE_VCA:
        return "VCA";
    case MODULE_OUTPUT:
        return "Output";
    default:
        return "Unknown";
    }
}

const char *FilterTypeToString(FilterType type)
{
    switch (type)
    {
    case FILTER_LowPass:
        return "Low-Pass";
    case FILTER_HighPass:
        return "High-Pass";
    case FILTER_BandPass:
        return "Band-Pass";
    case FILTER_NOTCH:
        return "Notch";
    default:
        return "Unknown";
    }
}

void AddRackTool()
{
    CreateRack(std::string("Rack ") + std::to_string(NextRackID));
}

bool PopUpTool(Rack &rack)
{
    bool requestDelete = false;
    if (ImGui::BeginPopupModal("RackContextMenu"))
    {
        if (ImGui::InputText("Rack Name", &rack.Name, ImGuiInputTextFlags_EnterReturnsTrue))
        {
            ImGui::CloseCurrentPopup();
        }

        ImGui::Separator();

        ImGui::Text("Available Modules");
        DrawAvailableModulesChild(rack);

        ImGui::Separator();

        if (ImGui::Selectable("Remove Rack"))
        {
            requestDelete = true;
            ImGui::CloseCurrentPopup();
        }

        if (ImGui::IsKeyPressed(ImGuiKey_Escape))
        {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
    return requestDelete;
}

void DrawAvailableModulesChild(Rack &rack)
{
    if (!ImGui::BeginChild("AvailableModulesChild", ImVec2(0.0f, 220.0f), true))
    {
        ImGui::EndChild();
        return;
    }

    if (ImGui::Selectable("VCO", false, ImGuiSelectableFlags_DontClosePopups))
        AddModuleToRack(rack, MODULE_VCO, "VCO-1");
    if (ImGui::Selectable("LFO", false, ImGuiSelectableFlags_DontClosePopups))
        AddModuleToRack(rack, MODULE_LFO, "LFO-1");
    if (ImGui::Selectable("VCF", false, ImGuiSelectableFlags_DontClosePopups))
        AddModuleToRack(rack, MODULE_VCF, "VCF-1");
    if (ImGui::Selectable("VCA", false, ImGuiSelectableFlags_DontClosePopups))
        AddModuleToRack(rack, MODULE_VCA, "VCA-1");
    if (ImGui::Selectable("Output", false, ImGuiSelectableFlags_DontClosePopups))
        AddModuleToRack(rack, MODULE_OUTPUT, "Output-1");

    const auto &loadedModules = GModuleLoader.GetLoadedModules();
    for (const auto &entry : loadedModules)
    {
        const std::string &sourcePath = entry.first;
        const auto &loadedModule = entry.second;

        std::string label = loadedModule.Metadata.ModuleName.empty()
                                ? sourcePath
                                : loadedModule.Metadata.ModuleName;

        if (ImGui::Selectable(label.c_str(), false, ImGuiSelectableFlags_DontClosePopups))
        {
            std::string error;
            AddDynamicModuleToRack(rack, sourcePath, &error);
            if (!error.empty())
            {
                std::cerr << error << std::endl;
            }
        }
    }

    ImGui::EndChild();
}

Rack *FindRackByID(int rackID)
{
    for (auto &rack : Racks)
    {
        if (rack.ID == rackID)
            return &rack;
    }

    return nullptr;
}

// --mdu handling--

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

                                          const int removedModuleID = module.ID;
                                          RemoveDynamicModuleFromRack(module);
                                          RemoveLinksForModule(rack, removedModuleID);
                                          return true;
                                      });
    }
}

void ProcessMduFileChanges()
{
    if (!GFileWatcherInitialized)
    {
        GFileWatcher.SetWatchPaths({"src/Modules", "modules"});
        GFileWatcher.PrimeSnapshot();
        GFileWatcherInitialized = true;
    }

    for (const auto &change : GFileWatcher.PollChanges())
    {
        if (change.Type == MDU::FileChangeType::Added ||
            change.Type == MDU::FileChangeType::Modified)
        {
            if (change.Type == MDU::FileChangeType::Modified)
            {
                RemoveDynamicModulesFromAllRacksBySourcePath(change.Path);
            }

            std::string error;
            GModuleLoader.LoadFromMduFile(change.Path, &error);
            if (!error.empty())
            {
                std::cerr << error << std::endl;
            }
        }
        else if (change.Type == MDU::FileChangeType::Removed)
        {
            RemoveDynamicModulesFromAllRacksBySourcePath(change.Path);

            std::string error;
            GModuleLoader.UnloadByPath(change.Path, &error);
            if (!error.empty())
            {
                std::cerr << error << std::endl;
            }
        }
    }
}
