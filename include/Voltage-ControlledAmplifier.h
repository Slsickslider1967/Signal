#pragma once 

struct Module;

namespace VCA 
{
    void MainImGui();
    void DrawModuleEditor(Module &module, bool &requestRemove);
    void SetCVInput(float value);
    void ProcessAudio(float* inputBuffer, float* outputBuffer, int numSamples);
    void ProcessAudioWithCVBuffer(float* inputBuffer, float* outputBuffer, int numSamples, const float* cvBuffer, int cvBufferSize);
}