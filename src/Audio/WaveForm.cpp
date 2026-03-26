#include <cmath>
#include <random>
#include "WaveForm.h"
#include "Audio/CV.h"


float NormalizedToVoltage(float normalized, WaveForm::VoltageRange range)
{
    double maxVoltage = 10.0;
    switch (range)
    {
        case WaveForm::Bipolar5V: maxVoltage = 5.0; break;
        case WaveForm::Bipolar10V: maxVoltage = 10.0; break;
        case WaveForm::Bipolar12V: maxVoltage = 12.0; break;
        case WaveForm::Bipolar15V: maxVoltage = 15.0; break;
        default: maxVoltage = 10.0; break;
    }
    return (normalized * 2.0f - 1.0f) * maxVoltage;
}

float VoltageToNormalized(float voltage, WaveForm::VoltageRange range)
{
    double maxVoltage = 10.0;
    switch (range)
    {
        case WaveForm::Bipolar5V: maxVoltage = 5.0; break;
        case WaveForm::Bipolar10V: maxVoltage = 10.0; break;
        case WaveForm::Bipolar12V: maxVoltage = 12.0; break;
        case WaveForm::Bipolar15V: maxVoltage = 15.0; break;
        default: maxVoltage = 10.0; break;
    }
    return (voltage / maxVoltage + 1.0f) * 0.5f;
}

void GetWaveFormData(WaveForm& wave, float* buffer, int bufferSize, int startSample)
{
    if (!buffer || bufferSize <= 0) return;
    if (wave.SampleRate <= 0) return;
    
    const double twoPi = 6.283185307179586476925286766559;

    double phase = wave.Phase;
    phase = std::fmod(phase, twoPi);
    
    static thread_local std::mt19937 noiseGen(std::random_device{}());
    std::uniform_real_distribution<float> noiseDist(-1.0f, 1.0f);

    double maxVoltage = 10.0;
    switch (wave.vRange) {
        case WaveForm::Bipolar5V: maxVoltage = 5.0; break;
        case WaveForm::Bipolar10V: maxVoltage = 10.0; break;
        case WaveForm::Bipolar12V: maxVoltage = 12.0; break;
        case WaveForm::Bipolar15V: maxVoltage = 15.0; break;
        default: maxVoltage = 10.0; break;
    }

    double vOctVoltage = NormalizedToVoltage(wave.vOctCV, wave.vRange);
    double octaveFromCV = vOctVoltage / 1.0; 
    
    double totalOctaveShift = wave.octave + octaveFromCV;
    double fineTuneRatio = std::pow(2.0, wave.fineTune / 1200.0);
    double baseFreq = wave.coarseTune * std::pow(2.0, totalOctaveShift) * fineTuneRatio;
    
    double fmFrequencyOffset = 0.0;
    if (wave.fmDepth > 0.0)
    {
        double fmVoltage = NormalizedToVoltage(wave.linearFMCV, wave.vRange);
        double normalizedFM = fmVoltage / maxVoltage; 
        fmFrequencyOffset = CV::ScaleCV(normalizedFM, wave.fmDepth) * baseFreq * 2.0;
    }
    
    double finalFreq = baseFreq + fmFrequencyOffset;
    if (finalFreq < 0.1) finalFreq = 0.1;
    
    wave.Frequency = finalFreq; 

    double phaseIncrement = twoPi * finalFreq / static_cast<double>(wave.SampleRate);
    
    if (wave.syncInput)
    {
        phase = 0.0;
        wave.syncInput = false;
    }

    // === GENERATE VCO WAVEFORM SAMPLES ===
    for (int sampleIndex = 0; sampleIndex < bufferSize; ++sampleIndex)
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
                double normalizedPhase = (phase / twoPi);
                double phaseFraction = normalizedPhase - std::floor(normalizedPhase);
                value = 2.0 * (phaseFraction - 0.5);
            }
            break;
            
        case Triangle:
            {
                double normalizedPhase = (phase / twoPi);
                double phaseFraction = normalizedPhase - std::floor(normalizedPhase);
                value = 2.0 * std::abs(2.0 * phaseFraction - 1.0) - 1.0;
            }
            break;
            
        case Pulse: 
            {
                double normalizedPhase = (phase / twoPi);
                double phaseFraction = normalizedPhase - std::floor(normalizedPhase);
                double pulseWidth = CV::ClampCV(wave.pwmCV, {0.05f, 0.95f});
                value = (phaseFraction < pulseWidth) ? 1.0 : -1.0;
            }
            break;
            
        case Noise: 
            value = noiseDist(noiseGen);
            break;
            
        default:
            value = std::sin(phase);
            break;
        }
        
        double normalizedOutput = value * wave.Amplitude;
        
        if (normalizedOutput > 1.0) normalizedOutput = 1.0;
        if (normalizedOutput < -1.0) normalizedOutput = -1.0;
        
        buffer[sampleIndex] = static_cast<float>(normalizedOutput);
        wave.currentVoltageOut = static_cast<float>(normalizedOutput * maxVoltage);

        phase += phaseIncrement;
        
        if (phase >= twoPi) phase -= twoPi;
        if (phase < 0.0) phase += twoPi;
    }
    
    // Store phase state
    wave.Phase = phase;
}
