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
    bool OpenWindow = true; // whether the per-wave window is open/visible
    bool RequestDockBelow = false; // request to dock this window below others on next frame
    WaveType Type;
    // Control voltage (normalized 0..1). Mapped to actual voltage using `VoltageRange`.
    float cv = 0.5f;

    // Voltage standard selection (affects how CV is interpreted)
    enum VoltageRange { Bipolar5V = 0, Bipolar10V, Bipolar12V, Bipolar15V };
    VoltageRange vRange = Bipolar5V;

    // Additional timbre controls useful for eurorack/serge styles
    float foldAmount = 0.0f;    // wavefolding amount (0..1)
    float harmonicMix = 0.0f;   // mix amount for added 2nd harmonic (0..1)
    // CV destination routing
    enum CVDestination { CV_None = 0, CV_Frequency, CV_Amplitude, CV_Drive };
    CVDestination cvDest = CV_None;
    float Frequency;
    float Amplitude;
    int SampleRate;
    float Speed = 1.0f; // independent multiplier for phase advance (not frequency)
    int displayOffset = 0;
    double Phase = 0.0; // per-wave phase state (radians)
};

void GetWaveFormData(WaveForm& wave, float* buffer, int bufferSize, int startSample = 0);