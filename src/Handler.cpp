#include <iostream>
#include <list>
#include <thread>
#include <chrono>
#include <map>

#include "imgui.h"
#include "imgui_stdlib.h"
#include "imnodes.h"
#include <vector>
#include <string>
#include <algorithm>
#include <cstdio>

// --Local headers--
#include "../include/Functions/Window.h"
#include "../include/Functions/Audio.h"
#include "../include/Functions/Module.h"
#include "../include/Functions/ImGuiUtil.h"
#include "../include/Output.h"
#include "../include/VoltageControllFilter.h"
#include "../include/WaveForm.h"
#include "../include/Voltage-ControlledAmplifier.h"
#include "../include/LowFrequencyOscillator.h"
#include "../include/ModuleEditor.h"
#include "../include/MDU/ModuleLoader.h"
#include "../include/MDU/mduParser.h"

// --Rack Structure--
struct Rack
{
    int ID;
    std::string Name;
    std::list<Module> Modules;
    std::vector<Link> Links;
    bool Enabled = true;
};

// --Variables--
static int NextRackID = 1;
static int NextModuleID = 1;
static int NextLinkID = 1;
std::list<Rack> Racks;
static int SelectedModuleID = -1;
static int SelectedRackID = -1;

// --Forward declarations for addon modules--
namespace SpeedManipulation
{
    void MainImgui();
}

// --Rack Management Functions--
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
    Racks.remove_if([rackID](const Rack &r)
                    { return r.ID == rackID; });
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

void RemoveModuleFromRack(Rack &rack, int moduleID)
{
    rack.Modules.remove_if([moduleID](const Module &m)
                           { return m.ID == moduleID; });
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

// --Function Prototypes--
void MainWindow();
void CleanUp();

void ImGuiFuncGen();
void Render();

void DrawRackEditor(Rack &rack);
void CreateLinks(Rack &rack);
void DrawLinks(Rack &rack);
void AddPins(const Module &module);

void ChildNodeWindow();
void DrawModuleDetails();
bool PopUpTool(Rack &rack);
void AudioFilterCallback(float *buffer, int numSamples, void *userData);
void SetupAudioHandling();
void ShutdownAudioHandling();
void UpdateAudioWaveFormsFromRacks();

void AddRackTool();
void PopUpTool();
Rack *FindRackByID(int rackID);

// mdu prototypes
void ProcessMDUModule(const MDU::BufferView& bufferView, MDU::Module* module);

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
        
        // Display all racks
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

        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    CleanUp();
    return 0;
}


// --Draw--


void DrawRackEditor(Rack &rack)
{
    ImNodes::BeginNodeEditor();
    if (ImNodes::IsEditorHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        SelectedRackID = rack.ID;
        SelectedModuleID = -1;
    }
    bool openContextMenu = ImNodes::IsEditorHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right);

    int nodeIndex = 0;

    for (auto moduleIt = rack.Modules.begin(); moduleIt != rack.Modules.end();)
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

        ++moduleIt;
        nodeIndex++;
    }

    DrawLinks(rack);
    ImNodes::EndNodeEditor();

    if (openContextMenu)
        ImGui::OpenPopup("RackContextMenu");

    CreateLinks(rack);

    {
        int deletedLinkID;
        if (ImNodes::IsLinkDestroyed(&deletedLinkID))
        {
            rack.Links.erase(
                std::remove_if(rack.Links.begin(), rack.Links.end(),
                               [deletedLinkID](const Link &l)
                               { return l.ID == deletedLinkID; }),
                rack.Links.end());
        }
    }

}

void CreateLinks(Rack &rack)
{
    int startAttr, endAttr;
    if (ImNodes::IsLinkCreated(&startAttr, &endAttr))
    {
        int startModuleID = startAttr / 1000;
        int endModuleID = endAttr / 1000;
        
        int startPin = startAttr % 1000;
        int endPin = endAttr % 1000;

        // Output pins: remainder < 100, Input pins: remainder >= 100
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
        ImNodes::Link(link.ID, link.StartModuleID * 1000 + link.StartPinIndex, 
                      link.EndModuleID * 1000 + 100 + link.EndPinIndex);
    }
}

void AddPins(const Module &module)
{

    for (int i = 0; i < module.InPins; i++)
    {
        ImNodes::BeginInputAttribute(module.ID * 1100 + i);
        ImGui::Text("In");
        ImNodes::EndInputAttribute();
    }
    for (int i = 0; i < module.OutPins; i++)
    {
        ImNodes::BeginOutputAttribute(module.ID * 1000 + i);
        ImGui::Text("Out");
        ImNodes::EndOutputAttribute();
    }
}

void DrawChildNodeWindow()
{
    ImGui::BeginChild("ChildNodeWindow", ImVec2(0, 200), true);
    ImGui::Text("This is a child window inside the node editor.");
    ImGui::EndChild();
}

void RemoveNode(int nodeID)
{
    for (auto &rack : Racks)
    {
        RemoveModuleFromRack(rack, nodeID);
        
        rack.Links.erase(
            std::remove_if(rack.Links.begin(), rack.Links.end(),
                           [nodeID](const Link &l)
                           { return l.StartModuleID == nodeID || l.EndModuleID == nodeID; }),
            rack.Links.end());
    }
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

// --Draw Handling--



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

        if (ImGui::Selectable("Add VCO", false, ImGuiSelectableFlags_DontClosePopups))
            AddModuleToRack(rack, MODULE_VCO, "VCO-1");
        if (ImGui::Selectable("Add LFO", false, ImGuiSelectableFlags_DontClosePopups))
            AddModuleToRack(rack, MODULE_LFO, "LFO-1");
        if (ImGui::Selectable("Add VCF", false, ImGuiSelectableFlags_DontClosePopups))
            AddModuleToRack(rack, MODULE_VCF, "VCF-1");
        if (ImGui::Selectable("Add VCA", false, ImGuiSelectableFlags_DontClosePopups))
            AddModuleToRack(rack, MODULE_VCA, "VCA-1");
        if (ImGui::Selectable("Add Output", false, ImGuiSelectableFlags_DontClosePopups))
            AddModuleToRack(rack, MODULE_OUTPUT, "Output-1");

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
void ProcessMDUModule(const MDU::BufferView& bufferView, MDU::Module* module)
{
    MDU::FileWatcher watcher;
    watcher.SetWatchPaths({"src/Modules", "modules"});
    watcher.PrimeSnapshot();

    // each frame or every N milliseconds
    for (const auto& change : watcher.PollChanges())
    {
        if (change.Type == MDU::FileChangeType::Added ||
            change.Type == MDU::FileChangeType::Modified)
        {
            std::string error;
            loader.LoadFromMduFile(change.Path, &error);
        }
        else if (change.Type == MDU::FileChangeType::Removed)
        {
            std::string error;
            loader.UnloadByPath(change.Path, &error);
        }
    }
}