#include <iostream>
#include "imgui.h"
#include "imgui-knobs.h"
#include "../../include/Functions/ImGuiUtil.h"
#include "../../include/Module.h"
#include "../../include/Output.h"
#include "../../include/Functions/Audio.h"

namespace Output
{
    // --Variables--
    static float outputLevel = 0.8f;
    static float *audioBuffer = nullptr;
    static int bufferSize = 5120;

    // --Function--
    void MainImGui()
    {
        ImGui::Text("Output Controls:");
        ImGuiKnobs::Knob("Volume", &outputLevel, 0.0f, 1.0f, 0.01f, "%.2f", ImGuiKnobVariant_Wiper);
    }

    void DrawModuleEditor(Module &module, bool &requestRemove)
    {
        if (module.outputConfig.outputLevel < 0.0f)
        {
            module.outputConfig.outputLevel = 0.0f;
        }

        if (module.outputConfig.outputLevel > 1.0f)
        {
            module.outputConfig.outputLevel = 1.0f;
        }

        outputLevel = module.outputConfig.outputLevel;

        ImGui::Text("MASTER OUTPUT");
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0.0f, 6.0f));
        if (ImGui::BeginTable("OUT_ROW", 3, ImGuiTableFlags_SizingStretchSame))
        {
            ImGui::TableNextColumn();
            ImGui::TableNextColumn();
            ImGui::Text("LEVEL");
            if (ImGuiKnobs::Knob("##out_level", &outputLevel, 0.0f, 1.0f, 0.01f, "%.2f", ImGuiKnobVariant_WiperDot, 0.0f, ImGuiKnobFlags_NoTitle))
            {
                module.outputConfig.outputLevel = outputLevel;
            }
            ImGui::TableNextColumn();

            ImGui::EndTable();
        }

        ImGui::Dummy(ImVec2(0.0f, 4.0f));
        ImGui::Text("Signal to DAC");
        ImGui::ProgressBar(outputLevel, ImVec2(-1.0f, 12.0f));

        if (ImGui::Button("Remove Output Module", ImVec2(-1.0f, 0.0f)))
        {
            requestRemove = true;
        }
    }

    void ProcessAudio(float *inputBuffer, int numSamples)
    {
        for (int sampleIndex = 0; sampleIndex < numSamples; sampleIndex++)
        {
            inputBuffer[sampleIndex] *= outputLevel;
        }
    }

    void Initialize()
    {
        audioBuffer = new float[bufferSize];
    }

    void Cleanup()
    {
        delete[] audioBuffer;
    }
}