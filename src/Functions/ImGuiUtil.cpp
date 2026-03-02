#include <iostream>
#include "imgui.h"
#include "implot.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#include <vector>
#include <algorithm>
#include "WaveForm.h"

namespace ImGuiUtil
{
    static void RenderSignalPlot(const float *samples, int sampleCount, const char *label, float minY, float maxY)
    {
        if (!samples || sampleCount <= 0)
        {
            return;
        }

        ImVec2 plotSize = ImVec2(-1.0f, 170.0f);
        ImPlotFlags plotFlags = ImPlotFlags_NoMenus |
                                ImPlotFlags_NoBoxSelect |
                                ImPlotFlags_NoMouseText |
                                ImPlotFlags_NoLegend;
        ImPlotAxisFlags axisFlags = ImPlotAxisFlags_NoDecorations |
                                    ImPlotAxisFlags_Lock;

        if (ImPlot::BeginPlot(label, plotSize, plotFlags))
        {
            ImPlot::SetupAxes(nullptr, nullptr, axisFlags, axisFlags);
            ImPlot::SetupAxisLimits(ImAxis_X1, 0.0, static_cast<double>(sampleCount), ImGuiCond_Always);
            ImPlot::SetupAxisLimits(ImAxis_Y1, static_cast<double>(minY), static_cast<double>(maxY), ImGuiCond_Always);
            ImPlot::PlotLine("Signal", samples, sampleCount);
            ImPlot::EndPlot();
        }
    }

    void Render()
    {
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // Update and render additional platform windows when using multi-viewports
        ImGuiIO& imguiIo = ImGui::GetIO();
        if (imguiIo.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            GLFWwindow* previousOpenGlContext = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(previousOpenGlContext);
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
        float nyquist = static_cast<float>(wave.SampleRate) * 0.5f;
        float freqMax = std::min(20000.0f, nyquist);
        if (wave.Frequency > freqMax)
        {
            wave.Frequency = freqMax;
            ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), "Frequency clamped to Nyquist: %.0f Hz", freqMax);
        }

        int displaySamples = std::min(wave.SampleRate, 2048);

        std::vector<float> displayBuffer(displaySamples);
        GetWaveFormData(wave, displayBuffer.data(), displaySamples, wave.displayOffset);
        RenderSignalPlot(displayBuffer.data(), displaySamples, label, -1.05f, 1.05f);

        // Advance display offset so waveform scrolls. Use fixed frame step matching ~16ms.
        int displayAdvanceSamples = std::max(1, static_cast<int>(wave.SampleRate * 0.016f));
        int displayWrapLength = std::max(1, wave.SampleRate * 1000);
        wave.displayOffset = (wave.displayOffset + displayAdvanceSamples) % displayWrapLength;
    }

    void PlotSignal(const float *samples, int sampleCount, const char *label, float minY, float maxY)
    {
        RenderSignalPlot(samples, sampleCount, label, minY, maxY);
    }
}