#pragma once

#include <vector>
#include "WaveForm.h"

namespace Audio
{
    void Init();

    void Close();

    void Play(const WaveForm& wave);
    
    void SetWaveForms(const std::vector<WaveForm>& waves);

    void WriteAudio(float* buffer, int numSamples);
    
    // Filter callback for rack processing
    typedef void (*FilterCallback)(float* buffer, int numSamples, void* userData);
    void SetFilterCallback(FilterCallback callback, void* userData);
}