#include "imgui.h"
#include <array>
#include <atomic>
#include <cmath>

#include "../../include/Handler.h"
#include "../../include/Voltage-ControlledAmplifier.h"
#include "../../include/Functions/Audio.h"
#include "../../include/Functions/ImGuiUtil.h"

namespace VCA
{
    // --Variables--
    // Amplitude Control
    static float baseGain = 0.8f;
    static float cvAmount = 0.5f;
    static float cvInput = 0.5f;

    // Envelope Shaping
    static float attackTime = 10.0f;
    static float releaseTime = 50.0f;
    static float envelopeState = 0.0f;

    static bool envelopeActive = false;
    static bool useAttackandRelease = false;

    // Dynamic Range Control
    static float rangeMin = 0.0f;
    static float rangeMax = 1.0f;

    // Oscilloscope
    static constexpr int scopeBufferSize = 2048;
    static std::array<float, scopeBufferSize> vcaScopeBuffer = {0.0f};
    static std::atomic<int> vcaScopeWriteIndex = 0;
    static std::atomic<bool> hasVCAScopeData = false;

    // WaveForm Managment
    WaveForm Input;
    WaveForm Buffer = Input;
    WaveForm Output;

    // --Function--
    // -- Prorotypes --
    void AmpControl();
    void EnvolopeShaping();
    void DynamicRangeControl();

    void Gain(float *buffer, int numSamples);
    void CV(float *buffer, int numSamples);
    void Envelope(float *buffer, int numSamples);
    void Response(float *buffer, int numSamples);

    static void CaptureScopeSamples(const float *outputBuffer, int bufferSize);
    void ProcessAudio(float *inputBuffer, float *outputBuffer, int numSamples);

    // --MainImGui call--
    void MainImGui()
    {
        ImGui::Text("VCA Controls:");
        AmpControl();
        EnvolopeShaping();
        DynamicRangeControl();

        ImGuiUtil::Oscilloscope(Buffer, "VCA Output Display");
    }

    void AmpControl()
    {
        ImGui::SliderFloat("Base Gain", &baseGain, 0.0f, 1.0f);
        ImGui::SliderFloat("CV Amount", &cvAmount, 0.0f, 1.0f);
        ImGui::SliderFloat("CV Input", &cvInput, 0.0f, 1.0f);
    }

    void EnvolopeShaping()
    {
        if (ImGui::Checkbox("Use Attack/Release", &useAttackandRelease))
        {
            envelopeActive = false;
            envelopeState = 0.0f;
        }
        if (useAttackandRelease)
        {
            if (ImGui::Button("Trigger Envelope"))
            {
                envelopeActive = true;
            }
            ImGui::SliderFloat("Attack", &attackTime, 0.0f, 1000.0f, "%.1f ms");
            ImGui::SliderFloat("Release", &releaseTime, 0.0f, 1000.0f, "%.1f ms");
        }
    }

    void DynamicRangeControl()
    {
        ImGui::SliderFloat("Range Min", &rangeMin, 0.0f, 1.0f);
        ImGui::SliderFloat("Range Max", &rangeMax, 0.0f, 1.0f);
    }

    // -Audio proscessing-
    void Gain(float *buffer, int numSamples)
    {
        float effectiveGain = baseGain * (1.0f + (cvInput - 0.5f) * 2.0f * cvAmount);
        if (effectiveGain < 0.0f)
            effectiveGain = 0.0f;
        if (effectiveGain > 1.0f)
            effectiveGain = 1.0f;

        for (int i = 0; i < numSamples; i++)
        {
            buffer[i] *= effectiveGain;
        }
    }

    void CV(float *buffer, int numSamples)
    {
    }

    void Envelope(float *buffer, int numSamples)
    {
        if (!useAttackandRelease)
            return;

        float target = envelopeActive ? 1.0f : 0.0f;
        float coefficient = 0.0f;
        for (int i = 0; i < numSamples; i++)
        {
            if (envelopeState < target)
            {
                coefficient = 1.0f - std::exp(-1.0f / (attackTime * 0.001f * 44100.0f));
            }
            else
            {
                coefficient = 1.0f - std::exp(-1.0f / (releaseTime * 0.001f * 44100.0f));
            }

            envelopeState += coefficient * (target - envelopeState);
            buffer[i] *= envelopeState;
        }

        if (envelopeActive && envelopeState >= 0.999f)
        {
            envelopeState = 1.0f;
            envelopeActive = false;
        }
    }

    void Response(float *buffer, int numSamples)
    {
        for (int i = 0; i < numSamples; i++)
        {
            if (buffer[i] < rangeMin)
                buffer[i] = rangeMin;
            if (buffer[i] > rangeMax)
                buffer[i] = rangeMax;
        }
    }

    static void CaptureScopeSamples(const float *outputBuffer, int bufferSize)
    {
        if (!outputBuffer || bufferSize <= 0)
            return;

        int writeIndex = vcaScopeWriteIndex.load(std::memory_order_relaxed);
        for (int sampleIndex = 0; sampleIndex < bufferSize; sampleIndex++)
        {
            vcaScopeBuffer[writeIndex] = outputBuffer[sampleIndex];
            writeIndex = (writeIndex + 1) % scopeBufferSize;
        }

        vcaScopeWriteIndex.store(writeIndex, std::memory_order_release);
        hasVCAScopeData.store(true, std::memory_order_release);
    }

    void ProcessAudio(float *inputBuffer, float *outputBuffer, int numSamples)
    {
        if (!inputBuffer || !outputBuffer || numSamples <= 0)
            return;

        for (int i = 0; i < numSamples; i++)
        {
            outputBuffer[i] = inputBuffer[i];
        }

        // Apply effects
        Gain(outputBuffer, numSamples);
        Envelope(outputBuffer, numSamples);
        Response(outputBuffer, numSamples);

        // Capture for oscilloscope display
        CaptureScopeSamples(outputBuffer, numSamples);
    }

}