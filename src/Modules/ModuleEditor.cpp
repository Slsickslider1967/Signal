#include "imgui.h"

#include <cstring>
#include <string>

#include "../../include/Module.h"
#include "../../include/ModuleEditor.h"
#include "../../include/WaveForm.h"
#include "../../include/LowFrequencyOscillator.h"
#include "../../include/VoltageControllFilter.h"
#include "../../include/Voltage-ControlledAmplifier.h"
#include "../../include/Output.h"

static const char *ModuleTypeToStringLocal(ModuleType type)
{
    if (type == MODULE_VCO)
    {
        return "VCO";
    }

    if (type == MODULE_LFO)
    {
        return "LFO";
    }

    if (type == MODULE_VCF)
    {
        return "VCF";
    }

    if (type == MODULE_VCA)
    {
        return "VCA";
    }

    if (type == MODULE_OUTPUT)
    {
        return "Output";
    }

    return "Unknown";
}

static void DrawCommonModuleEditorHeader(Module &module)
{
    char moduleNameBuffer[128];
    strncpy(moduleNameBuffer, module.Name.c_str(), sizeof(moduleNameBuffer));
    moduleNameBuffer[sizeof(moduleNameBuffer) - 1] = '\0';

    if (ImGui::InputText("Module Name", moduleNameBuffer, sizeof(moduleNameBuffer)))
    {
        module.Name = moduleNameBuffer;
    }

    ImGui::Checkbox("Module Active", &module.Active);

    ImGui::Separator();
}

bool DrawModuleEditorWindow(Module &module, bool &windowOpen, bool &requestRemove)
{
    ImGui::SetNextWindowSize(ImVec2(500.0f, 720.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(380.0f, 420.0f), ImVec2(1400.0f, 1600.0f));
    std::string windowTitle = std::string("Module Panel: ") + ModuleTypeToStringLocal(module.Type) + " #" + std::to_string(module.ID);
    if (!ImGui::Begin(windowTitle.c_str(), &windowOpen))
    {
        ImGui::End();
        return false;
    }

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.11f, 0.12f, 0.13f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10.0f, 8.0f));

    if (ImGui::BeginChild("ModuleFaceplate", ImVec2(0.0f, 0.0f), true))
    {
        ImGui::Text("%s", module.Name.c_str());
        ImGui::SameLine();
        ImGui::TextDisabled("[%s]", ModuleTypeToStringLocal(module.Type));

        DrawCommonModuleEditorHeader(module);

        ImGui::Text("I/O");
        ImGui::Separator();
        ImGui::Text("Input Jacks: %d", module.InPins);
        ImGui::SameLine();
        ImGui::Text("Output Jacks: %d", module.OutPins);

        if (module.Type == MODULE_VCO)
        {
            ImGui::PushID(module.ID);
            WaveFormGen::DrawModuleEditor(module, requestRemove);
            ImGui::PopID();
        }
        else if (module.Type == MODULE_LFO)
        {
            ImGui::PushID(module.ID);
            LFO::DrawModuleEditor(module, requestRemove);
            ImGui::PopID();
        }
        else if (module.Type == MODULE_VCF)
        {
            ImGui::PushID(module.ID);
            VCF::DrawModuleEditor(module, requestRemove);
            ImGui::PopID();
        }
        else if (module.Type == MODULE_VCA)
        {
            ImGui::PushID(module.ID);
            VCA::DrawModuleEditor(module, requestRemove);
            ImGui::PopID();
        }
        else if (module.Type == MODULE_OUTPUT)
        {
            ImGui::PushID(module.ID);
            Output::DrawModuleEditor(module, requestRemove);
            ImGui::PopID();
        }
    }
    ImGui::EndChild();

    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor();

    ImGui::End();
    return true;
}