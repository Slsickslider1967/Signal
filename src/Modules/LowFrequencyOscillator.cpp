#include <iostream>
#include <vector>
#include <list>
#include <cmath>

#include "imgui.h"
#include "imgui-knobs.h"
#include "../../include/Functions/ImGuiUtil.h"
#include "../../include/Functions/Module.h"
#include "../../include/LowFrequencyOscillator.h"
#include "../../include/Functions/Audio.h"

namespace LFO
{
    // --Variables--
    std::list<WaveForm> LFOs;
    int NextLFOID = 1;

    // --Prototypes--
    void WaveSettings();
    void AddLFO(const WaveForm &newLFO);
    void ProcessAudio(WaveForm& lfoWave, float *inputBuffer, float *outputBuffer, int numSamples);

    // --functions--
    void InitializeLFOWaveForm(WaveForm& lfo)
    {
        lfo.WaveID = 0;
        lfo.Enabled = true;
        lfo.OpenWindow = true;
        lfo.RequestDockBelow = true;
        lfo.Type = Sine;
        lfo.coarseTune = 1.0f;
        lfo.fineTune = 0.0f;
        lfo.Amplitude = 0.5f;
        lfo.SampleRate = 44100;
        lfo.Phase = 0.0;
        lfo.vOctCV = 0.5f;
        lfo.linearFMCV = 0.5f;
        lfo.pwmCV = 0.5f;
        lfo.vRange = WaveForm::Bipolar10V;
        lfo.fmDepth = 0.0f;
    }

    void InitializeLFOWaveForm(WaveForm& lfo, int moduleID)
    {
        InitializeLFOWaveForm(lfo);
        lfo.WaveID = moduleID;
    }

    void MainImGui()
    {
        if (LFOs.empty())
        {
            ImGui::Text("LFO Controls:");
            ImGui::Separator();
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 1.0f, 1.0f), "Create LFOs via Rack Manager:");
            ImGui::BulletText("Click 'Add Rack' to create a new rack");
            ImGui::BulletText("Select a rack and click 'Add Module' -> 'LFO'");
            ImGui::BulletText("Click on the LFO module to edit its waveform");

            if (ImGui::Button("Add LFO"))
            {
                AddLFO(WaveForm());
            }
        }
        else
        {
            ImGui::Text("LFO Controls:");
            ImGui::Separator();
            WaveSettings();
        }
    }

    void DrawModuleEditor(Module &module, bool &requestRemove)
    {
        ImGui::Text("LFO Module");
        ImGui::Separator();

        DrawLFOEditor(module.lfoConfig.waveform);

        if (ImGui::Button("Remove LFO Module", ImVec2(-1.0f, 0.0f)))
        {
            requestRemove = true;
        }
    }

    void DrawLFOEditor(WaveForm& lfo)
    {
        ImGui::Text("LFO ID: %d", lfo.WaveID);
        ImGui::Separator();

        ImGui::Text("RATE");
        if (ImGui::BeginTable("LFO_RATE_ROW", 3, ImGuiTableFlags_SizingStretchSame))
        {
            ImGui::TableNextColumn();
            ImGui::Text("FREQ");
            ImGuiKnobs::Knob("##lfo_freq", &lfo.coarseTune, 0.1f, 20.0f, 0.01f, "%.2f Hz", ImGuiKnobVariant_Wiper, 0.0f, ImGuiKnobFlags_NoTitle);

            ImGui::TableNextColumn();
            ImGui::Text("FINE");
            ImGuiKnobs::Knob("##lfo_fine", &lfo.fineTune, -5.0f, 5.0f, 0.01f, "%.2f", ImGuiKnobVariant_Wiper, 0.0f, ImGuiKnobFlags_NoTitle);

            ImGui::TableNextColumn();
            ImGui::Text("LEVEL");
            ImGuiKnobs::Knob("##lfo_amp", &lfo.Amplitude, 0.0f, 1.0f, 0.01f, "%.2f", ImGuiKnobVariant_Wiper, 0.0f, ImGuiKnobFlags_NoTitle);

            ImGui::EndTable();
        }
        ImGui::Spacing();

        ImGui::Text("WAVE SHAPE");
        const char *waveNames[] = {"Sine", "Square", "Sawtooth", "Triangle", "Pulse", "Noise"};
        WaveType waveVals[] = {Sine, Square, Sawtooth, Triangle, Pulse, Noise};

        for (int waveTypeIndex = 0; waveTypeIndex < 6; waveTypeIndex++)
        {
            bool isSelected = (lfo.Type == waveVals[waveTypeIndex]);
            if (isSelected)
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2, 0.7, 0.2, 1.0));

            if (ImGui::Button(waveNames[waveTypeIndex], ImVec2(80, 0)))
            {
                lfo.Type = waveVals[waveTypeIndex];
            }

            if (isSelected)
                ImGui::PopStyleColor();
            if (waveTypeIndex < 5)
                ImGui::SameLine();
        }

        ImGui::Spacing();

        if (lfo.Type == Pulse)
        {
            ImGui::Text("Pulse Width");
            ImGuiKnobs::Knob("##lfo_pwm", &lfo.pwmCV, 0.05f, 0.95f, 0.01f, "%.2f", ImGuiKnobVariant_WiperDot, 0.0f, ImGuiKnobFlags_NoTitle);
            ImGui::Spacing();
        }

        ImGui::Text("MODULE");
        ImGui::Checkbox("Enabled##lfo", &lfo.Enabled);
        ImGui::Spacing();

        ImGuiUtil::Oscilloscope(lfo, ("LFO Waveform##" + std::to_string(lfo.WaveID)).c_str());
    }

    void WaveSettings()
    {
        for (auto &lfo : LFOs)
        {
            DrawLFOEditor(lfo);
        }
    }

    void AddLFO(const WaveForm &newLFO)
    {
        WaveForm lfo = newLFO;
        lfo.WaveID = LFOs.size() + 1;
        lfo.Type = Sine;
        lfo.Enabled = true;
        lfo.SampleRate = 44100;
        lfo.Phase = 0.0;
        lfo.coarseTune = 1.0f; 
        lfo.fineTune = 0.0f;
        lfo.Amplitude = 1.0f;
        LFOs.push_back(lfo);
        std::cout << "Added LFO with ID: " << lfo.WaveID << std::endl;
    }

    void ProcessAudio(WaveForm& lfoWave, float *inputBuffer, float *outputBuffer, int numSamples)
    {
        if (!lfoWave.Enabled)
        {
            for (int i = 0; i < numSamples; i++)
            {
                outputBuffer[i] = 0.0f;
            }
            return;
        }

        for (int i = 0; i < numSamples; i++)
        {
            float frequency = lfoWave.coarseTune + lfoWave.fineTune;

            if (inputBuffer)
            {
                frequency += inputBuffer[i] * 5.0f;
            }

            frequency = std::max(0.1f, std::min(frequency, 20.0f));
            
            lfoWave.Frequency = frequency;
            
            double phaseIncrement = (double)frequency / (double)lfoWave.SampleRate;
            lfoWave.Phase += phaseIncrement;
            
            if (lfoWave.Phase >= 1.0)
            {
                lfoWave.Phase -= 1.0;
            }
            
            float waveValue = 0.0f;
            
            switch (lfoWave.Type)
            {
                case Sine:
                    waveValue = std::sin(2.0f * M_PI * lfoWave.Phase);
                    break;
                case Square:
                    waveValue = (lfoWave.Phase < 0.5) ? 1.0f : -1.0f;
                    break;
                case Sawtooth:
                    waveValue = 2.0f * lfoWave.Phase - 1.0f;
                    break;
                case Triangle:
                    if (lfoWave.Phase < 0.5)
                    {
                        waveValue = 4.0f * lfoWave.Phase - 1.0f;
                    }
                    else
                    {
                        waveValue = 3.0f - 4.0f * lfoWave.Phase;
                    }
                    break;
                case Pulse:
                    waveValue = (lfoWave.Phase < lfoWave.pwmCV) ? 1.0f : -1.0f;
                    break;
                case Noise:
                    waveValue = 0.0f;
                    break;
            }
            
            // Apply amplitude scaling to CV output
            float cvOutput = waveValue * lfoWave.Amplitude;
            outputBuffer[i] = cvOutput;
            
            // Store current voltage for UI display
            lfoWave.currentVoltageOut = cvOutput;
        }
    }
}
