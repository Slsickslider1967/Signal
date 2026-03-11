#include <iostream>
#include "../../include/Functions/Window.h"
#include "../../include/WaveForm.h"
#include "../../include/Functions/Audio.h"
#include "../../include/Functions/ImGuiUtil.h"
#include "../../include/VoltageControllFilter.h"
#include "../../include/Functions/CV.h"
#include "../../include/Module.h"
#include "imgui.h"
#include "imgui-knobs.h"
#include "imgui_internal.h"
#include <list>
#include <vector>
#include <thread>
#include <chrono>
#include <algorithm>
#include <string>
#include <GLFW/glfw3.h>

namespace WaveFormGen
{
    // --Main Variables--
    int NextWaveID = 1;
    std::list<WaveForm> WaveForms;

    void InitializeVCOWaveForm(WaveForm& wave, int moduleID)
    {
        wave.WaveID = moduleID;
        wave.Enabled = true;
        wave.OpenWindow = true;
        wave.RequestDockBelow = true;
        wave.Type = Sine;
        wave.coarseTune = 440.0f;
        wave.Amplitude = 0.8f;
        wave.SampleRate = 44100;
        wave.vOctCV = 0.5f;
        wave.linearFMCV = 0.5f;
        wave.pwmCV = 0.5f;
        wave.vRange = WaveForm::Bipolar10V;
        wave.fmDepth = 0.0f;
    }

    std::map<int, std::vector<float>> GenerateLFOOutputs(std::list<Module>& modules, int numSamples)
    {
        std::map<int, std::vector<float>> lfoOutputs;

        for (auto &module : modules)
        {
            if (!module.Active || module.Type != MODULE_LFO)
                continue;

            std::vector<float> lfoBuffer(numSamples);
            GetWaveFormData(module.lfoConfig.waveform, lfoBuffer.data(), numSamples, 0);
            lfoOutputs[module.ID] = std::move(lfoBuffer);
        }

        return lfoOutputs;
    }

    std::map<int, std::vector<float>> BuildNormalizedCVInputs(
        const std::list<Module>& modules,
        const std::vector<Link>& links,
        const std::map<int, std::vector<float>>& lfoOutputs)
    {
        std::map<int, std::vector<float>> normalizedCVInputs;

        for (const auto &module : modules)
        {
            if (!module.Active)
                continue;

            for (const auto &link : links)
            {
                if (link.EndModuleID != module.ID)
                    continue;

                const auto lfoIt = lfoOutputs.find(link.StartModuleID);
                if (lfoIt == lfoOutputs.end())
                    continue;

                std::vector<float> normalized = lfoIt->second;
                for (float &sample : normalized)
                {
                    sample = CV::ClampCV((sample + 1.0f) * 0.5f, {0.0f, 1.0f});
                }
                normalizedCVInputs[module.ID] = std::move(normalized);
                break;
            }
        }

        return normalizedCVInputs;
    }

    void DrawWaveFormEditor(WaveForm& wave)
    {
        double maxVoltage = 10.0;
        switch (wave.vRange)
        {
        case WaveForm::Bipolar5V:
            maxVoltage = 5.0;
            break;
        case WaveForm::Bipolar10V:
            maxVoltage = 10.0;
            break;
        case WaveForm::Bipolar12V:
            maxVoltage = 12.0;
            break;
        case WaveForm::Bipolar15V:
            maxVoltage = 15.0;
            break;
        default:
            maxVoltage = 10.0;
            break;
        }

        ImGui::Spacing();

        ImGui::Text("TUNING");
        if (ImGui::BeginTable("VCO_TUNING_ROW", 3, ImGuiTableFlags_SizingStretchSame))
        {
            ImGui::TableNextColumn();
            ImGui::Text("COARSE");
            ImGuiKnobs::Knob("##coarse_tune", &wave.coarseTune, 20.0f, 2000.0f, 0.1f, "%.1f Hz", ImGuiKnobVariant_WiperDot, 0.0f, ImGuiKnobFlags_Logarithmic | ImGuiKnobFlags_NoTitle);

            ImGui::TableNextColumn();
            ImGui::Text("OCTAVE");
            ImGuiKnobs::KnobInt("##oct_tune", &wave.octave, -4, 4, 1.0f, "%d", ImGuiKnobVariant_Stepped, 0.0f, ImGuiKnobFlags_NoTitle, 9);

            ImGui::TableNextColumn();
            ImGui::Text("FINE");
            ImGuiKnobs::Knob("##fine_tune", &wave.fineTune, -100.0f, 100.0f, 0.1f, "%.1f ct", ImGuiKnobVariant_Wiper, 0.0f, ImGuiKnobFlags_NoTitle);

            ImGui::EndTable();
        }
        ImGui::Text("Output Frequency: %.2f Hz", wave.Frequency);

        ImGui::Spacing();

        ImGui::Text("WAVEFORM");
        const char *waveNames[] = {"Sine", "Square", "Sawtooth", "Triangle", "Pulse", "Noise"};
        WaveType waveVals[] = {Sine, Square, Sawtooth, Triangle, Pulse, Noise};

        for (int waveTypeIndex = 0; waveTypeIndex < 6; waveTypeIndex++)
        {
            bool isSelected = (wave.Type == waveVals[waveTypeIndex]);
            if (isSelected)
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2, 0.7, 0.2, 1.0));

            if (ImGui::Button(waveNames[waveTypeIndex], ImVec2(80, 0)))
            {
                wave.Type = waveVals[waveTypeIndex];
            }

            if (isSelected)
                ImGui::PopStyleColor();
            if (waveTypeIndex < 5)
                ImGui::SameLine();
        }
        ImGui::Text("Current Output: %.3f V", wave.currentVoltageOut);

        ImGui::Spacing();

        ImGui::Text("PITCH CV");
        if (ImGui::BeginTable("VCO_PITCH_ROW", 2, ImGuiTableFlags_SizingStretchSame))
        {
            ImGui::TableNextColumn();
            ImGui::Text("V/OCT IN");
            ImGuiKnobs::Knob("##voct", &wave.vOctCV, 0.0f, 1.0f, 0.01f, "%.2f", ImGuiKnobVariant_Wiper, 0.0f, ImGuiKnobFlags_NoTitle);

            ImGui::TableNextColumn();
            ImGui::Text("FM IN");
            ImGuiKnobs::Knob("##fm_input", &wave.linearFMCV, 0.0f, 1.0f, 0.01f, "%.2f", ImGuiKnobVariant_Wiper, 0.0f, ImGuiKnobFlags_NoTitle);

            ImGui::EndTable();
        }
        double vOctVolt = NormalizedToVoltage(wave.vOctCV, wave.vRange);
        ImGui::Text("  Voltage: %+.3f V  (%.2f octaves shift)", vOctVolt, vOctVolt);

        ImGui::Spacing();

        ImGui::Text("FREQUENCY MOD");
        if (ImGui::BeginTable("VCO_FM_ROW", 2, ImGuiTableFlags_SizingStretchSame))
        {
            ImGui::TableNextColumn();
            ImGui::Text("FM DEPTH");
            ImGuiKnobs::Knob("##fmdepth", &wave.fmDepth, 0.0f, 1.0f, 0.01f, "%.2f", ImGuiKnobVariant_Wiper, 0.0f, ImGuiKnobFlags_NoTitle);

            ImGui::TableNextColumn();
            ImGui::Text("OUTPUT LVL");
            ImGuiKnobs::Knob("##out_lvl", &wave.Amplitude, 0.0f, 1.0f, 0.01f, "%.2f", ImGuiKnobVariant_Wiper, 0.0f, ImGuiKnobFlags_NoTitle);

            ImGui::EndTable();
        }
        ImGui::Text("  FM Active: %s", wave.fmDepth > 0.0f ? "Yes" : "No");

        ImGui::Spacing();

        if (wave.Type == Pulse)
        {
            ImGui::Text("Pulse Width Modulation");
            ImGuiKnobs::Knob("##pwm", &wave.pwmCV, 0.05f, 0.95f, 0.01f, "%.2f", ImGuiKnobVariant_WiperDot, 0.0f, ImGuiKnobFlags_NoTitle);
            ImGui::Text("  Pulse Width: %.1f%%", wave.pwmCV * 100.0f);
            ImGui::Spacing();
        }

        ImGui::Text("Sync Control");
        ImGui::Checkbox("Hard Sync Enable##sync", &wave.syncInput);
        ImGui::Text("  Sync: %s", wave.syncInput ? "Active" : "Off");

        ImGui::Spacing();

        ImGui::Text("  Level: %.1f%%", wave.Amplitude * 100.0f);

        const char *vRanges[] = {"±5V", "±10V", "±12V", "±15V"};
        int selectedVoltageRangeIndex = (int)wave.vRange;
        if (ImGui::Combo("Voltage Range##vrange", &selectedVoltageRangeIndex, vRanges, IM_ARRAYSIZE(vRanges)))
        {
            wave.vRange = (WaveForm::VoltageRange)selectedVoltageRangeIndex;
        }
        ImGui::Text("  Range: %.1f V to +%.1f V", -maxVoltage, maxVoltage);

        ImGui::Spacing();

        ImGui::Text("Output Meter");
        float meterPos = (wave.currentVoltageOut / maxVoltage + 1.0f) * 0.5f;
        ImGui::ProgressBar(meterPos, ImVec2(-1, 15), "");
        ImGui::Text("  Voltage: %.4f V", wave.currentVoltageOut);

        ImGui::Spacing();

        ImGui::Text("System");
        ImGui::Checkbox("Enabled##enable", &wave.Enabled);
        ImGui::SameLine();
        ImGui::Text("Sample Rate:");
        ImGui::SameLine();
        ImGui::SliderInt("##sr", &wave.SampleRate, 8000, 96000);

        ImGui::Spacing();

        ImGuiUtil::Oscilloscope(wave, "Waveform Display");

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
    }

    void DrawModuleEditor(Module &module, bool &requestRemove)
    {
        ImGui::Text("Oscillator Module");
        ImGui::Separator();

        DrawWaveFormEditor(module.vcoConfig.waveform);

        if (ImGui::Button("Remove VCO Module", ImVec2(-1.0f, 0.0f)))
        {
            requestRemove = true;
        }
    }

    void MainImgui()
    {
        ImGui::Text("VCO Management");
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 1.0f, 1.0f),
            "Create VCOs via Rack Manager:");
        ImGui::BulletText("Click 'Add Rack' to create a new rack");
        ImGui::BulletText("Select a rack and click 'Add Module' -> 'VCO'");
        ImGui::BulletText("Click on the VCO module to edit its waveform");
    }
}
