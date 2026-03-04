#pragma once 

#include "../include/WaveForm.h"

namespace LFO 
{
    void MainImGui();
    void DrawLFOEditor(WaveForm& lfoWave);
    void ProcessAudio(WaveForm& lfoWave, float* inputBuffer, float* outputBuffer, int numSamples);
}