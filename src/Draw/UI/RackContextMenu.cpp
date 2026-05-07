#include <iostream>

#include "imgui.h"
#include "imgui_stdlib.h"

#include "../../include/Audio/UI/RackContextMenu.h"
#include "../../include/Functions/ConsoleHandling.h"
#include "../../include/Draw/Draw.h"

bool OpenContextMenu = false;

// Public
void RackContextMenu::Show()
{
    RackContextMenu().CreateContextMenu();
    OpenContextMenu = true;
}

void RackContextMenu::Close()
{
    std::cout << "Closing rack context menu..." << std::endl;
    OpenContextMenu = false;
}

// Private
void RackContextMenu::CreateContextMenu()
{
    while (OpenContextMenu)
    {
        // Opens the rack menu for selected racks
        if (ImGui::BeginPopupModal("RackContextMenu"))
        {
            // Determine currently selected rack
            Rack *selectedRack = nullptr;
            if (!SelectedRackIDs.empty())
            {
                selectedRack = Draw::FindRackByID(SelectedRackIDs.back());
            }

            if (selectedRack == nullptr)
            {
                ImGui::TextDisabled("Select a Rack First");
            }
            else
            {
                if (ImGui::InputText("Rack Name", &selectedRack->Name, ImGuiInputTextFlags_EnterReturnsTrue))
                {
                    ImGui::CloseCurrentPopup();
                }

                ImGui::Separator();

                ImGui::Text("Rack Settings");

                ImGui::SameLine();
                RackContextMenu().VoltageRange(*selectedRack);

                ImGui::Separator();

                ImGui::Text("Available Modules");
                Draw::DrawAvailableModulesChild(*selectedRack);

                ImGui::Separator();

                if (ImGui::Selectable("Remove Rack"))
                {
                    DeleteRack(selectedRack->ID);
                    ImGui::CloseCurrentPopup();
                }
            }

            RackContextMenu().Input();

            ImGui::EndPopup();
        }
    }
}

bool RackContextMenu::RemoveRack()
{
    bool requestDelete = false;

    if (ImGui::Selectable("Remove Rack"))
    {
        requestDelete = true;
        ImGui::CloseCurrentPopup();
    }

    return requestDelete;
}

void RackContextMenu::VoltageRange(Rack &rack)
{
    const char *rangeLabels[] = {"-5V to +5V", "-10V to +10V", "-12V to +12V", "-15V to +15V"};

    if (ImGui::BeginMenu("Voltage Range"))
    {
        for (int i = 0; i < 4; ++i)
        {
            if (ImGui::MenuItem(rangeLabels[i], nullptr, rack.VoltageRange == i))
            {
                rack.VoltageRange = i;
                Console::AppendConsoleLine(std::string("[info] Voltage range changed to: ") + rangeLabels[i]);
            }
        }
        ImGui::EndMenu();
    }

    ImGui::Text("Currently: %s", (rack.VoltageRange >= 0 && rack.VoltageRange < 4) ? rangeLabels[rack.VoltageRange] : "Unknown");
}

void RackContextMenu::Input()
{

    if (ImGui::IsKeyPressed(ImGuiKey_Escape))
    {
        ImGui::CloseCurrentPopup();
    }
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsWindowHovered())
    {
        ImGui::CloseCurrentPopup();
    }
}