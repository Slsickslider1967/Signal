// #include <iostream>

// #include "imgui.h"

// #include "Draw/Draw.h"
// #include "WaveForm.h"

// namespace DrawSpace
// {
//     int SelectedModuleID = -1;
//     int SelectedRackID = -1;
//     int NextRackID = 1;
//     int NextModuleID = 1;

//     bool GModuleLoader;

//     std::list<Rack> Racks;
//     std::list<Module> Modules;

//     class Draw
//     {
//     public:
//         // Constructor: sets the selected module ID
//         Draw(int selectedModuleID)
//         {
//             SelectedModuleID = selectedModuleID;
//         }

//         void DrawTopBar()
//         {

//             if (ImGui::BeginMainMenuBar())
//             {
//                 if (ImGui::BeginMenu("Rack"))
//                 {
//                     if (ImGui::MenuItem("Add Rack"))
//                     {
//                         CreateRack(std::string("Rack ") + std::to_string(NextRackID));
//                     }
//                     if (ImGui::BeginMenu("Remove Rack"))
//                     {
//                         if (Racks.empty())
//                         {
//                             ImGui::TextDisabled("No racks to remove");
//                         }
//                         else
//                         {
//                             int rackIDToDelete = -1;
//                             for (const auto &rack : Racks)
//                             {
//                                 std::string label = "Rack #" + std::to_string(rack.ID) + ": " + rack.Name;
//                                 if (ImGui::MenuItem(label.c_str()))
//                                 {
//                                     rackIDToDelete = rack.ID;
//                                 }
//                             }
//                             if (rackIDToDelete != -1)
//                             {
//                                 DeleteRack(rackIDToDelete);
//                             }
//                         }
//                         ImGui::EndMenu();
//                     }
//                     ImGui::EndMenu();
//                 }
//                 if (ImGui::BeginMenu("Modules"))
//                 {
//                     if (ImGui::BeginMenu("Add Module to Selected Rack"))
//                     {
//                         Rack *selectedRack = FindRackByID(SelectedRackID);
//                         if (selectedRack == nullptr)
//                         {
//                             ImGui::TextDisabled("Select a Rack First");
//                         }
//                         else
//                         {
//                             DrawAvailableModulesChild(*selectedRack);
//                         }
//                         ImGui::EndMenu();
//                     }
//                     ImGui::EndMenu();
//                 }
//                 if (ImGui::BeginMenu("File"))
//                 {
//                     if (ImGui::MenuItem("Open template folder"))
//                     {
//                         LaunchDefaultFileManager(GModuleLoader.GetTemplatePath());
//                     }
//                     if (ImGui::MenuItem("Create template MDU"))
//                     {
//                         MDU::CreateTemplateMDU(GModuleLoader.GetTemplatePath());
//                     }
//                     ImGui::Separator();
//                     if (ImGui::BeginMenu("Set MDU Search Paths"))
//                     {
//                         std::string newPath;
//                         if (ImGui::InputText("New Path", &newPath, ImGuiInputTextFlags_EnterReturnsTrue))
//                         {
//                             if (!newPath.empty())
//                             {
//                                 GModuleLoader.AddSearchPath(newPath);
//                             }
//                             else
//                             {
//                                 Console::AppendConsoleLine("[warning] Cannot add empty path to MDU search paths.");
//                             }
//                         }
//                         ImGui::Separator();
//                         const auto &searchPaths = GModuleLoader.GetSearchPaths();
//                         if (searchPaths.empty())
//                         {
//                             ImGui::TextDisabled("No search paths set");
//                         }
//                         else
//                         {
//                             for (const auto &path : searchPaths)
//                             {
//                                 ImGui::Text("%s", path.c_str());
//                             }
//                         }
//                         ImGui::EndMenu();
//                     }
//                     ImGui::EndMenu();
//                 }
//                 if (ImGui::BeginMenu("Help"))
//                 {
//                     if (ImGui::MenuItem("Documentation"))
//                     {
//                         std::string command = "xdg-open https://github.com/Slsickslider1967/Signal/wiki";
//                         std::system(command.c_str());
//                     }
//                     if (ImGui::MenuItem("GitHub Repository"))
//                     {
//                         std::string command = "xdg-open https://github.com/Slsickslider1967/Signal";
//                         std::system(command.c_str());
//                     }
//                     ImGui::Separator();
//                     if (ImGui::MenuItem("Console"))
//                     {
//                         GShowDebugConsole = true;
//                     }
//                     ImGui::EndMenu();
//                 }
//                 ImGui::EndMainMenuBar();
//             }

//             Debug();
//         }

//         void Debug()
//         {
//             if (!GShowDebugConsole)
//             {
//                 return;
//             }

//             ImGui::SetNextWindowSize(ImVec2(900, 420), ImGuiCond_FirstUseEver);
//             ImGui::Begin("Debug Console", &GShowDebugConsole);

//             ImGui::SameLine();

//             ImGui::SameLine();
//             if (ImGui::Button("Clear Output"))
//             {
//                 Console::ClearConsoleOutput();
//             }

//             ImGui::SameLine();
//             ImGui::Checkbox("Auto-scroll", Console::AutoScrollFlag());

//             ImGui::Separator();

//             if (ImGui::BeginChild("ConsoleOutput", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar))
//             {
//                 std::vector<std::string> lines = Console::GetConsoleLinesSnapshot();
//                 for (const std::string &line : lines)
//                 {
//                     ImGui::TextUnformatted(line.c_str());
//                 }

//                 if (*Console::AutoScrollFlag() && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 10.0f)
//                 {
//                     ImGui::SetScrollHereY(1.0f);
//                 }
//             }
//             ImGui::EndChild();

//             ImGui::End();
//         }
//     };
// }