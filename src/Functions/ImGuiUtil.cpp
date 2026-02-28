#include <iostream>
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#include <vector>
#include <algorithm>
#include "WaveForm.h"

namespace ImGuiUtil
{
    void Render()
    {
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // Update and render additional platform windows when using multi-viewports
        ImGuiIO& io = ImGui::GetIO();
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            GLFWwindow* backup_current_context = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(backup_current_context);
        }
    }

    void Begin()
    {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        // Create a full-screen dockspace so other windows can dock
        static bool dockspaceOpen = true;
        ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize
                     | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
        ImGui::Begin("DockSpaceHost", &dockspaceOpen, window_flags);
        ImGui::PopStyleVar(2);
        ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);
        ImGui::End();
    }

    void End()
    {
        ImGui::EndFrame();
    }

    void Oscilloscope(WaveForm &wave, const char *label)
    {
        // Prevent aliasing: limit display frequency to Nyquist (sampleRate/2)
        float nyquist = static_cast<float>(wave.SampleRate) * 0.5f;
        float freqMax = std::min(20000.0f, nyquist);
        if (wave.Frequency > freqMax)
        {
            wave.Frequency = freqMax;
            ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), "Frequency clamped to Nyquist: %.0f Hz", freqMax);
        }

        // Use a smaller buffer for display to avoid plotting excessive points each frame
        int displaySamples = std::min(wave.SampleRate, 2048);

        std::vector<float> buf(displaySamples);
        GetWaveFormData(wave, buf.data(), displaySamples, wave.displayOffset);
        ImGui::PlotLines(label, buf.data(), displaySamples);

        // Advance display offset so waveform scrolls. Use fixed frame step matching ~16ms.
        int advance = std::max(1, static_cast<int>(wave.SampleRate * 0.016f));
        int wrap = std::max(1, wave.SampleRate * 1000);
        wave.displayOffset = (wave.displayOffset + advance) % wrap;
    }
}