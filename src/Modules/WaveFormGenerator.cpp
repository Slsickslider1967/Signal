#include <iostream>
#include "../../include/Functions/Window.h"
#include "../../include/WaveForm.h"
#include "../../include/Functions/Audio.h"
#include "../../include/Functions/ImGuiUtil.h"
#include "../../include/VoltageControllFilter.h"
#include "imgui.h"
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

    // --ImGui UI functions exposed as an "addon" --
    void DisplayWaveForm();

    void MainImgui()
    {
        DisplayWaveForm();

        if (ImGui::Button("Add VCO"))
        {
            WaveForm newWave;
            newWave.WaveID = NextWaveID++;
            newWave.Enabled = true;
            newWave.OpenWindow = true;
            newWave.RequestDockBelow = true;
            newWave.Type = Sine;
            newWave.coarseTune = 440.0f; // A4
            newWave.Amplitude = 0.8f;
            newWave.SampleRate = 44100;
            newWave.vOctCV = 0.5f; // 0V
            newWave.linearFMCV = 0.5f;
            newWave.pwmCV = 0.5f;
            newWave.vRange = WaveForm::Bipolar10V; // Eurorack standard
            newWave.fmDepth = 0.0f;
            WaveForms.push_back(newWave);
        }

        // Audio now routed through rack system - no direct output
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), 
            "Note: Audio only outputs via rack Output module");
    }

    void DisplayWaveForm()
    {
        if (WaveForms.empty())
        {
            ImGui::Text("No VCOs. Click 'Add VCO' to create an oscillator.");
            return;
        }

        for (auto waveIt = WaveForms.begin(); waveIt != WaveForms.end();)
        {
            WaveForm &wave = *waveIt;
            ImGui::PushID(wave.WaveID);

            // === VCO HEADER ===
            ImGui::Text("VCO #%d", wave.WaveID);
            ImGui::SameLine(100);
            if (ImGui::Button("Delete", ImVec2(60, 0)))
            {
                waveIt = WaveForms.erase(waveIt);
                NextWaveID--;
                ImGui::PopID();
                continue;
            }

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
            ImGui::Separator();

            // === TUNING SECTION ===
            ImGui::Text("TUNING");
            ImGui::SliderFloat("Coarse Frequency##tune", &wave.coarseTune, 20.0f, 2000.0f);
            ImGui::SliderInt("Octave##tune", &wave.octave, -4, 4);
            ImGui::SliderFloat("Fine Tune##tune", &wave.fineTune, -100.0f, 100.0f);
            ImGui::Text("Output Frequency: %.2f Hz", wave.Frequency);

            ImGui::Spacing();

            // === WAVEFORM SELECTION ===
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

            // === V/OCT SECTION ===
            ImGui::Text("V/OCT (Pitch Control)");
            ImGui::SliderFloat("V/Oct CV##voct", &wave.vOctCV, 0.0f, 1.0f);
            double vOctVolt = NormalizedToVoltage(wave.vOctCV, wave.vRange);
            ImGui::Text("  Voltage: %+.3f V  (%.2f octaves shift)", vOctVolt, vOctVolt);

            ImGui::Spacing();

            // === FM SECTION ===
            ImGui::Text("Frequency Modulation");
            ImGui::SliderFloat("FM Input##fm", &wave.linearFMCV, 0.0f, 1.0f);
            ImGui::SliderFloat("FM Depth##fmdepth", &wave.fmDepth, 0.0f, 1.0f);
            ImGui::Text("  FM Active: %s", wave.fmDepth > 0.0f ? "Yes" : "No");

            ImGui::Spacing();

            // === PWM SECTION (if Pulse) ===
            if (wave.Type == Pulse)
            {
                ImGui::Text("Pulse Width Modulation");
                ImGui::SliderFloat("PWM##pwm", &wave.pwmCV, 0.05f, 0.95f);
                ImGui::Text("  Pulse Width: %.1f%%", wave.pwmCV * 100.0f);
                ImGui::Spacing();
            }

            // === SYNC SECTION ===
            ImGui::Text("Sync Control");
            ImGui::Checkbox("Hard Sync Enable##sync", &wave.syncInput);
            ImGui::Text("  Sync: %s", wave.syncInput ? "Active" : "Off");

            ImGui::Spacing();

            // === OUTPUT SECTION ===
            ImGui::Text("Output Controls");
            ImGui::SliderFloat("Output Level##lvl", &wave.Amplitude, 0.0f, 1.0f);
            ImGui::Text("  Level: %.1f%%", wave.Amplitude * 100.0f);

            const char *vRanges[] = {"±5V", "±10V", "±12V", "±15V"};
            int selectedVoltageRangeIndex = (int)wave.vRange;
            if (ImGui::Combo("Voltage Range##vrange", &selectedVoltageRangeIndex, vRanges, IM_ARRAYSIZE(vRanges)))
            {
                wave.vRange = (WaveForm::VoltageRange)selectedVoltageRangeIndex;
            }
            ImGui::Text("  Range: %.1f V to +%.1f V", -maxVoltage, maxVoltage);

            ImGui::Spacing();

            // === METERING ===
            ImGui::Text("Output Meter");
            float meterPos = (wave.currentVoltageOut / maxVoltage + 1.0f) * 0.5f;
            ImGui::ProgressBar(meterPos, ImVec2(-1, 15), "");
            ImGui::Text("  Voltage: %.4f V", wave.currentVoltageOut);

            ImGui::Spacing();

            // === SYSTEM SETTINGS ===
            ImGui::Text("System");
            ImGui::Checkbox("Enabled##enable", &wave.Enabled);
            ImGui::SameLine();
            ImGui::Text("Sample Rate:");
            ImGui::SameLine();
            ImGui::SliderInt("##sr", &wave.SampleRate, 8000, 96000);

            ImGui::Spacing();

            // === OSCILLOSCOPE ===
            ImGuiUtil::Oscilloscope(wave, "Waveform Display");

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::PopID();
            ++waveIt;
        }
    }
}
