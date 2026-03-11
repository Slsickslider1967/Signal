#include "imgui.h"
#include "imgui-knobs.h"
#include <array>
#include <atomic>
#include <cmath>

#include "../../include/Handler.h"
#include "../../include/Module.h"
#include "../../include/Voltage-ControlledAmplifier.h"
#include "../../include/Functions/Audio.h"
#include "../../include/Functions/ImGuiUtil.h"
#include "../../include/Functions/CV.h"

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

    void SetCVInput(float value);
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
        ImGui::Separator();
        
        AmpControl();
        EnvolopeShaping();
        DynamicRangeControl();

        if (hasVCAScopeData.load(std::memory_order_acquire))
        {
            ImGuiUtil::PlotSignal(vcaScopeBuffer.data(), scopeBufferSize, "VCA Output Display");
        }
    }

    void DrawModuleEditor(Module &module, bool &requestRemove)
    {
        baseGain = module.vcaConfig.baseGain;
        cvAmount = module.vcaConfig.cvAmount;
        cvInput = module.vcaConfig.cvInput;
        attackTime = module.vcaConfig.attackTime;
        releaseTime = module.vcaConfig.releaseTime;
        envelopeState = module.vcaConfig.envelopeState;
        envelopeActive = module.vcaConfig.envelopeActive;
        useAttackandRelease = module.vcaConfig.useAttackandRelease;
        rangeMin = module.vcaConfig.rangeMin;
        rangeMax = module.vcaConfig.rangeMax;

        ImGui::Text("Amplifier Module");
        ImGui::Separator();

        MainImGui();

        module.vcaConfig.baseGain = baseGain;
        module.vcaConfig.cvAmount = cvAmount;
        module.vcaConfig.cvInput = cvInput;
        module.vcaConfig.attackTime = attackTime;
        module.vcaConfig.releaseTime = releaseTime;
        module.vcaConfig.envelopeState = envelopeState;
        module.vcaConfig.envelopeActive = envelopeActive;
        module.vcaConfig.useAttackandRelease = useAttackandRelease;
        module.vcaConfig.rangeMin = rangeMin;
        module.vcaConfig.rangeMax = rangeMax;

        if (ImGui::Button("Remove VCA Module", ImVec2(-1.0f, 0.0f)))
        {
            requestRemove = true;
        }
    }

    void AmpControl()
    {
        ImGui::Text("AMPLIFIER");
        if (ImGui::BeginTable("VCA_AMP_ROW", 3, ImGuiTableFlags_SizingStretchSame))
        {
            ImGui::TableNextColumn();
            ImGui::Text("GAIN");
            ImGuiKnobs::Knob("##vca_gain", &baseGain, 0.0f, 1.0f, 0.01f, "%.2f", ImGuiKnobVariant_Wiper, 0.0f, ImGuiKnobFlags_NoTitle);

            ImGui::TableNextColumn();
            ImGui::Text("CV AMT");
            ImGuiKnobs::Knob("##vca_cv_amt", &cvAmount, 0.0f, 1.0f, 0.01f, "%.2f", ImGuiKnobVariant_Wiper, 0.0f, ImGuiKnobFlags_NoTitle);

            ImGui::TableNextColumn();
            ImGui::Text("CV IN");
            ImGuiKnobs::Knob("##vca_cv_in", &cvInput, 0.0f, 1.0f, 0.01f, "%.2f", ImGuiKnobVariant_Wiper, 0.0f, ImGuiKnobFlags_NoTitle);

            ImGui::EndTable();
        }
        ImGui::Text("CV -> Gain depth and manual CV");
        
        // Show effective gain calculation
        float effectiveGain = CV::ModulateParameter(
            baseGain, cvInput, cvAmount,
            {0.0f, 1.0f}, {0.0f, 1.0f},
            CV::CVFunction::Linear
        );
        ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), 
            "Effective Gain: %.3f", effectiveGain);
    }

    void EnvolopeShaping()
    {
        ImGui::Text("ENVELOPE");
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
            if (ImGui::BeginTable("VCA_ENV_ROW", 2, ImGuiTableFlags_SizingStretchSame))
            {
                ImGui::TableNextColumn();
                ImGui::Text("ATTACK");
                ImGuiKnobs::Knob("##vca_attack", &attackTime, 0.0f, 1000.0f, 10.0f, "%.1f ms", ImGuiKnobVariant_Wiper, 0.0f, ImGuiKnobFlags_NoTitle);

                ImGui::TableNextColumn();
                ImGui::Text("RELEASE");
                ImGuiKnobs::Knob("##vca_release", &releaseTime, 0.0f, 1000.0f, 10.0f, "%.1f ms", ImGuiKnobVariant_Wiper, 0.0f, ImGuiKnobFlags_NoTitle);

                ImGui::EndTable();
            }
        }
    }

    void DynamicRangeControl()
    {
        ImGui::Text("RANGE");
        if (ImGui::BeginTable("VCA_RANGE_ROW", 2, ImGuiTableFlags_SizingStretchSame))
        {
            ImGui::TableNextColumn();
            ImGui::Text("MIN");
            ImGuiKnobs::Knob("##vca_min", &rangeMin, 0.0f, 1.0f, 0.01f, "%.2f", ImGuiKnobVariant_Wiper, 0.0f, ImGuiKnobFlags_NoTitle);

            ImGui::TableNextColumn();
            ImGui::Text("MAX");
            ImGuiKnobs::Knob("##vca_max", &rangeMax, 0.0f, 1.0f, 0.01f, "%.2f", ImGuiKnobVariant_Wiper, 0.0f, ImGuiKnobFlags_NoTitle);

            ImGui::EndTable();
        }
    }

    void SetCVInput(float value)
    {
        cvInput = CV::ClampCV(value, {0.0f, 1.0f});
    }
    void Gain(float *buffer, int numSamples)
    {
        float effectiveGain = CV::ModulateParameter(
            baseGain,
            cvInput,
            cvAmount,
            {0.0f, 1.0f},
            {0.0f, 1.0f},
            CV::CVFunction::Linear
        );

        for (int i = 0; i < numSamples; i++)
        {
            buffer[i] *= effectiveGain;
        }
    }

    void CV(float *buffer, int numSamples)
    {
        float modulation = CV::ModulateParameter(
            0.5f,  
            cvInput,
            cvAmount,
            {0.0f, 1.0f},
            {0.0f, 1.0f},
            CV::CVFunction::Linear
        );

        for (int i = 0; i < numSamples; i++)
        {
            buffer[i] *= modulation;
        }
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

        CaptureScopeSamples(outputBuffer, numSamples);
    }

    void ProcessAudioWithCVBuffer(float *inputBuffer, float *outputBuffer, int numSamples, const float *cvBuffer, int cvBufferSize)
    {
        if (!inputBuffer || !outputBuffer || numSamples <= 0)
            return;

        // Store CV buffer info for UI display
        static bool hasExternalCV = false;
        hasExternalCV = (cvBuffer != nullptr && cvBufferSize > 0);

        for (int i = 0; i < numSamples; i++)
        {
            float sampleCV = cvInput;
            if (cvBuffer && i < cvBufferSize)
            {
                sampleCV = CV::ClampCV(cvBuffer[i], {0.0f, 1.0f});
                
                // Update the static CV input for UI display (use first sample)
                if (i == 0)
                {
                    cvInput = sampleCV;
                }
            }

            float effectiveGain = CV::ModulateParameter(
                baseGain,
                sampleCV,
                cvAmount,
                {0.0f, 1.0f},
                {0.0f, 1.0f},
                CV::CVFunction::Linear
            );

            outputBuffer[i] = inputBuffer[i] * effectiveGain;
        }

        Envelope(outputBuffer, numSamples);
        Response(outputBuffer, numSamples);
        CaptureScopeSamples(outputBuffer, numSamples);
    }

}