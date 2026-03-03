#include <iostream>
#include <vector>
#include <list>
#include <cmath>

#include "imgui.h"
#include "../../include/Functions/ImGuiUtil.h"
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
    void ProcessAudio(float* inputBuffer, float* outputBuffer, int numSamples);

    // --functions--
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

    void WaveSettings()
    {
        for (auto &lfo : LFOs)
        {
            ImGui::Text("LFO ID: %d", lfo.WaveID);
            ImGui::Separator();

            // Low Frequency Control
            ImGui::SliderFloat("Frequency##lfo", &lfo.coarseTune, 0.1f, 20.0f);
            ImGui::SliderFloat("Fine Tune##lfo", &lfo.fineTune, -5.0f, 5.0f);
            ImGui::SliderFloat("Amplitude##lfo", &lfo.Amplitude, 0.0f, 1.0f);
            ImGui::Spacing();

            // WaveFrom Selection
            ImGui::Text("WAVEFORM");
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

            ImGui::Text("Phase Control");


            ImGui::Spacing();

            ImGui::Text("Pulse Width");

            //ImGui::SliderFloat("Pulse Width##lfo", &lfo.PulseWidth, 0.0f, 1.0f);

            ImGui::Spacing();

            ImGui::Text("Polarity");



            ImGuiUtil::Oscilloscope(lfo, ("LFO Waveform##" + std::to_string(lfo.WaveID)).c_str());
        }
    }

    void AddLFO(const WaveForm &newLFO)
    {
        WaveForm lfo = newLFO;
        lfo.WaveID = LFOs.size() + 1;
        lfo.Type = Sine;
        LFOs.push_back(lfo);
        std::cout << "Added LFO with ID: " << lfo.WaveID << std::endl;
    }
    
    void ProcessAudio(float* inputBuffer, float* outputBuffer, int numSamples)
    {
        for (auto &lfo : LFOs)
        {
            float frequency = lfo.coarseTune + lfo.fineTune;
            for (int i = 0; i < numSamples; i++)
            {
                float t = (float)i / lfo.SampleRate;
                float lfoValue = lfo.Amplitude * sinf(2.0f * 3.14159f * frequency * t);
                outputBuffer[i] += lfoValue;
            }
        }
    }
}
