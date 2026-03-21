// #include <iostream>

// #include "imgui.h"

// #include "../../include/Functions/Draw.h"
// #include "../../include/Functions/Module.h"
// #include "../../include/Functions/ConsoleHandling.h"
// #include "../../include/MDU/ModuleLoader.h"

// namespace Draw
// {
//     void DrawTopBar()
//     {
//         if (ImGui::BeginMainMenuBar())
//         {
//             if (ImGui::BeginMenu("Rack"))
//             {
//                 if (ImGui::MenuItem("Add Rack"))
//                 {
//                     CreateRack(std::string("Rack ") + std::to_string(NextRackID));
//                 }
//                 if (ImGui::BeginMenu("Remove Rack"))
//                 {
//                     if (Racks.empty())
//                     {
//                         ImGui::TextDisabled("No racks to remove");
//                     }
//                     else
//                     {
//                         int rackIDToDelete = -1;
//                         for (const auto &rack : Racks)
//                         {
//                             std::string label = "Rack #" + std::to_string(rack.ID) + ": " + rack.Name;
//                             if (ImGui::MenuItem(label.c_str()))
//                             {
//                                 rackIDToDelete = rack.ID;
//                             }
//                         }
//                         if (rackIDToDelete != -1)
//                         {
//                             DeleteRack(rackIDToDelete);
//                         }
//                     }
//                     ImGui::EndMenu();
//                 }
//                 ImGui::EndMenu();
//             }
//             if (ImGui::BeginMenu("Modules"))
//             {
//                 if (ImGui::BeginMenu("Add Module to Selected Rack"))
//                 {
//                     Rack *selectedRack = FindRackByID(SelectedRackID);
//                     if (selectedRack == nullptr)
//                     {
//                         ImGui::TextDisabled("Select a Rack First");
//                     }
//                     else
//                     {
//                         DrawAvailableModulesChild(*selectedRack);
//                     }
//                     ImGui::EndMenu();
//                 }
//                 ImGui::EndMenu();
//             }
//             if (ImGui::BeginMenu("File"))
//             {
//                 if (ImGui::MenuItem("Open template folder"))
//                 {
//                     LaunchDefaultFileManager(GModuleLoader.GetTemplatePath());
//                 }
//                 if (ImGui::MenuItem("Create template MDU"))
//                 {
//                     MDU::CreateTemplateMDU(GModuleLoader.GetTemplatePath());
//                 }
//                 ImGui::Separator();
//                 if (ImGui::BeginMenu("Set MDU Search Paths"))
//                 {
//                     std::string newPath;
//                     if (ImGui::InputText("New Path", &newPath, ImGuiInputTextFlags_EnterReturnsTrue))
//                     {
//                         if (!newPath.empty())
//                         {
//                             GModuleLoader.AddSearchPath(newPath);
//                         }
//                         else
//                         {
//                             Console::AppendConsoleLine("[warning] Cannot add empty path to MDU search paths.");
//                         }
//                     }
//                     ImGui::Separator();
//                     const auto &searchPaths = GModuleLoader.GetSearchPaths();
//                     if (searchPaths.empty())
//                     {
//                         ImGui::TextDisabled("No search paths set");
//                     }
//                     else
//                     {
//                         for (const auto &path : searchPaths)
//                         {
//                             ImGui::Text("%s", path.c_str());
//                         }
//                     }
//                     ImGui::EndMenu();
//                 }
//                 ImGui::EndMenu();
//             }
//             if (ImGui::BeginMenu("Help"))
//             {
//                 if (ImGui::MenuItem("Documentation"))
//                 {
//                     std::string command = "xdg-open https://github.com/Slsickslider1967/Signal/wiki";
//                     std::system(command.c_str());
//                 }
//                 if (ImGui::MenuItem("GitHub Repository"))
//                 {
//                     std::string command = "xdg-open https://github.com/Slsickslider1967/Signal";
//                     std::system(command.c_str());
//                 }
//                 ImGui::Separator();
//                 if (ImGui::MenuItem("Console"))
//                 {
//                     GShowDebugConsole = true;
//                 }
//                 ImGui::EndMenu();
//             }
//             ImGui::EndMainMenuBar();
//         }

//         Debug();
//     }
// }