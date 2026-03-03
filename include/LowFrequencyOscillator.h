#pragma once 

namespace LFO 
{
    void MainImGui();
    void ProcessAudio(float* inputBuffer, float* outputBuffer, int numSamples);
}