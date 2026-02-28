#pragma once

#include <vector>
#include "../WaveForm.h"

namespace Audio
{
    // Initialize the audio subsystem. Call once at startup.
    void Init();

    // Close the audio subsystem. Call at shutdown.
    void Close();

    // Play a single waveform (plays a short buffer, non-blocking).
    void Play(const WaveForm& wave);
    
    // Update the set of waveforms that are mixed for continuous playback.
    // Call this when wave parameters change; Enabled controls per-wave on/off.
    void SetWaveForms(const std::vector<WaveForm>& waves);
}