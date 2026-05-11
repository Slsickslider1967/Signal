#include "imgui.h"
#include "imgui_stdlib.h"

#include "../../include/Draw/UI/RackContextMenu.h"
#include "../../include/Functions/ConsoleHandling.h"
#include "../../include/Draw/Draw.h"

static bool OpenContextMenu = false;
static bool OpenContextMenuRequested = false;
static const char *kRackContextMenuPopupName = "RackContextMenu";

// Public
void RackContextMenu::Show()
{
    OpenContextMenuRequested = true;
}

void RackContextMenu::Render()
{
    RackContextMenu().CreateContextMenu();
}

void RackContextMenu::Close()
{
    ImGui::CloseCurrentPopup();
    OpenContextMenu = false;
    OpenContextMenuRequested = false;
}

// Private
void RackContextMenu::CreateContextMenu()
{
    if (OpenContextMenuRequested)
    {
        ImGui::OpenPopup(kRackContextMenuPopupName);
        OpenContextMenu = true;
        OpenContextMenuRequested = false;
    }

    if (!OpenContextMenu)
    {
        return;
    }

    // Opens the rack menu for selected racks
    if (ImGui::BeginPopupModal(kRackContextMenuPopupName))
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
                Close();
            }

            ImGui::Separator();

            ImGui::Text("Rack Settings");

            ImGui::SameLine();
            RackContextMenu().VoltageRange(*selectedRack);

            ImGui::Separator();

            ImGui::Text("Available Modules");
            Draw::DrawAvailableModulesChild(*selectedRack);

            ImGui::Separator();

            if (RemoveRack())
            {
                DeleteRack(selectedRack->ID);
            }
        }

        RackContextMenu().Input();

        ImGui::EndPopup();
    }

    if (!ImGui::IsPopupOpen(kRackContextMenuPopupName))
    {
        OpenContextMenu = false;
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