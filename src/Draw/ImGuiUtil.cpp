#include <iostream>
#include "imgui.h"
#include "imnodes.h"
#include "implot.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#include <vector>
#include <algorithm>
#include "WaveForm.h"

namespace ImGuiUtil
{
    // Draw a single signal line plot with locked axes and minimal plot chrome.
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

    // Draw two overlaid signals for quick visual comparison.
    static void RenderDualSignalPlot(const float *redSamples, const float *blueSamples, int sampleCount, const char *label, float minY, float maxY)
    {
        if (!redSamples || !blueSamples || sampleCount <= 0)
        {
            return;
        }

        ImVec2 plotSize = ImVec2(-1.0f, 170.0f);
        ImPlotFlags plotFlags = ImPlotFlags_NoMenus |
                                ImPlotFlags_NoBoxSelect |
                                ImPlotFlags_NoMouseText;
        ImPlotAxisFlags axisFlags = ImPlotAxisFlags_NoDecorations |
                                    ImPlotAxisFlags_Lock;

        if (ImPlot::BeginPlot(label, plotSize, plotFlags))
        {
            ImPlot::SetupAxes(nullptr, nullptr, axisFlags, axisFlags);
            ImPlot::SetupAxisLimits(ImAxis_X1, 0.0, static_cast<double>(sampleCount), ImGuiCond_Always);
            ImPlot::SetupAxisLimits(ImAxis_Y1, static_cast<double>(minY), static_cast<double>(maxY), ImGuiCond_Always);

            ImPlotSpec blueSpec;
            blueSpec.LineColor = ImVec4(0.20f, 0.55f, 0.95f, 1.0f);
            blueSpec.LineWeight = 1.7f;
            ImPlot::PlotLine("Reference (Blue)", blueSamples, sampleCount, 1.0, 0.0, blueSpec);

            ImPlotSpec redSpec;
            redSpec.LineColor = ImVec4(0.92f, 0.18f, 0.20f, 1.0f);
            redSpec.LineWeight = 1.7f;
            ImPlot::PlotLine("Output (Red)", redSamples, sampleCount, 1.0, 0.0, redSpec);

            ImPlot::EndPlot();
        }
    }

    // Render the current ImGui frame, including multi-viewport windows when enabled.
    void Render()
    {
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // When viewports are on, render platform windows and restore the previous GL context.
        ImGuiIO& imguiIo = ImGui::GetIO();
        if (imguiIo.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            GLFWwindow* previousOpenGlContext = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(previousOpenGlContext);
        }
    }

    // Begin a new ImGui frame for this tick.
    void Begin()
    {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        // Fixed-window mode: do not create a fullscreen dockspace host.
    }

    // End the current ImGui frame.
    void End()
    {
        ImGui::EndFrame();
    }

    // void Oscilloscope(WaveForm &wave, const char *label)
    // {
    //     float nyquist = static_cast<float>(wave.SampleRate) * 0.5f;
    //     float freqMax = std::min(20000.0f, nyquist);
    //     if (wave.Frequency > freqMax)
    //     {
    //         wave.Frequency = freqMax;
    //         ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), "Frequency clamped to Nyquist: %.0f Hz", freqMax);
    //     }

    //     int displaySamples = std::min(wave.SampleRate, 2048);

    //     WaveForm referenceWave = wave;
    //     referenceWave.vOctCV = 0.5f;
    //     referenceWave.linearFMCV = 0.5f;
    //     referenceWave.fmDepth = 0.0f;

    //     std::vector<float> referenceBuffer(displaySamples);
    //     GetWaveFormData(referenceWave, referenceBuffer.data(), displaySamples, wave.displayOffset);

    //     std::vector<float> displayBuffer(displaySamples);
    //     GetWaveFormData(wave, displayBuffer.data(), displaySamples, wave.displayOffset);
    //     RenderDualSignalPlot(displayBuffer.data(), referenceBuffer.data(), displaySamples, label, -1.05f, 1.05f);

    //     ImGui::TextColored(ImVec4(0.20f, 0.55f, 0.95f, 1.0f), "Blue = reference");
    //     ImGui::SameLine();
    //     ImGui::TextColored(ImVec4(0.92f, 0.18f, 0.20f, 1.0f), "Red = output");

    //     // Advance display offset so waveform scrolls. ~16ms.
    //     int displayAdvanceSamples = std::max(1, static_cast<int>(wave.SampleRate * 0.0016f));
    //     int displayWrapLength = std::max(1, wave.SampleRate * 1000);
    //     wave.displayOffset = (wave.displayOffset + displayAdvanceSamples) % displayWrapLength;
    // }

    // void PlotSignal(const float *samples, int sampleCount, const char *label, float minY, float maxY)
    // {
    //     RenderSignalPlot(samples, sampleCount, label, minY, maxY);
    // }
}