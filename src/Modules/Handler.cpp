#include <iostream>
#include <list>
#include <thread>
#include <chrono>
#include <map>
#include "imgui.h"
#include "imnodes.h"
#include <vector>
#include <string>
#include <algorithm>
#include <cstdio>

// --Local headers--
#include "../../include/Functions/Window.h"
#include "../../include/Functions/Audio.h"
#include "../../include/Functions/ImGuiUtil.h"
#include "../../include/Output.h"
#include "../../include/VoltageControllFilter.h"
#include "../../include/WaveForm.h"
#include "../../include/Voltage-ControlledAmplifier.h"
#include "../../include/LowFrequencyOscillator.h"
#include "../../include/Module.h"
#include "../../include/ModuleEditor.h"

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

// --Audio Processing Callback--
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

int main()
{
    MainWindow();

    while (!Window::ShouldClose())
    {
        ImGuiUtil::Begin();

        bool showDemoWindow = false;
        if (showDemoWindow)
            ImGui::ShowDemoWindow(&showDemoWindow);

        ImGui::Begin("Rack Manager");

        if (ImGui::Button("Add Rack"))
        {
            CreateRack(std::string("Rack ") + std::to_string(NextRackID));
        }

        ImGui::Separator();

        // Display all racks
        for (auto rackIt = Racks.begin(); rackIt != Racks.end();)
        {
            Rack &rack = *rackIt;
            ImGui::PushID(rack.ID);

            std::string rackHeaderLabel = "Rack #" + std::to_string(rack.ID) + ": " + rack.Name;

            bool rackOpen = ImGui::TreeNodeEx(rackHeaderLabel.c_str(), ImGuiTreeNodeFlags_DefaultOpen);

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
                ImGui::Indent();

                std::string addButtonLabel = "Add Module##" + std::to_string(rack.ID);
                if (ImGui::Button(addButtonLabel.c_str()))
                {
                    ImGui::OpenPopup("AddModulePopup");
                }

                if (ImGui::BeginPopup("AddModulePopup"))
                {

                    if (ImGui::MenuItem("VCO"))
                        AddModuleToRack(rack, MODULE_VCO, "VCO-1");
                    if (ImGui::MenuItem("LFO"))
                        AddModuleToRack(rack, MODULE_LFO, "LFO-1");
                    if (ImGui::MenuItem("VCF"))
                        AddModuleToRack(rack, MODULE_VCF, "VCF-1");
                    if (ImGui::MenuItem("VCA"))
                        AddModuleToRack(rack, MODULE_VCA, "VCA-1");
                    if (ImGui::MenuItem("Output"))
                        AddModuleToRack(rack, MODULE_OUTPUT, "Output");
                    ImGui::EndPopup();
                }

                ImGui::Separator();

                ImGui::Text("Signal Chain - Node Editor:");

                DrawRackEditor(rack);

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

        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    CleanUp();
    return 0;
}

void DrawRackEditor(Rack &rack)
{
    ImNodes::BeginNodeEditor();
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

void MainWindow()
{
    setenv("PREFER_X11", "1", 1);

    Window::CreateWindow(1280, 720, "Signal Handler");
    Audio::Init();

    Audio::SetFilterCallback(AudioFilterCallback, nullptr);

    Window::PollEvents();
}

// --Memory Cleanup--
void CleanUp()
{
    Window::DestroyWindow();
    Audio::Close();
}

// --Rendering for ImGui and Window--
void Render()
{
    Window::ClearColor(0.08f, 0.08f, 0.10f, 1.0f);
    ImGuiUtil::Render();
    Window::SwapBuffers();
    Window::PollEvents();
}
