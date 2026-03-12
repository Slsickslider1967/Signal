#include <iostream>
#include <vector>
#include <array>
#include <atomic>
#include "imgui.h"
#include "imgui-knobs.h"
#include "../../include/Functions/ImGuiUtil.h"
#include "../../include/Functions/Module.h"
#include "../../include/WaveForm.h"
#include "../../include/VoltageControllFilter.h"
#include "../../include/Functions/CV.h"


namespace VCF
{
    // --Variables--
    static float cutoffFrequency = 1000.0f; // Hz
    static float baseCutoff = 1000.0f; // Base cutoff before CV modulation
    static float cutoffCVInput = 0.5f; // CV input (0..1)
    static float cutoffCVAmount = 0.0f; // CV modulation depth
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

    // --Function--
    void DrawFilterTypeEditor(FilterType& filterType)
    {
        ImGui::Text("VCF Controls:");
        const char *filterTypes[] = {"Low-Pass", "High-Pass", "Band-Pass", "Notch"};
        int currentFilterType = static_cast<int>(filterType);
        if (ImGui::Combo("Filter Type", &currentFilterType, filterTypes, 4))
        {
            filterType = static_cast<FilterType>(currentFilterType);
        }
    }

    void MainImGui()
    {
        ImGui::Text("CUTOFF");
        if (ImGui::BeginTable("VCF_CUTOFF_ROW", 3, ImGuiTableFlags_SizingStretchSame))
        {
            ImGui::TableNextColumn();
            ImGui::Text("BASE");
            ImGuiKnobs::Knob("##vcf_base", &baseCutoff, 20.0f, 20000.0f, 20.0f, "%.1f Hz", ImGuiKnobVariant_WiperDot, 0.0f, ImGuiKnobFlags_Logarithmic | ImGuiKnobFlags_NoTitle);

            ImGui::TableNextColumn();
            ImGui::Text("CV IN");
            ImGuiKnobs::Knob("##vcf_cv_in", &cutoffCVInput, 0.0f, 1.0f, 0.01f, "%.2f", ImGuiKnobVariant_Wiper, 0.0f, ImGuiKnobFlags_NoTitle);

            ImGui::TableNextColumn();
            ImGui::Text("CV AMT");
            ImGuiKnobs::Knob("##vcf_cv_amt", &cutoffCVAmount, 0.0f, 1.0f, 0.01f, "%.2f", ImGuiKnobVariant_Wiper, 0.0f, ImGuiKnobFlags_NoTitle);

            ImGui::EndTable();
        }
        
        // Apply CV modulation with exponential curve for natural frequency response
        cutoffFrequency = CV::ModulateParameter(
            CV::NormalizeCV(baseCutoff, {20.0f, 20000.0f}),
            cutoffCVInput,
            cutoffCVAmount,
            {0.0f, 1.0f},
            {0.0f, 1.0f},
            CV::CVFunction::Exponential
        );
        cutoffFrequency = CV::DenormalizeCV(cutoffFrequency, {20.0f, 20000.0f});
        
        ImGui::Text("Effective Cutoff: %.1f Hz", cutoffFrequency);
        ImGui::Text("RESONANCE");
        ImGuiKnobs::Knob("##vcf_res", &resonance, 0.0f, 1.0f, 0.01f, "%.2f", ImGuiKnobVariant_Wiper, 0.0f, ImGuiKnobFlags_NoTitle);

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

    void DrawModuleEditor(Module &module, bool &requestRemove)
    {
        ImGui::Text("Filter Module");
        ImGui::Separator();

        DrawFilterTypeEditor(module.vcfConfig.filterType);
        MainImGui();

        if (ImGui::Button("Remove VCF Module", ImVec2(-1.0f, 0.0f)))
        {
            requestRemove = true;
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
            case 0: 
                ApplyLowPass(inputBuffer, outputBuffer, bufferSize);
                break;
            case 1: 
                ApplyHighPass(inputBuffer, outputBuffer, bufferSize);
                break;
            case 2:
                ApplyBandPass(inputBuffer, outputBuffer, bufferSize);
                break;
            case 3: 
                ApplyNotch(inputBuffer, outputBuffer, bufferSize);
                break;
            default:
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