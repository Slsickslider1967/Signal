#include <cmath>
#include "../../include/WaveForm.h"

void GetWaveFormData(WaveForm& wave, float* buffer, int bufferSize, int startSample)
{
    if (!buffer || bufferSize <= 0) return;
    const double twoPi = 6.283185307179586476925286766559;
    if (wave.SampleRate <= 0) return;

    // Wave state
    double phase = wave.Phase;
    phase = std::fmod(phase, twoPi);

    // Map normalized CV (0..1) to an actual bipolar voltage based on selected range
    double vMax = 5.0;
    switch (wave.vRange)
    {
        case WaveForm::Bipolar5V: vMax = 5.0; break;
        case WaveForm::Bipolar10V: vMax = 10.0; break;
        case WaveForm::Bipolar12V: vMax = 12.0; break;
        case WaveForm::Bipolar15V: vMax = 15.0; break;
        default: vMax = 5.0; break;
    }
    double cvVolt = (static_cast<double>(wave.cv) * 2.0 - 1.0) * vMax;

    // Determine effective parameters based on CV routing
    double freqEff = static_cast<double>(wave.Frequency);
    double ampEff = static_cast<double>(wave.Amplitude);
    double driveCVScale = 1.0;
    if (wave.cvDest == WaveForm::CV_Frequency)
    {
        // map normalized CV (-1..1) to +/-4 octaves
        double cvNorm = (static_cast<double>(wave.cv) - 0.5) * 2.0;
        double maxOctaves = 4.0;
        freqEff = freqEff * std::pow(2.0, cvNorm * maxOctaves);
    }
    else if (wave.cvDest == WaveForm::CV_Amplitude)
    {
        // map CV to amplitude multiplier (avoid zero)
        ampEff = ampEff * (0.1 + static_cast<double>(wave.cv) * 1.9);
    }
    else if (wave.cvDest == WaveForm::CV_Drive)
    {
        // increase drive proportionally when CV is large
        driveCVScale = 1.0 + (std::abs(cvVolt) / vMax);
    }

    double delta = twoPi * freqEff * wave.Speed / static_cast<double>(wave.SampleRate);

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
                double x = (phase / twoPi);
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
                value = std::tan(phase);
            }
            break;
        default:
            value = 0.0;
            break;
        }

        // Apply harmonic mixing (second harmonic) for timbral change across all types
        double secondHarm = std::sin(2.0 * phase);
        value = (1.0 - wave.harmonicMix) * value + (wave.harmonicMix) * secondHarm;

        // Apply folding/non-linear drive influenced by foldAmount and CV (Serge-like)
        if (wave.foldAmount > 0.0)
        {
            double drive = 1.0 + wave.foldAmount * (std::abs(cvVolt) / vMax) * 8.0;
            drive *= driveCVScale; // include CV-driven extra when routed
            value = std::tanh(value * drive);
        }

        // Apply amplitude and clamp (use ampEff)
        double out = value * ampEff;
        if (out > 1.0) out = 1.0;
        if (out < -1.0) out = -1.0;
        buffer[i] = static_cast<float>(out);

        phase += delta;
        if (phase >= twoPi || phase <= -twoPi) phase = std::fmod(phase, twoPi);
    }
    wave.Phase = phase;
}
