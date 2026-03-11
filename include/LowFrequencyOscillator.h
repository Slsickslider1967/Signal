#pragma once 

#include "../include/WaveForm.h"

struct Module;

namespace LFO 
{
    void MainImGui();
    void DrawModuleEditor(Module &module, bool &requestRemove);
    void InitializeLFOWaveForm(WaveForm& lfoWave, int moduleID);
    void DrawLFOEditor(WaveForm& lfoWave);
    void ProcessAudio(WaveForm& lfoWave, float* inputBuffer, float* outputBuffer, int numSamples);
}