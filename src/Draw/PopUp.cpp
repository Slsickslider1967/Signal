#include <iostream>

#include "imgui.h"
#include "imgui_stdlib.h"
#include "imnodes.h"

#include "Draw/Draw.h"
#include "Functions/ConsoleHandling.h"

namespace Draw
{
    // Draw the selected module details panel, including controls and live scope view.
    void DrawModuleDetails()
    {
        DynamicModule *selectedModule = nullptr;
        // Locate the currently selected module across all racks.
        for (auto &rack : Racks)
        {
            for (auto &module : rack.DynamicModules)
            {
                if (module.ID == SelectedModuleID)
                {
                    selectedModule = &module;
                    break;
                }
            }
            if (selectedModule != nullptr)
            {
                break;
            }
        }

        if (selectedModule == nullptr)
        {
            SelectedModuleID = -1;
            return;
        }

        bool windowOpen = true;
        ImGui::SetNextWindowSize(ImVec2(520.0f, 760.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSizeConstraints(ImVec2(400.0f, 460.0f), ImVec2(1400.0f, 1800.0f));
        std::string windowTitle = "Module Panel: " + selectedModule->Name + " #" + std::to_string(selectedModule->ID);
        ImGui::Begin(windowTitle.c_str(), &windowOpen);

        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.11f, 0.12f, 0.13f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10.0f, 8.0f));

        if (ImGui::BeginChild("ModuleFaceplate", ImVec2(0.0f, 0.0f), true))
        {
            ImGui::Text("%s", selectedModule->Name.c_str());
            ImGui::SameLine();
            ImGui::TextDisabled("[%s]", selectedModule->Metadata.ModuleType.c_str());

            ImGui::InputText("Module Name", &selectedModule->Name);
            ImGui::Checkbox("Module Active", &selectedModule->Active);
            ImGui::Separator();

            ImGui::Text("I/O");
            ImGui::Separator();
            ImGui::Text("Input Jacks: %d", selectedModule->InPins);
            ImGui::SameLine();
            ImGui::Text("Output Jacks: %d", selectedModule->OutPins);

            if (selectedModule->Instance != nullptr)
            {
                selectedModule->Instance->DrawEditor();
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Text("Signal Scope");
            auto inputIt = GlobaloduleScopeInputs.find(selectedModule->ID);
            auto outputIt = GlobalModuleScopeOutputs.find(selectedModule->ID);

            if (inputIt == GlobaloduleScopeInputs.end() || outputIt == GlobalModuleScopeOutputs.end())
            {
                ImGui::TextDisabled("No scope data yet. Start audio and run the patch.");
            }
            else
            {
                DrawScopeOverlay(&inputIt->second, &outputIt->second, "Input vs Output");

                float peakOut = 0.0f;
                for (float sample : outputIt->second)
                {
                    float amplitude = sample < 0.0f ? -sample : sample;
                    if (amplitude > peakOut)
                    {
                        peakOut = amplitude;
                    }
                }
                if (peakOut > 1.0f)
                    peakOut = 1.0f;
            }
        }

        ImGui::EndChild();
        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor();

        bool requestRemove = ImGui::Button("Remove Module", ImVec2(-1.0f, 0.0f));
        ImGui::End();

        if (requestRemove)
        {
            Console::AppendConsoleLine("Remove Module clicked for module #" + std::to_string(selectedModule->ID) + " (" + selectedModule->Name + ")");
            RemoveNode(selectedModule->ID);
            SelectedModuleID = -1;
            return;
        }

        if (!windowOpen)
        {
            ShowModuleDetails = false;
        }
    }

    // Render selectable loaded modules and add one to the rack when clicked.
    void DrawAvailableModulesChild(Rack &rack)
    {
        if (!ImGui::BeginChild("AvailableModulesChild", ImVec2(0.0f, 220.0f), true))
        {
            ImGui::EndChild();
            return;
        }

        const auto &loadedModules = GlobalModuleLoader.GetLoadedModules();
        ImGui::Text("Loaded: %d", static_cast<int>(loadedModules.size()));
        if (loadedModules.empty())
        {
            ImGui::TextDisabled("No loaded .mdu modules");
            if (!GlobalLastMduError.empty())
            {
                ImGui::Separator();
                ImGui::TextWrapped("Last loader error: %s", GlobalLastMduError.c_str());
            }
        }
        else
        {
            for (const auto &entry : loadedModules)
            {
                const std::string &sourcePath = entry.first;
                const auto &loadedModule = entry.second;

                std::string label = loadedModule.Metadata.ModuleName.empty() ? sourcePath : loadedModule.Metadata.ModuleName;

                if (ImGui::Selectable(label.c_str(), false, ImGuiSelectableFlags_DontClosePopups))
                {
                    std::string error;
                    AddDynamicModuleToRack(rack, sourcePath, &error);
                    if (!error.empty())
                    {
                        std::cerr << error << std::endl;
                    }
                }

                if (!loadedModule.Metadata.Author.empty())
                {
                    ImGui::SameLine();
                    ImGui::TextDisabled("by %s", loadedModule.Metadata.Author.c_str());
                }
                if (!loadedModule.Metadata.ModuleVersion.empty())
                {
                    ImGui::SameLine(ImGui::GetWindowWidth() - 80.0f);
                    ImGui::TextDisabled("v%s", loadedModule.Metadata.ModuleVersion.c_str());
                }
            }
        }

        ImGui::EndChild();
    }

    // Handle rack context popup interactions and return whether rack deletion was requested.
    bool PopUpTool(Rack &rack)
    {
        // For creating a rack by clicking an empty space
        if (ImGui::BeginPopupContextItem("RackCreatorContextMenu"))
        {
            if (ImGui::Selectable("Create New Rack"))
            {
                // CreateNewRack();
            }

            if (ImGui::IsKeyPressed(ImGuiKey_Escape))
            {
                ImGui::CloseCurrentPopup();
            }
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsWindowHovered())
            {
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
    }
}