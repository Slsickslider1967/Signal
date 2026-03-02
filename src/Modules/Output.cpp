#include <iostream>
#include "imgui.h"
#include "../../include/Functions/ImGuiUtil.h"
#include "../../include/Output.h"
#include "../../include/Functions/Audio.h"

namespace Output
{
    // --Variables--
    static float outputLevel = 0.8f;
    static float *audioBuffer = nullptr;
    static int bufferSize = 5120;

    // --Function--
    void MainImGui()
    {
        ImGui::SliderFloat("Volume", &outputLevel, 0.0f, 1.0f);
    }

    void ProcessAudio(float *inputBuffer, int numSamples)
    {
        for (int sampleIndex = 0; sampleIndex < numSamples; sampleIndex++)
        {
            inputBuffer[sampleIndex] *= outputLevel;
        }
    }

    void Initialize()
    {
        audioBuffer = new float[bufferSize];
    }

    void Cleanup()
    {
        delete[] audioBuffer;
    }
}