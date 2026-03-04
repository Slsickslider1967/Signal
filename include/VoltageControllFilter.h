#pragma once

enum FilterType
{
    FILTER_LowPass,
    FILTER_HighPass,
    FILTER_BandPass,
    FILTER_NOTCH
};

namespace VCF
{
    void DrawFilterTypeEditor(FilterType& filterType);
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