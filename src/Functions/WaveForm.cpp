#include <cmath>
#include "../../include/WaveForm.h"

void GetWaveFormData(WaveForm& wave, float* buffer, int bufferSize, int startSample)
{
    if (!buffer || bufferSize <= 0) return;
    const double twoPi = 6.283185307179586476925286766559;
    if (wave.SampleRate <= 0) return;

    // use per-wave phase state; startSample is ignored when per-wave phase is available
    double phase = wave.Phase;
    // ensure phase is within range
    phase = std::fmod(phase, twoPi);
    double delta = twoPi * wave.Frequency * wave.Speed / static_cast<double>(wave.SampleRate);

    for (int i = 0; i < bufferSize; ++i)
    {
        double value = 0.0;
        switch (wave.Type)
        {
        case Sine:
            value = std::sin(phase);
            break;
        case Square:
            value = (std::sin(phase) >= 0.0) ? 1.0 : -1.0;
            break;
        case Sawtooth:
            {
                double x = (phase / twoPi); // fractional cycles since wrapped
                double frac = x - std::floor(x);
                value = 2.0 * (frac - 0.5);
            }
            break;
        case Triangle:
            {
                double x = (phase / twoPi);
                double frac = x - std::floor(x);
                value = 2.0 * std::abs(2.0 * (frac) - 1.0) - 1.0;
            }
            break;
        case Tangent:
            {
                double v = std::tan(phase);
                if (v > 1.0) v = 1.0;
                if (v < -1.0) v = -1.0;
                value = v;
            }
            break;
        default:
            value = 0.0;
            break;
        }

        buffer[i] = static_cast<float>(value * wave.Amplitude);

        phase += delta;
        if (phase >= twoPi || phase <= -twoPi) phase = std::fmod(phase, twoPi);
    }

    // store updated phase back to waveform state
    wave.Phase = phase;
}
