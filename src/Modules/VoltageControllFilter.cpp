#include <iostream>
#include <vector>
#include <array>
#include <atomic>
#include "imgui.h"
#include "../../include/Functions/ImGuiUtil.h"
#include "../../include/WaveForm.h"
#include "../../include/VoltageControllFilter.h"


namespace VCF
{
    // --Variables--
    static float cutoffFrequency = 1000.0f; // Hz
    static float resonance = 0.5f;
    static float previousOutputSample = 0.0f;
    static const float sampleRateHz = 44100.0f;
    static constexpr int scopeBufferSize = 2048;
    static std::array<float, scopeBufferSize> filteredScopeBuffer = {0.0f};
    static std::atomic<int> filteredScopeWriteIndex = 0;
    static std::atomic<bool> hasFilteredScopeData = false;

    static void CaptureScopeSamples(const float *outputBuffer, int bufferSize)
    {
        if (!outputBuffer || bufferSize <= 0)
        {
            return;
        }

        int writeIndex = filteredScopeWriteIndex.load(std::memory_order_relaxed);
        for (int sampleIndex = 0; sampleIndex < bufferSize; sampleIndex++)
        {
            filteredScopeBuffer[writeIndex] = outputBuffer[sampleIndex];
            writeIndex = (writeIndex + 1) % scopeBufferSize;
        }

        filteredScopeWriteIndex.store(writeIndex, std::memory_order_release);
        hasFilteredScopeData.store(true, std::memory_order_release);
    }

    // --Enum--
    enum FilterType
    {
        LowPass,
        HighPass,
        BandPass,
        Notch,
        Variable
    }; 

    // --Function--
    void MainImGui()
    {
        ImGui::SliderFloat("Cutoff Frequency", &cutoffFrequency, 20.0f, 20000.0f);
        ImGui::SliderFloat("Resonance", &resonance, 0.0f, 1.0f);

        if (hasFilteredScopeData.load(std::memory_order_acquire))
        {
            std::array<float, scopeBufferSize> displayScopeBuffer = {0.0f};
            int writeIndex = filteredScopeWriteIndex.load(std::memory_order_acquire);
            for (int sampleIndex = 0; sampleIndex < scopeBufferSize; sampleIndex++)
            {
                int readIndex = (writeIndex + sampleIndex) % scopeBufferSize;
                displayScopeBuffer[sampleIndex] = filteredScopeBuffer[readIndex];
            }
            ImGuiUtil::PlotSignal(displayScopeBuffer.data(), scopeBufferSize, "VCF Output Display");
        }
        else
        {
            static float emptyOscilloscope[256] = {0.0f};
            ImGui::Text("No filtered signal yet");
            ImGuiUtil::PlotSignal(emptyOscilloscope, 256, "VCF Output Display");
        }
    }

    // --The diffrent filter algorithms--
    void ApplyLowPass(float *inputBuffer, float *outputBuffer, int bufferSize)
    {
        float angularCutoff = 2.0f * 3.14159f * cutoffFrequency / sampleRateHz;
        float smoothingFactor = angularCutoff / (angularCutoff + 1.0f);
        
        for (int sampleIndex = 0; sampleIndex < bufferSize; sampleIndex++)
        {
            outputBuffer[sampleIndex] = smoothingFactor * inputBuffer[sampleIndex] + (1.0f - smoothingFactor) * previousOutputSample;
            previousOutputSample = outputBuffer[sampleIndex];
        }
    }

    void ApplyHighPass(float *inputBuffer, float *outputBuffer, int bufferSize)
    {
        float angularCutoff = 2.0f * 3.14159f * cutoffFrequency / sampleRateHz;
        float highPassFactor = 1.0f / (angularCutoff + 1.0f);
        
        for (int sampleIndex = 0; sampleIndex < bufferSize; sampleIndex++)
        {
            outputBuffer[sampleIndex] = highPassFactor * (previousOutputSample + inputBuffer[sampleIndex] - previousOutputSample);
            previousOutputSample = inputBuffer[sampleIndex];
        }
    }

    void ApplyBandPass(float *inputBuffer, float *outputBuffer, int bufferSize)
    {
        std::vector<float> lowPassBuffer(bufferSize);
        ApplyLowPass(inputBuffer, lowPassBuffer.data(), bufferSize);
        ApplyHighPass(lowPassBuffer.data(), outputBuffer, bufferSize);
    }

    void ApplyNotch(float *inputBuffer, float *outputBuffer, int bufferSize)
    {
        std::vector<float> bandPassBuffer(bufferSize);
        ApplyBandPass(inputBuffer, bandPassBuffer.data(), bufferSize);
        
        for (int sampleIndex = 0; sampleIndex < bufferSize; sampleIndex++)
        {
            outputBuffer[sampleIndex] = inputBuffer[sampleIndex] - bandPassBuffer[sampleIndex];
        }
    }

    void ProcessAudio(float *inputBuffer, float *outputBuffer, int bufferSize, int selectedFilterType)
    {
        switch (selectedFilterType)
        {
            case 0: // FILTER_LP
                ApplyLowPass(inputBuffer, outputBuffer, bufferSize);
                break;
            case 1: // FILTER_HP
                ApplyHighPass(inputBuffer, outputBuffer, bufferSize);
                break;
            case 2: // FILTER_BP
                ApplyBandPass(inputBuffer, outputBuffer, bufferSize);
                break;
            case 3: // FILTER_NOTCH
                ApplyNotch(inputBuffer, outputBuffer, bufferSize);
                break;
            default:
                // Pass through
                for (int sampleIndex = 0; sampleIndex < bufferSize; sampleIndex++)
                    outputBuffer[sampleIndex] = inputBuffer[sampleIndex];
                break;
        }

        CaptureScopeSamples(outputBuffer, bufferSize);
    }

    void State()
    {

    }
}