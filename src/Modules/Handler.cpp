#include <iostream>
#include <list>
#include <thread>
#include <chrono>
#include "../../include/Functions/Window.h"
#include "../../include/Functions/Audio.h"
#include "../../include/Functions/ImGuiUtil.h"
#include "../../include/Output.h"
#include "../../include/VoltageControllFilter.h"
#include "../../include/WaveForm.h"
#include "imgui.h"
#include <vector>
#include <string>
#include <algorithm>
#include <cstdio>

// --Module Type Definitions--
enum ModuleType
{
    MODULE_VCO,   // VCO (Voltage Controlled Oscillator)
    MODULE_VCF,   // VCF (Voltage Controlled Filter)
    MODULE_VCA,   // VCA (Voltage Controlled Amplifier)
    MODULE_OUTPUT // Output
};

enum FilterType
{
    FILTER_LP,   // Low-Pass
    FILTER_HP,   // High-Pass
    FILTER_BP,   // Band-Pass
    FILTER_NOTCH // Notch
};

// --Module Instance--
struct ModuleInstance
{
    ModuleType Type;
    int ID;
    std::string Name;
    bool Active = true;
    FilterType filterType = FILTER_LP; // For VCF modules
    // Module-specific data stored here (could use variant, union, or void*)
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

// --Audio Processing Callback (called from audio thread)--
void AudioFilterCallback(float* buffer, int numSamples, void* userData)
{
    // Process audio through the first enabled rack with an Output module
    for (auto& rack : Racks)
    {
        if (!rack.Enabled) continue;
        
        // Check if rack has Output module
        bool hasOutput = false;
        for (const auto& module : rack.Modules)
        {
            if (module.Type == MODULE_OUTPUT && module.Active)
            {
                hasOutput = true;
                break;
            }
        }
        
        if (!hasOutput) continue;
        
        // Process through each module in the chain
        std::vector<float> tempBuffer(numSamples);
        float* currentInput = buffer;
        float* currentOutput = tempBuffer.data();
        bool needsSwap = false;
        
        for (const auto& module : rack.Modules)
        {
            if (!module.Active) continue;
                
            switch (module.Type)
            {
                case MODULE_VCO:
                    // VCO already generated audio
                    break;
                    
                case MODULE_VCF:
                    // Apply filter
                    VCF::ProcessAudio(currentInput, currentOutput, numSamples, static_cast<int>(module.filterType));
                    std::swap(currentInput, currentOutput);
                    needsSwap = !needsSwap;
                    break;
                    
                case MODULE_VCA:
                    // VCA (Amplifier) - not implemented yet, pass through
                    break;
                    
                case MODULE_OUTPUT:
                    // Apply output volume
                    Output::ProcessAudio(currentInput, numSamples);
                    break;
            }
        }
        
        // Copy result back to buffer if needed
        if (needsSwap)
        {
            std::copy(currentInput, currentInput + numSamples, buffer);
        }
        
        break; // Only process first rack with Output
    }
}

// --Function Prototypes--
void DrawConnector(const ImVec2 &start, const ImVec2 &end);
void DrawConnectors(const Rack &rack);

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
    case FILTER_LP:
        return "Low-Pass";
    case FILTER_HP:
        return "High-Pass";
    case FILTER_BP:
        return "Band-Pass";
    case FILTER_NOTCH:
        return "Notch";
    default:
        return "Unknown";
    }
}

// --Audio Processing Through Rack--
void ProcessRackAudio(Rack& rack, float* inputBuffer, int bufferSize)
{
    // Check if rack has an Output module
    bool hasOutput = false;
    for (const auto& module : rack.Modules)
    {
        if (module.Type == MODULE_OUTPUT)
        {
            hasOutput = true;
            break;
        }
    }
    
    // Only process if rack has Output module
    if (!hasOutput || !rack.Enabled)
        return;
    
    // Create buffers for signal chain
    std::vector<float> buffer1(bufferSize);
    std::vector<float> buffer2(bufferSize);
    
    // Copy input to first buffer
    std::copy(inputBuffer, inputBuffer + bufferSize, buffer1.begin());
    
    float* currentInput = buffer1.data();
    float* currentOutput = buffer2.data();
    
    // Process through each module in the chain
    for (const auto& module : rack.Modules)
    {
        if (!module.Active)
            continue;
            
        switch (module.Type)
        {
            case MODULE_VCO:
                // VCO generates signal (input is ignored for VCO)
                // Input comes from WaveFormGen namespace
                break;
                
            case MODULE_VCF:
                VCF::ProcessAudio(currentInput, currentOutput, bufferSize, static_cast<int>(module.filterType));
                std::swap(currentInput, currentOutput);
                break;
                
            case MODULE_VCA:
                // VCA (Amplifier) - not implemented yet, pass through
                std::copy(currentInput, currentInput + bufferSize, currentOutput);
                std::swap(currentInput, currentOutput);
                break;
                
            case MODULE_OUTPUT:
                // Send to audio output
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

        // Add new rack button
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

            // Rack header with button - use TreeNode for better control
            std::string rackHeaderLabel = "Rack #" + std::to_string(rack.ID) + ": " + rack.Name;

            // TreeNode doesn't span full width like CollapsingHeader, so button can be on same line
            bool rackOpen = ImGui::TreeNodeEx(rackHeaderLabel.c_str(), ImGuiTreeNodeFlags_DefaultOpen);

            // Button on same line
            ImGui::SameLine();
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 4.0f);
            bool shouldDelete = false;

            if (ImGui::Button("Del##rack", ImVec2(50, 0)))
            {
                shouldDelete = true;
                NextRackID--;
            }

            // Handle deletion after rendering
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

                // Add module button
                std::string addButtonLabel = "Add Module##" + std::to_string(rack.ID);
                if (ImGui::Button(addButtonLabel.c_str()))
                {
                    ImGui::OpenPopup("AddModulePopup");
                }

                if (ImGui::BeginPopup("AddModulePopup"))
                {
                    if (ImGui::MenuItem("VCO"))
                        AddModuleToRack(rack, MODULE_VCO, "VCO-1");
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

                    // Make module selectable with constrained width
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

                    // Check if there's a next module and draw arrow
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

        // Module Editor Window
        if (SelectedModuleID != -1)
        {
            // Find the selected module
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
                    WaveFormGen::MainImgui();
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
                    ImGui::Text("VCA Controls:");
                    
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
        // (Audio processing happens in the Audio callback, this updates waveforms)
        std::vector<WaveForm> activeWaves;
        for (auto& rack : Racks)
        {
            if (!rack.Enabled) continue;
            
            // Check if rack has Output module
            bool hasOutput = false;
            for (const auto& module : rack.Modules)
            {
                if (module.Type == MODULE_OUTPUT && module.Active)
                {
                    hasOutput = true;
                    break;
                }
            }
            
            // Only output audio from racks with Output modules
            if (hasOutput)
            {
                // Add all enabled VCOs to active waves
                for (auto& wave : WaveFormGen::WaveForms)
                {
                    if (wave.Enabled)
                        activeWaves.push_back(wave);
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
    
    // Register filter callback for rack processing
    Audio::SetFilterCallback(AudioFilterCallback, nullptr);
    
    Window::PollEvents();
}

void CleanUp()
{
    Window::DestroyWindow();
    Audio::Close();
}

void Render()
{
    Window::ClearColor(0.08f, 0.08f, 0.10f, 1.0f); // Darker background matching theme
    ImGuiUtil::Render();
    Window::SwapBuffers();
    Window::PollEvents();
}

void ImGuiFuncGen()
{
    WaveFormGen::MainImgui();
}

void DrawConnectors(const Rack &rack)
{
    if (rack.Modules.size() < 2)
        return;
    
    // Draw connectors between all consecutive modules
    int moduleCount = 0;
    for (auto moduleIt = rack.Modules.begin(); moduleIt != rack.Modules.end(); ++moduleIt)
    {
        if (std::next(moduleIt) != rack.Modules.end())
        {
            ImGui::Text("    ||");
            ImGui::Text("    V");
        }
        moduleCount++;
    }
}