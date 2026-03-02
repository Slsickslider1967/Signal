#pragma once

namespace Output
{
    void MainImGui();
    void ProcessAudio(float *inputBuffer, int numSamples);
    void Initialize();
    void Cleanup();
}