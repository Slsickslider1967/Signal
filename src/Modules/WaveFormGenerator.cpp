#include <iostream>
#include "../../include/Functions/Window.h"
#include "../../include/WaveForm.h"
#include "../../include/Functions/Audio.h"
#include "../../include/Functions/ImGuiUtil.h"
#include "../../include/SpeedManipulation.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <list>
#include <vector>
#include <thread>
#include <chrono>
#include <algorithm>
#include <string>
#include <GLFW/glfw3.h>

// NOTE: `GetWaveFormData` is declared in include/WaveForm.h — no local prototype.
namespace WaveFormGen
{
    // --Main Variables--
    int NextWaveID = 1;
    std::list<WaveForm> WaveForms;

    // --ImGui UI functions exposed as an "addon" --
    void DisplayWaveForm();
    void DrawWaveFromsMenuu(WaveForm &wave);

    void MainImgui()
    {
        // ImGui::SetNextWindowPos(ImVec2(60, 60), ImGuiCond_FirstUseEver);
        // ImGui::SetNextWindowSize(ImVec2(420, 300), ImGuiCond_FirstUseEver);
        ImGui::Begin("Waveform Generator");

        DisplayWaveForm();

        if (ImGui::Button("Add Waveform"))
        {
            // create a new waveform with sensible defaults
            WaveForm newWave;
            newWave.WaveID = NextWaveID++;
            newWave.Enabled = true;
            newWave.OpenWindow = true;
            newWave.RequestDockBelow = true;
            newWave.Type = Sine;
            newWave.Frequency = 440.0f;
            newWave.Amplitude = 1.0f;
            newWave.SampleRate = 44100;
                // sensible defaults for new CV-based controls
                newWave.cv = 0.5f;
                newWave.vRange = WaveForm::Bipolar5V;
                newWave.foldAmount = 0.0f;
                newWave.harmonicMix = 0.0f;
            WaveForms.push_back(newWave);
        }

        // Push updated waveform list to audio system so playback reflects UI changes
        std::vector<WaveForm> wavesVec;
        for (auto &w : WaveForms) wavesVec.push_back(w);
        Audio::SetWaveForms(wavesVec);

        ImGui::End();
    }


    // NEeds to be merged with DrawWaveFromsMenuu
    void DisplayWaveForm()
    {
        if (WaveForms.empty())
        {
            ImGui::Text("No waveforms. Click 'Add Waveform' to create one.");
            return;
        }

        for (auto it = WaveForms.begin(); it != WaveForms.end(); )
        {
            WaveForm &wave = *it;
            ImGui::PushID(wave.WaveID);

            ImGui::Text("Waveform %d", wave.WaveID);
            ImGui::SameLine();
            if (ImGui::Button("Remove"))
            {
                it = WaveForms.erase(it);
                NextWaveID--;
                ImGui::PopID();
                continue;
            }

            ImGui::Separator();

            DrawWaveFromsMenuu(wave);

            ImGui::Spacing();

            ImGui::PopID();
            ++it;
        }
    }



    // Function settings
    void DrawWaveFromsMenuu(WaveForm &wave)
    {
            if (ImGui::Button("Sine")) wave.Type = Sine; ImGui::SameLine();
            if (ImGui::Button("Square")) wave.Type = Square; ImGui::SameLine();
            if (ImGui::Button("Sawtooth")) wave.Type = Sawtooth; ImGui::SameLine();
            if (ImGui::Button("Triangle")) wave.Type = Triangle; ImGui::SameLine();
            if (ImGui::Button("Tangent")) wave.Type = Tangent;

            // Voltage standard selection
            const char* vRanges[] = {"±5V","±10V","±12V","±15V"};
            int vr = (int)wave.vRange;
            if (ImGui::Combo("Voltage Range", &vr, vRanges, IM_ARRAYSIZE(vRanges)))
                wave.vRange = (WaveForm::VoltageRange)vr;

            // CV and timbre controls
            ImGui::SliderFloat("CV (norm)", &wave.cv, 0.0f, 1.0f);
            ImGui::SliderFloat("Fold Amount", &wave.foldAmount, 0.0f, 1.0f);
            ImGui::SliderFloat("Harmonic Mix", &wave.harmonicMix, 0.0f, 1.0f);
            // CV destination routing
            const char* cvDests[] = {"None","Frequency","Amplitude","Drive"};
            int dest = (int)wave.cvDest;
            if (ImGui::Combo("CV Destination", &dest, cvDests, IM_ARRAYSIZE(cvDests)))
                wave.cvDest = (WaveForm::CVDestination)dest;

            // Show computed CV volts and an indicative drive amount
            double vMax = 5.0;
            switch (wave.vRange)
            {
                case WaveForm::Bipolar5V: vMax = 5.0; break;
                case WaveForm::Bipolar10V: vMax = 10.0; break;
                case WaveForm::Bipolar12V: vMax = 12.0; break;
                case WaveForm::Bipolar15V: vMax = 15.0; break;
                default: vMax = 5.0; break;
            }
            double cvVolt = (static_cast<double>(wave.cv) * 2.0 - 1.0) * vMax;
            double drive = 1.0 + wave.foldAmount * (std::abs(cvVolt) / vMax) * 8.0;
            ImGui::Text("CV volts: %.3f V", cvVolt);
            ImGui::Text("Drive (est): %.3f", drive);

            ImGui::Separator();
            ImGui::Checkbox("Enabled", &wave.Enabled);
            ImGui::SameLine();
            ImGui::Text("Waveform %d", wave.WaveID);
            ImGui::SliderFloat("Frequency", &wave.Frequency, 0.0f, 20000.0f);
            ImGui::SliderFloat("Speed", &wave.Speed, 0.01f, 16.0f);
            ImGui::SliderFloat("Amplitude", &wave.Amplitude, 0.0f, 1.0f);
            ImGui::SliderInt("Sample Rate", &wave.SampleRate, 8000, 96000);

            ImGuiUtil::Oscilloscope(wave, "Waveform Preview");
    }
}