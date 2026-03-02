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
    
    enum VoltageRange { 
        Bipolar5V = 0,   
        Bipolar10V,      
        Bipolar12V,      
        Bipolar15V       
    };
    VoltageRange vRange = Bipolar10V;
    
    // CV Inputs
    float vOctCV = 0.5f;       
    float linearFMCV = 0.5f;    // Fewquancy modulation
    float pwmCV = 0.5f;         
    bool syncInput = false;   
    
    // VCO Controls
    float coarseTune = 440.0f;  
    float fineTune = 0.0f;     
    int octave = 0;       
    
    // FM depth control 
    float fmDepth = 0.0f;  
    
    // === VCO INTERNALS ===
    
    float Frequency = 440.0f;          
    float outputAmplitude = 1.0f;      
    int SampleRate = 44100;           
    double Phase = 0.0;      
    int displayOffset = 0;
    
    // === VOLTAGE OUTPUT ===
    float currentVoltageOut = 0.0f;  
    float Amplitude = 1.0f;        
};

void GetWaveFormData(WaveForm& wave, float* buffer, int bufferSize, int startSample = 0);

float NormalizedToVoltage(float normalized, WaveForm::VoltageRange range);
float VoltageToNormalized(float voltage, WaveForm::VoltageRange range);

namespace WaveFormGen
{
    extern std::list<WaveForm> WaveForms;
    void MainImgui();
    void DrawWaveFormEditor(WaveForm& wave);
}