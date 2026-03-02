#pragma once

#include <list>

// VCO Waveform Types
enum WaveType
{
    Sine,           
    Square,         
    Sawtooth,       
    Triangle,        
    Pulse,          
    Noise           // White noise 
};

// VCO 
struct WaveForm
{
    int WaveID;
    bool Enabled;
    bool OpenWindow = true;
    bool RequestDockBelow = false;
    WaveType Type;
    
    // === VCO INPUTS ) ===
    
    // Voltage selection 
    enum VoltageRange { 
        Bipolar5V = 0,   
        Bipolar10V,      
        Bipolar12V,      
        Bipolar15V       
    };
    VoltageRange vRange = Bipolar10V;
    
    // CV Inputs (like patch jacks on a real VCO)
    float vOctCV = 0.5f;        // V/Oct input (1V = 1 octave, the standard)
    float linearFMCV = 0.5f;    // Linear FM input (direct frequency modulation)
    float pwmCV = 0.5f;         // PWM input (pulse width modulation, for Pulse wave)
    bool syncInput = false;     // Hard sync input (resets phase)
    
    // VCO Controls (knobs on the front panel)
    float coarseTune = 440.0f;  // Coarse frequency knob (Hz)
    float fineTune = 0.0f;      // Fine tune knob (-100 to +100 cents)
    int octave = 0;             // Octave switch (-4 to +4)
    
    // FM depth control (how much linearFMCV affects frequency)
    float fmDepth = 0.0f;       // 0 = no FM, 1 = full FM range
    
    // === VCO INTERNALS ===
    
    float Frequency = 440.0f;          // Current output frequency (computed)
    float outputAmplitude = 1.0f;      // Output level trim (typically ±5V or ±10V)
    int SampleRate = 44100;            // Sample rate (must be initialized!)
    double Phase = 0.0;                // Oscillator phase
    int displayOffset = 0;
    
    // === VOLTAGE OUTPUT ===
    float currentVoltageOut = 0.0f;  // Current output voltage for monitoring
    float Amplitude = 1.0f;         // For compatibility with existing code
};

// Generate VCO voltage signal
void GetWaveFormData(WaveForm& wave, float* buffer, int bufferSize, int startSample = 0);

// Voltage utility functions
float NormalizedToVoltage(float normalized, WaveForm::VoltageRange range);
float VoltageToNormalized(float voltage, WaveForm::VoltageRange range);

// WaveFormGen namespace (for accessing VCO list)
namespace WaveFormGen
{
    extern std::list<WaveForm> WaveForms;
    void MainImgui();
}