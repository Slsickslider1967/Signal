#include <cmath>
#include <random>
#include "../../include/WaveForm.h"

// === VOLTAGE UTILITY FUNCTIONS ===

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
    // Map 0..1 to -maxVoltage..+maxVoltage
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
    // Map -maxVoltage..+maxVoltage to 0..1
    return (voltage / maxVoltage + 1.0f) * 0.5f;
}

// === VCO SIGNAL GENERATION (PURE OSCILLATOR) ===

// === VCO SIGNAL GENERATION (PURE OSCILLATOR) ===

void GetWaveFormData(WaveForm& wave, float* buffer, int bufferSize, int startSample)
{
    // Safety checks
    if (!buffer || bufferSize <= 0) return;
    if (wave.SampleRate <= 0) return;
    
    const double twoPi = 6.283185307179586476925286766559;

    // Initialize phase
    double phase = wave.Phase;
    phase = std::fmod(phase, twoPi);
    
    // Create a local noise generator for this call (thread-safe)
    static thread_local std::mt19937 noiseGen(std::random_device{}());
    std::uniform_real_distribution<float> noiseDist(-1.0f, 1.0f);

    // Get voltage range max
    double maxVoltage = 10.0;
    switch (wave.vRange) {
        case WaveForm::Bipolar5V: maxVoltage = 5.0; break;
        case WaveForm::Bipolar10V: maxVoltage = 10.0; break;
        case WaveForm::Bipolar12V: maxVoltage = 12.0; break;
        case WaveForm::Bipolar15V: maxVoltage = 15.0; break;
        default: maxVoltage = 10.0; break;
    }

    // === COMPUTE FREQUENCY FROM V/Oct CV ===
    // 1V/octave standard: 1V = 1 octave change
    // vOctCV of 0.5 = 0V = no change
    double vOctVoltage = NormalizedToVoltage(wave.vOctCV, wave.vRange);
    double octaveFromCV = vOctVoltage / 1.0;  // 1V = 1 octave
    
    // Combine all pitch controls
    double totalOctaveShift = wave.octave + octaveFromCV;
    double fineTuneRatio = std::pow(2.0, wave.fineTune / 1200.0);  // cents to ratio
    double baseFreq = wave.coarseTune * std::pow(2.0, totalOctaveShift) * fineTuneRatio;
    
    // Apply Linear FM if enabled
    double fmFrequencyOffset = 0.0;
    if (wave.fmDepth > 0.0)
    {
        double fmVoltage = NormalizedToVoltage(wave.linearFMCV, wave.vRange);
        fmFrequencyOffset = (fmVoltage / maxVoltage) * wave.fmDepth * baseFreq * 2.0;  // Linear FM
    }
    
    double finalFreq = baseFreq + fmFrequencyOffset;
    if (finalFreq < 0.1) finalFreq = 0.1;  // Prevent negative/zero frequency
    
    wave.Frequency = finalFreq;  // Store computed frequency

    // === PHASE INCREMENT ===
    double phaseIncrement = twoPi * finalFreq / static_cast<double>(wave.SampleRate);
    
    // Handle hard sync at buffer start
    if (wave.syncInput)
    {
        phase = 0.0;
        wave.syncInput = false;  // Reset trigger
    }

    // === GENERATE VCO WAVEFORM SAMPLES ===
    for (int sampleIndex = 0; sampleIndex < bufferSize; ++sampleIndex)
    {
        double value = 0.0;
        
        // Generate waveform voltage with bounds checking
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
            
        case Pulse:  // Variable pulse width controlled by PWM CV
            {
                double normalizedPhase = (phase / twoPi);
                double phaseFraction = normalizedPhase - std::floor(normalizedPhase);
                double pulseWidth = std::max(0.05, std::min(0.95, (double)wave.pwmCV));
                value = (phaseFraction < pulseWidth) ? 1.0 : -1.0;
            }
            break;
            
        case Noise:  // White noise generator
            value = noiseDist(noiseGen);
            break;
            
        default:
            // Default to sine if invalid type
            value = std::sin(phase);
            break;
        }
        
        // Scale to output voltage range
        double normalizedOutput = value * wave.Amplitude;
        
        // Clip to voltage rails
        if (normalizedOutput > 1.0) normalizedOutput = 1.0;
        if (normalizedOutput < -1.0) normalizedOutput = -1.0;
        
        buffer[sampleIndex] = static_cast<float>(normalizedOutput);
        wave.currentVoltageOut = static_cast<float>(normalizedOutput * maxVoltage);  // Store actual voltage for monitoring

        // Advance oscillator phase
        phase += phaseIncrement;
        
        // Wrap phase
        if (phase >= twoPi) phase -= twoPi;
        if (phase < 0.0) phase += twoPi;
    }
    
    // Store phase state
    wave.Phase = phase;
}
