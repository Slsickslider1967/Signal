#include <iostream>
#include <list>
#include <thread>
#include <chrono>
#include "imgui.h"
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

// --Module Type Definitions--
enum ModuleType
{
    MODULE_VCO,
    MODULE_LFO,
    MODULE_VCF,
    MODULE_VCA,
    MODULE_OUTPUT
};

enum FilterType
{
    FILTER_LowPass,
    FILTER_HighPass,
    FILTER_BandPass,
    FILTER_NOTCH
};

// --Module Instance--
// Each module is independent - VCO modules own their waveform via vcoWave
// No shared/global state: each module has its own configuration and parameters
struct ModuleInstance
{
    ModuleType Type;
    int ID;
    std::string Name;
    bool Active = true;
    FilterType filterType = FILTER_LowPass; // For VCF modules
    WaveForm vcoWave{};                     // Private waveform for VCO modules
};

// --Rack Structure--
struct Rack
{
    int ID;
    std::string Name;
    std::list<ModuleInstance> Modules;
    bool Enabled = true;
};

// --Variables--
static int NextRackID = 1;
static int NextModuleID = 1;
std::list<Rack> Racks;
static int SelectedModuleID = -1;

// --Audio Processing Callback--
void AudioFilterCallback(float *buffer, int numSamples, void *userData)
{
    for (auto &rack : Racks)
    {
        if (!rack.Enabled)
            continue;

        bool hasOutput = false;
        for (const auto &module : rack.Modules)
        {
            if (module.Type == MODULE_OUTPUT && module.Active)
            {
                hasOutput = true;
                break;
            }
        }

        if (!hasOutput)
            continue;

        std::vector<float> tempBuffer(numSamples);
        float *currentInput = buffer;
        float *currentOutput = tempBuffer.data();
        bool needsSwap = false;

        for (const auto &module : rack.Modules)
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
                VCF::ProcessAudio(currentInput, currentOutput, numSamples, static_cast<int>(module.filterType));
                std::swap(currentInput, currentOutput);
                needsSwap = !needsSwap;
                break;

            case MODULE_VCA:
                VCA::ProcessAudio(currentInput, currentOutput, numSamples);
                std::swap(currentInput, currentOutput);
                needsSwap = !needsSwap;
                break;

            case MODULE_OUTPUT:
                Output::ProcessAudio(currentInput, numSamples);
                break;
            }
        }

        // Copy result back to buffer if needed
        if (needsSwap)
        {
            std::copy(currentInput, currentInput + numSamples, buffer);
        }

        break;
    }
}

// --Function Prototypes--


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
    ModuleInstance module;
    module.ID = NextModuleID++;
    module.Type = type;
    module.Name = name;
    module.Active = true;

    if (type == MODULE_VCO)
    {
        module.vcoWave.WaveID = module.ID;
        module.vcoWave.Enabled = true;
        module.vcoWave.OpenWindow = true;
        module.vcoWave.RequestDockBelow = true;
        module.vcoWave.Type = Sine;
        module.vcoWave.coarseTune = 440.0f;
        module.vcoWave.Amplitude = 0.8f;
        module.vcoWave.SampleRate = 44100;
        module.vcoWave.vOctCV = 0.5f;
        module.vcoWave.linearFMCV = 0.5f;
        module.vcoWave.pwmCV = 0.5f;
        module.vcoWave.vRange = WaveForm::Bipolar10V;
        module.vcoWave.fmDepth = 0.0f;
    }

    rack.Modules.push_back(module);
}

void RemoveModuleFromRack(Rack &rack, int moduleID)
{
    rack.Modules.remove_if([moduleID](const ModuleInstance &m)
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

// --Audio Processing Through Rack--
void ProcessRackAudio(Rack &rack, float *inputBuffer, int bufferSize)
{
    bool hasOutput = false;
    for (const auto &module : rack.Modules)
    {
        if (module.Type == MODULE_OUTPUT)
        {
            hasOutput = true;
            break;
        }
    }

    if (!hasOutput || !rack.Enabled)
        return;

    std::vector<float> buffer1(bufferSize);
    std::vector<float> buffer2(bufferSize);

    std::copy(inputBuffer, inputBuffer + bufferSize, buffer1.begin());

    float *currentInput = buffer1.data();
    float *currentOutput = buffer2.data();

    for (const auto &module : rack.Modules)
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
            VCF::ProcessAudio(currentInput, currentOutput, bufferSize, static_cast<int>(module.filterType));
            std::swap(currentInput, currentOutput);
            break;

        case MODULE_VCA:
            VCA::ProcessAudio(currentInput, currentOutput, bufferSize);
            std::swap(currentInput, currentOutput);
            break;

        case MODULE_OUTPUT:
            Output::ProcessAudio(currentInput, bufferSize);
            break;
        }
    }
}

// --Function Prototypes--
void MainWindow();
void CleanUp();
void ImGuiFuncGen();
void Render();

int main()
{
    MainWindow();

    while (!Window::ShouldClose())
    {
        ImGuiUtil::Begin();

        ImGui::Begin("Rack Manager");

        if (ImGui::Button("Add Rack"))
        {
            CreateRack("Rack " + std::to_string(NextRackID));
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
                NextRackID--;
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
                    bool hasVCO = false;
                    bool hasLFO = false;
                    for (const auto &module : rack.Modules)
                    {
                        if (module.Type == MODULE_VCO)
                            hasVCO = true;
                        else if (module.Type == MODULE_LFO)
                            hasLFO = true;
                    }

                    if (hasLFO || hasVCO)
                    {
                        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Note: Only one VCO or LFO allowed per rack");
                    }
                    else 
                    {
                        if (!hasVCO && ImGui::MenuItem("VCO"))
                            AddModuleToRack(rack, MODULE_VCO, "VCO-1");
                        if (!hasLFO && ImGui::MenuItem("LFO"))
                            AddModuleToRack(rack, MODULE_LFO, "LFO-1");
                    }
                    if (ImGui::MenuItem("VCF"))
                        AddModuleToRack(rack, MODULE_VCF, "VCF-1");
                    if (ImGui::MenuItem("VCA"))
                        AddModuleToRack(rack, MODULE_VCA, "VCA-1");
                    if (ImGui::MenuItem("Output"))
                        AddModuleToRack(rack, MODULE_OUTPUT, "Output");
                    ImGui::EndPopup();
                }

                ImGui::Separator();

                ImGui::Text("Signal Chain:");
                int moduleIndex = 0;
                for (auto moduleIt = rack.Modules.begin(); moduleIt != rack.Modules.end();)
                {
                    ModuleInstance &module = *moduleIt;
                    ImGui::PushID(module.ID);

                    std::string moduleLabel = "[" + std::to_string(moduleIndex) + "] " + ModuleTypeToString(module.Type) + " (" + module.Name + ")";
                    if (module.Type == MODULE_VCF)
                    {
                        moduleLabel += " [" + std::string(FilterTypeToString(module.filterType)) + "]";
                    }

                    bool isSelected = (module.ID == SelectedModuleID);
                    float availWidth = ImGui::GetContentRegionAvail().x;
                    float buttonWidth = 25.0f;
                    float selectableWidth = availWidth - buttonWidth - ImGui::GetStyle().ItemSpacing.x;

                    if (ImGui::Selectable(moduleLabel.c_str(), isSelected, ImGuiSelectableFlags_None, ImVec2(selectableWidth, 0)))
                    {
                        SelectedModuleID = module.ID;
                    }

                    ImGui::SameLine();
                    std::string deleteLabel = "X##del" + std::to_string(module.ID);
                    if (ImGui::Button(deleteLabel.c_str(), ImVec2(buttonWidth, 0)))
                    {
                        if (module.ID == SelectedModuleID)
                            SelectedModuleID = -1;
                        moduleIt = rack.Modules.erase(moduleIt);
                        ImGui::PopID();
                        moduleIndex++;
                        continue;
                    }

                    auto nextModuleIt = std::next(moduleIt);
                    if (nextModuleIt != rack.Modules.end())
                    {
                        ImGui::Text("    ||");
                        ImGui::Text("    V");
                    }

                    ImGui::PopID();
                    ++moduleIt;
                    moduleIndex++;
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
            ModuleInstance *selectedModule = nullptr;
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
                std::string windowTitle = "Edit: " + std::string(ModuleTypeToString(selectedModule->Type)) + " #" + std::to_string(selectedModule->ID);
                ImGui::Begin(windowTitle.c_str(), &windowOpen, ImGuiWindowFlags_AlwaysAutoResize);

                // Module name editor
                char moduleNameBuffer[128];
                strncpy(moduleNameBuffer, selectedModule->Name.c_str(), sizeof(moduleNameBuffer));
                if (ImGui::InputText("Name", moduleNameBuffer, sizeof(moduleNameBuffer)))
                {
                    selectedModule->Name = moduleNameBuffer;
                }

                ImGui::Separator();

                // Module-specific controls
                switch (selectedModule->Type)
                {
                case MODULE_VCO:
                {
                    ImGui::PushID(selectedModule->ID);
                    WaveFormGen::DrawWaveFormEditor(selectedModule->vcoWave);
                    ImGui::PopID();
                    break;
                }

                case MODULE_LFO:
                {
                    LFO::MainImGui();
                    break;
                }

                case MODULE_VCF:
                {
                    ImGui::Text("VCF Controls:");
                    const char *filterTypes[] = {"Low-Pass", "High-Pass", "Band-Pass", "Notch"};
                    int currentFilterType = static_cast<int>(selectedModule->filterType);
                    if (ImGui::Combo("Filter Type", &currentFilterType, filterTypes, 4))
                    {
                        selectedModule->filterType = static_cast<FilterType>(currentFilterType);
                    }
                    VCF::MainImGui();
                    break;
                }

                case MODULE_VCA:
                {
                    VCA::MainImGui();

                    break;
                }

                case MODULE_OUTPUT:
                {
                    ImGui::Text("Output Controls:");
                    Output::MainImGui();
                    break;
                }
                }

                if (ImGui::Button("Close") || !windowOpen)
                {
                    SelectedModuleID = -1;
                }

                ImGui::End();
            }
            else
            {
                SelectedModuleID = -1;
            }
        }

        ImGuiUtil::End();

        Render();

        // Process audio through each enabled rack
        std::vector<WaveForm> activeWaves;
        for (auto &rack : Racks)
        {
            if (!rack.Enabled)
                continue;

            bool hasOutput = false;
            for (const auto &module : rack.Modules)
            {
                if (module.Type == MODULE_OUTPUT && module.Active)
                {
                    hasOutput = true;
                    break;
                }
            }

            if (hasOutput)
            {
                for (const auto &module : rack.Modules)
                {
                    if (module.Type == MODULE_VCO && module.Active && module.vcoWave.Enabled)
                    {
                        activeWaves.push_back(module.vcoWave);
                    }
                }
            }
        }

        // Send active waves to audio system
        Audio::SetWaveForms(activeWaves);

        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    CleanUp();
    return 0;
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
