#pragma once

namespace VCF
{
    void MainImGui();
    void ApplyLowPass(float *inputBuffer, float *outputBuffer, int bufferSize);
    void ApplyHighPass(float *inputBuffer, float *outputBuffer, int bufferSize);
    void ApplyBandPass(float *inputBuffer, float *outputBuffer, int bufferSize);
    void ApplyNotch(float *inputBuffer, float *outputBuffer, int bufferSize);
    void ProcessAudio(float *inputBuffer, float *outputBuffer, int bufferSize, int filterType);
}

namespace SpeedManipulation
{
    void SpeedManipulationMain();
}