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
};

