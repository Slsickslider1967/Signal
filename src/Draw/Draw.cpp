#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "imgui.h"
#include "imgui_stdlib.h"
#include "imnodes.h"
#include "implot.h"

#include "Audio/Record.h"
#include "Draw/Draw.h"
#include "Draw/ImGuiUtil.h"
#include "Draw/Window.h"
#include "Functions/ConsoleHandling.h"
#include "MDU/CreateMDU.h"

namespace
{
    // Draw overlaid input/output scope traces for a module panel.
    void DrawScopeOverlay(const std::vector<float> *inputSamples,
                          const std::vector<float> *outputSamples,
                          const char *plotLabel)
    {
        if (inputSamples == nullptr || outputSamples == nullptr)
        {
            return;
        }

        if (inputSamples->empty() || outputSamples->empty())
        {
            return;
        }

        ImPlotFlags plotFlags = ImPlotFlags_NoMenus |
                                ImPlotFlags_NoBoxSelect |
                                ImPlotFlags_NoMouseText;
        ImPlotAxisFlags axisFlags = ImPlotAxisFlags_NoDecorations |
                                    ImPlotAxisFlags_Lock;

        if (ImPlot::BeginPlot(plotLabel, ImVec2(-1.0f, 170.0f), plotFlags))
        {
            int sampleCount = std::min(static_cast<int>(inputSamples->size()), static_cast<int>(outputSamples->size()));
            ImPlot::SetupAxes(nullptr, nullptr, axisFlags, axisFlags);
            ImPlot::SetupAxisLimits(ImAxis_X1, 0.0, static_cast<double>(sampleCount), ImGuiCond_Always);
            ImPlot::SetupAxisLimits(ImAxis_Y1, -1.1, 1.1, ImGuiCond_Always);

            ImPlot::PlotLine("Input", inputSamples->data(), sampleCount);
            ImPlot::PlotLine("Output", outputSamples->data(), sampleCount);

            ImPlot::EndPlot();
        }
    }

    // Build a unique imnodes attribute ID for an input pin.
    int MakeInputAttributeID(int moduleID, int pinIndex)
    {
        return moduleID * 1000 + 100 + pinIndex;
    }

    // Build a unique imnodes attribute ID for an output pin.
    int MakeOutputAttributeID(int moduleID, int pinIndex)
    {
        return moduleID * 1000 + pinIndex;
    }

    // Find a rack by runtime ID and return a mutable pointer.
    Rack *FindRackByID(int rackID)
    {
        for (auto &rack : Racks)
        {
            if (rack.ID == rackID)
            {
                return &rack;
            }
        }
        return nullptr;
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
            }
        }

        ImGui::EndChild();
    }

    // Handle rack context popup interactions and return whether rack deletion was requested.
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

    // Render all existing links for the active node editor.
    void DrawLinks(Rack &rack)
    {
        for (const auto &link : rack.Links)
        {
            ImNodes::Link(link.ID,
                          MakeOutputAttributeID(link.StartModuleID, link.StartPinIndex),
                          MakeInputAttributeID(link.EndModuleID, link.EndPinIndex));
        }
    }

    // Create a new link when a valid output->input connection is made in the editor.
    void CreateLinks(Rack &rack)
    {
        int startAttr = 0;
        int endAttr = 0;
        if (ImNodes::IsLinkCreated(&startAttr, &endAttr))
        {
            int startModuleID = startAttr / 1000;
            int endModuleID = endAttr / 1000;
            int startPin = startAttr % 1000;
            int endPin = endAttr % 1000;

            bool isStartOutput = (startPin < 100);
            bool isEndInput = (endPin >= 100);
            if (!isStartOutput || !isEndInput)
            {
                return;
            }

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

    // Draw named input/output pin attributes for one module node.
    void AddPins(const DynamicModule &module)
    {
        for (int i = 0; i < module.InPins; ++i)
        {
            std::string label = "In";
            if (i < static_cast<int>(module.Metadata.InputPins.size()))
            {
                const auto &pin = module.Metadata.InputPins[i];
                if (!pin.label.empty())
                {
                    label = pin.label;
                }
                else if (!pin.ID.empty())
                {
                    label = pin.ID;
                }
            }

            ImNodes::BeginInputAttribute(MakeInputAttributeID(module.ID, i));
            ImGui::Text("%s", label.c_str());
            ImNodes::EndInputAttribute();
        }

        for (int i = 0; i < module.OutPins; ++i)
        {
            std::string label = "Out";
            if (i < static_cast<int>(module.Metadata.OutputPins.size()))
            {
                const auto &pin = module.Metadata.OutputPins[i];
                if (!pin.label.empty())
                {
                    label = pin.label;
                }
                else if (!pin.ID.empty())
                {
                    label = pin.ID;
                }
            }

            ImNodes::BeginOutputAttribute(MakeOutputAttributeID(module.ID, i));
            ImGui::Text("%s", label.c_str());
            ImNodes::EndOutputAttribute();
        }
    }

    // Render the detachable debug console window.
    void Debug()
    {
        if (!GlobalShowDebugConsole)
        {
            return;
        }

        ImGui::SetNextWindowSize(ImVec2(900, 420), ImGuiCond_FirstUseEver);
        ImGui::Begin("Debug Console", &GlobalShowDebugConsole);

        ImGui::SameLine();

        ImGui::SameLine();
        if (ImGui::Button("Clear Output"))
        {
            Console::ClearConsoleOutput();
        }

        ImGui::SameLine();
        ImGui::Checkbox("Auto-scroll", Console::AutoScrollFlag());

        ImGui::Separator();

        if (ImGui::BeginChild("ConsoleOutput", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar))
        {
            std::vector<std::string> lines = Console::GetConsoleLinesSnapshot();
            for (const std::string &line : lines)
            {
                ImGui::TextUnformatted(line.c_str());
            }

            if (*Console::AutoScrollFlag() && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 10.0f)
            {
                ImGui::SetScrollHereY(1.0f);
            }
        }
        ImGui::EndChild();

        ImGui::End();
    }
}

namespace Draw
{
    // Initialize the app window and audio pipeline for the editor session.
    void MainWindow()
    {
        #if (defined(__linux__) || defined(__unix__) || defined(__APPLE__))
                setenv("PREFER_X11", "1", 1);
        #endif
        int Width = 1020;
        int Height = 720;
        Window::CreateWindow(Width, Height, "Signal Handler");
        Console::AppendConsoleLine("Window initialized: Signal Handler (" + std::to_string(Width) + "x" + std::to_string(Height) + ")");
        SetupAudioHandling();
        Window::PollEvents();
    }

    // Tear down rendering window and audio resources.
    void CleanUp()
    {
        Window::DestroyWindow();
        ShutdownAudioHandling();
    }

    // Render one frame and present it.
    void Render()
    {
        Window::ClearColor(0.08f, 0.08f, 0.10f, 1.0f);
        ImGuiUtil::Render();
        Window::SwapBuffers();
        Window::PollEvents();
    }

    // Draw the top menu bar and global actions like rack/module/file/record/help.
    void DrawTopBar()
    {
        if (ImGui::BeginMainMenuBar())
        {
            // Rack menu groups creation, deletion, and future save/load actions.
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
                            {
                                rackIDToDelete = rack.ID;
                            }
                        }
                        if (rackIDToDelete != -1)
                        {
                            DeleteRack(rackIDToDelete);
                        }
                    }
                    ImGui::EndMenu();
                }

                //  ImGui::Separator();

                // if (ImGui::MenuItem("Save Racks (not implemented)"))
                // {
                //     Console::AppendConsoleLine("[info] Save Racks triggered (not implemented)");
                // }

                // if (ImGui::MenuItem("Load Racks (not implemented)"))
                // {
                //     Console::AppendConsoleLine("[info] Load Racks triggered (not implemented)");
                // }

                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Modules"))
            {
                auto removeSelectedModules = []()
                {
                    if (SelectedModuleIDs.empty())
                    {
                        return;
                    }

                    std::vector<int> moduleIDs(SelectedModuleIDs.begin(), SelectedModuleIDs.end());
                    for (int moduleID : moduleIDs)
                    {
                        RemoveNode(moduleID);
                    }
                    SelectedModuleIDs.clear();
                };

                if (ImGui::BeginMenu("Add Module to Selected Rack"))
                {
                    Rack *selectedRack = nullptr;
                    if (!SelectedRackIDs.empty())
                    {
                        selectedRack = FindRackByID(SelectedRackIDs.back());
                    }

                    if (selectedRack == nullptr)
                    {
                        ImGui::TextDisabled("Select a Rack First");
                    }
                    else
                    {
                        DrawAvailableModulesChild(*selectedRack);
                    }
                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("Remove Module from Selected Rack"))
                {
                    Rack *selectedRack = nullptr;
                    if (!SelectedRackIDs.empty())
                    {
                        selectedRack = FindRackByID(SelectedRackIDs.back());
                    }

                    if (selectedRack == nullptr)
                    {
                        ImGui::TextDisabled("Select a Rack First");
                    }
                    else if (selectedRack->DynamicModules.empty())
                    {
                        ImGui::TextDisabled("No modules in selected rack");
                    }
                    else
                    {
                        ImGui::TextDisabled("Check modules to include in Remove all selected Modules");
                        ImGui::Separator();
                        if (ImGui::Button("Remove Selected"))
                        {
                            if (!SelectedModuleIDs.empty())
                            {
                                std::vector<int> moduleIDs(SelectedModuleIDs.begin(), SelectedModuleIDs.end());
                                for (int moduleID : moduleIDs)
                                {
                                    RemoveNode(moduleID);
                                }
                                SelectedModuleIDs.clear();
                            }
                        }

                        ImGui::BeginChild("RemoveSelectedModulesChild", ImVec2(0.0f, 220.0f), true, ImGuiWindowFlags_HorizontalScrollbar);
                        for (const auto &module : selectedRack->DynamicModules)
                        {
                            bool moduleSelected = std::find(SelectedModuleIDs.begin(), SelectedModuleIDs.end(), module.ID) != SelectedModuleIDs.end();
                            std::string checkboxLabel = "##SelectModuleToRemove" + std::to_string(module.ID);
                            if (ImGui::Checkbox(checkboxLabel.c_str(), &moduleSelected))
                            {
                                if (moduleSelected)
                                {
                                    if (std::find(SelectedModuleIDs.begin(), SelectedModuleIDs.end(), module.ID) == SelectedModuleIDs.end())
                                    {
                                        SelectedModuleIDs.push_back(module.ID);
                                    }
                                }
                                else
                                {
                                    SelectedModuleIDs.remove(module.ID);
                                }
                            }

                            ImGui::SameLine();
                            ImGui::Text("%s", module.Name.c_str());
                        }
                        ImGui::EndChild();
                    }
                    ImGui::EndMenu();
                }

                ImGui::EndMenu();
            }

            static bool showSaveDialog = false;
            static bool popupJustOpened = false;
            static char saveFileName[256] = "";

            // Record menu controls capture lifecycle and saving.
            if (ImGui::BeginMenu("Record"))
            {
                if (ImGui::MenuItem("Start Recording"))
                {
                    Record::OpenWavForRecording("");
                    Record::StartRecording();
                    IsRecording = true;
                }
                if (ImGui::MenuItem("Stop Recording"))
                {
                    Record::StopRecording();
                    IsRecording = false;
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Save Last Recording"))
                {
                    showSaveDialog = true;
                    popupJustOpened = true;
                    std::strcpy(saveFileName, "recording.wav");
                }
                if (ImGui::MenuItem("Recording Settings"))
                {
                    Console::AppendConsoleLine("[info] Recording settings opened (not implemented)");
                }
                ImGui::EndMenu();
            }

            if (showSaveDialog && popupJustOpened)
            {
                ImGui::OpenPopup("Save Recording As");
                popupJustOpened = false;
            }
            if (ImGui::BeginPopupModal("Save Recording As", NULL, ImGuiWindowFlags_AlwaysAutoResize))
            {
                ImGui::InputText("File Name", saveFileName, IM_ARRAYSIZE(saveFileName));
                if (ImGui::Button("Save"))
                {
                    std::string fullPath = std::string(std::getenv("HOME") ? std::getenv("HOME") : "") + "/Documents/Signal/Recordings/" + saveFileName;
                    Record::SetWavPath(fullPath);
                    Record::SaveLastRecording();
                    showSaveDialog = false;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancel"))
                {
                    showSaveDialog = false;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
            if (ImGui::BeginMenu("File"))
            {
                if (ImGui::MenuItem("Open Template Folder"))
                {
                    LaunchDefaultFileManager(GlobalModuleLoader.GetTemplatePath());
                }
                if (ImGui::MenuItem("Open Recordings Folder"))
                {
                    const char *home = std::getenv("HOME");
                    std::filesystem::path recordingsPath;
                    if (home && *home)
                    {
                        recordingsPath = std::filesystem::path(home) / "Documents" / "Signal";
                    }
                    else
                    {
                        recordingsPath = std::filesystem::current_path() / "documents" / "signal";
                    }

                    LaunchDefaultFileManager(recordingsPath);
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Create Template MDU"))
                {
                    MDU::CreateTemplateMDU(GlobalModuleLoader.GetTemplatePath());
                }
                ImGui::Separator();
                if (ImGui::BeginMenu("Set MDU Search Paths"))
                {
                    std::string newPath;
                    if (ImGui::InputText("New Path", &newPath, ImGuiInputTextFlags_EnterReturnsTrue))
                    {
                        if (!newPath.empty())
                        {
                            AddMduSearchPathAndPersist(newPath);
                        }
                        else
                        {
                            Console::AppendConsoleLine("[warning] Cannot add empty path to MDU search paths.");
                        }
                    }
                    ImGui::Separator();
                    const auto &searchPaths = GlobalModuleLoader.GetSearchPaths();
                    if (searchPaths.empty())
                    {
                        ImGui::TextDisabled("No search paths set");
                    }
                    else
                    {
                        for (const auto &path : searchPaths)
                        {
                            if (ImGui::BeginMenu(path.c_str()))
                            {
                                if (ImGui::MenuItem("Remove"))
                                {
                                    MDU::RemoveMduSearchPathFromSettingsFile(path);
                                }

                                ImGui::EndMenu();
                            }
                        }
                    }
                    ImGui::EndMenu();
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Help"))
            {
                if (ImGui::MenuItem("Documentation"))
                {
                    std::string command = "xdg-open https://github.com/Slsickslider1967/Signal/wiki";
                    std::system(command.c_str());
                }
                if (ImGui::MenuItem("GitHub Repository"))
                {
                    std::string command = "xdg-open https://github.com/Slsickslider1967/Signal";
                    std::system(command.c_str());
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Console"))
                {
                    GlobalShowDebugConsole = true;
                }
                ImGui::EndMenu();
            }
            if (IsRecording)
            {
                ImDrawList *draw_list = ImGui::GetWindowDrawList();
                ImVec2 bar_pos = ImGui::GetWindowPos();
                ImVec2 bar_size = ImGui::GetWindowSize();
                ImVec2 circle_center = ImVec2(bar_pos.x + bar_size.x - 20.0f, bar_pos.y + bar_size.y / 2.0f);
                float radius = 7.0f;
                draw_list->AddCircleFilled(circle_center, radius, IM_COL32(255, 0, 0, 255));
            }
            ImGui::EndMainMenuBar();
        }

        Debug();
    }

    // Draw the node editor for one rack, including modules, links, and context actions.
    void DrawRackEditor(Rack &rack)
    {
        ImNodes::BeginNodeEditor();
        if (ImNodes::IsEditorHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            if (std::find(SelectedRackIDs.begin(), SelectedRackIDs.end(), rack.ID) == SelectedRackIDs.end())
            {
                SelectedRackIDs.push_back(rack.ID);
            }
            SelectedModuleID = -1;
        }

        bool openContextMenu = ImNodes::IsEditorHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right);

        // Draw each module node with pins and metadata.
        for (auto moduleIt = rack.DynamicModules.begin(); moduleIt != rack.DynamicModules.end(); ++moduleIt)
        {
            DynamicModule &module = *moduleIt;

            ImNodes::PushAttributeFlag(ImNodesAttributeFlags_EnableLinkDetachWithDragClick);
            ImNodes::BeginNode(module.ID);

            AddPins(module);

            ImGui::Text("MDU: %s", module.Name.c_str());
            if (!module.Metadata.Author.empty())
            {
                ImGui::TextDisabled("by %s", module.Metadata.Author.c_str());
            }

            ImNodes::EndNode();
            ImNodes::PopAttributeFlag();

            if (ImNodes::IsNodeSelected(module.ID))
            {
                SelectedModuleID = module.ID;
            }
        }

        DrawLinks(rack);
        ImNodes::EndNodeEditor();

        if (openContextMenu && !ShowModuleDetails && SelectedModuleID == -1)
        {
            ImGui::OpenPopup("RackContextMenu");
        }

        CreateLinks(rack);

        int deletedLinkID = 0;
        if (ImNodes::IsLinkDestroyed(&deletedLinkID))
        {
            rack.Links.erase(
                std::remove_if(rack.Links.begin(), rack.Links.end(),
                               [deletedLinkID](const Link &link)
                               { return link.ID == deletedLinkID; }),
                rack.Links.end());
        }
    }

    // Thin wrapper to expose the shared popup helper through Draw namespace.
    bool PopUpTool(Rack &rack)
    {
        return ::PopUpTool(rack);
    }

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
}