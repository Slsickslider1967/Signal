#pragma once

enum WaveType
{
    Sine,
    Tangent,
    Square,
    Sawtooth,
    Triangle
};

struct WaveForm
{
    int WaveID;
    bool Enabled;
    WaveType Type;
    float Frequency;
    float Amplitude;
    int SampleRate;
    float Speed = 1.0f; // independent multiplier for phase advance (not frequency)
    int displayOffset = 0;
    double Phase = 0.0; // per-wave phase state (radians)
};

void GetWaveFormData(WaveForm& wave, float* buffer, int bufferSize, int startSample = 0);