#pragma once

struct Module;

namespace Output
{
    void MainImGui();
    void DrawModuleEditor(Module &module, bool &requestRemove);
    void ProcessAudio(float *inputBuffer, int numSamples);
    void Initialize();
    void Cleanup();
}