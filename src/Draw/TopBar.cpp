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

#include "HandlerShared.h"
#include "Functions/ConsoleHandling.h"
#include "MDU/CreateMDU.h"
#include "Draw/Draw.h"
#include "Draw/ImGuiUtil.h"
#include "Audio/Record.h"


namespace Draw
{
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

                ImGui::Separator();

                if (ImGui::MenuItem("Save Racks (not implemented)"))
                {
                    Console::AppendConsoleLine("[info] Save Racks triggered (not implemented)");
                }

                if (ImGui::MenuItem("Load Racks (not implemented)"))
                {
                    Console::AppendConsoleLine("[info] Load Racks triggered (not implemented)");
                }

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

                if (ImGui::BeginMenu("Remove Selected Modules"))
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

                ImGui::Separator();

                if (ImGui::MenuItem("Remove selected Modules"))
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
}