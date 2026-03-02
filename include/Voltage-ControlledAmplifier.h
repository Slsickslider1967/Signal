#pragma once 

namespace VCA 
{
    void MainImGui();
    void ProcessAudio(float* inputBuffer, float* outputBuffer, int numSamples);
}