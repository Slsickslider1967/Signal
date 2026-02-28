#include <iostream>
#include "../../include/Functions/Window.h"
#include "../../include/WaveForm.h"
#include "../../include/Functions/Audio.h"
#include "../../include/Functions/ImGuiUtil.h"
#include "imgui.h"
#include <list>
#include <vector>
#include <thread>
#include <chrono>
#include <algorithm>
#include <GLFW/glfw3.h>

// Ensure waveform data helper is visible here
void GetWaveFormData(const WaveForm &wave, float *buffer, int bufferSize);

// --Main Variables--
int NextWaveID = 1;
std::list<WaveForm> WaveForms;

// --Function Prototypes--
int min();
void MakeWindow();
void DestroyWindow();
void MainImgui();
void Render();
int CleanUp();
void DrawWaveFormsMenu();
void AddWaveForm();

int main()
{
    std::cout << "Hello World!" << std::endl;
    MakeWindow();
    while (!Window::ShouldClose())
    {
        ImGuiUtil::Begin();
        MainImgui();
        ImGuiUtil::End();

        Window::ClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        Render();

        // Sleep a bit to cap CPU usage (~60 FPS)
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    return CleanUp();
}

// -- Window Functions --

void MakeWindow()
{
    std::cout << "Creating window..." << std::endl;
    Window::CreateWindow(800, 600, "Signal Generator");
    Audio::Init();
}

void DestroyWindow()
{
    std::cout << "Destroying window..." << std::endl;
    Window::DestroyWindow();
}

int CleanUp()
{
    std::cout << "Cleaning up..." << std::endl;
    DestroyWindow();
    Audio::Close();

    return 0;
}

void Render()
{
    ImGuiUtil::Render();
    Window::SwapBuffers();
    Window::PollEvents();
}

// -- ImGui Functions --
void MainImgui()
{
    ImGui::Begin("Waveform Generator");

    DrawWaveFormsMenu();
    if (ImGui::Button("Add Waveform"))
    {
        AddWaveForm();
    }

    ImGui::End();
}

void DrawWaveFormsMenu()
{
    for (auto &wave : WaveForms)
    {

        ImGui::PushID(wave.WaveID);
        ImGui::Text("Waveform %d", wave.WaveID);
        ImGui::Separator();
        if (ImGui::Button("Sine"))
            wave.Type = Sine;
        ImGui::SameLine();
        if (ImGui::Button("Square"))
            wave.Type = Square;
        ImGui::SameLine();
        if (ImGui::Button("Sawtooth"))
            wave.Type = Sawtooth;
        ImGui::SameLine();
        if (ImGui::Button("Triangle"))
            wave.Type = Triangle;
        ImGui::SameLine();
        if (ImGui::Button("Tangent"))
            wave.Type = Tangent;
        ImGui::Separator();
        ImGui::Checkbox("Enabled", &wave.Enabled);
        ImGui::SameLine();
        ImGui::Text("Waveform %d", wave.WaveID);
        ImGui::SliderFloat("Frequency", &wave.Frequency, 0.0f, 20000.0f);
        ImGui::SliderFloat("Speed", &wave.Speed, 0.01f, 16.0f);
        ImGui::SliderFloat("Amplitude", &wave.Amplitude, 0.0f, 1.0f);
        ImGui::SliderInt("Sample Rate", &wave.SampleRate, 8000, 96000);

        ImGuiUtil::Oscilloscope(wave, "Waveform Preview");
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::Spacing();

        ImGui::PopID();
    }

    std::vector<WaveForm> wavesVec;
    for (auto &w : WaveForms)
        wavesVec.push_back(w);
    Audio::SetWaveForms(wavesVec);
}

void AddWaveForm()
{
    WaveForm newWave;
    newWave.WaveID = NextWaveID++;
    newWave.Enabled = true;
    newWave.Type = Sine;
    newWave.Frequency = 440.0f;
    newWave.Amplitude = 1.0f;
    newWave.SampleRate = 44100;

    WaveForms.push_back(newWave);
}