#pragma once

#include <list>
#include <map>
#include <vector>

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
    
    // --VCO INPUTS--
    
    enum VoltageRange { 
        Bipolar5V = 0,   
        Bipolar10V,      
        Bipolar12V,      
        Bipolar15V       
    };
    VoltageRange vRange = Bipolar10V;
    
    float vOctCV = 0.5f;       
    float linearFMCV = 0.5f;    // Fewquancy modulation
    float pwmCV = 0.5f;         
    bool syncInput = false;   

    float coarseTune = 440.0f;  
    float fineTune = 0.0f;     
    int octave = 0;       
    
    float fmDepth = 0.0f;  
    
    float Frequency = 440.0f;          
    float outputAmplitude = 1.0f;      
    int SampleRate = 44100;           
    double Phase = 0.0;      
    int displayOffset = 0;
    
    float currentVoltageOut = 0.0f;  
    float Amplitude = 1.0f;        
};

void GetWaveFormData(WaveForm& wave, float* buffer, int bufferSize, int startSample = 0);

float NormalizedToVoltage(float normalized, WaveForm::VoltageRange range);
float VoltageToNormalized(float voltage, WaveForm::VoltageRange range);

struct Module;
struct Link;

namespace WaveFormGen
{
    extern std::list<WaveForm> WaveForms;
    void MainImgui();
    void DrawModuleEditor(Module &module, bool &requestRemove);
    void DrawWaveFormEditor(WaveForm& wave);
    void InitializeVCOWaveForm(WaveForm& wave, int moduleID);

    std::map<int, std::vector<float>> GenerateLFOOutputs(std::list<Module>& modules, int numSamples);
    std::map<int, std::vector<float>> BuildNormalizedCVInputs(
        const std::list<Module>& modules,
        const std::vector<Link>& links,
        const std::map<int, std::vector<float>>& lfoOutputs);
}

// --Module Type Definitions--
enum ModuleType
{
    MODULE_VCO,
    MODULE_LFO,
    MODULE_VCF,
    MODULE_VCA,
    MODULE_OUTPUT
};

// --Link Structure--
struct Link
{
    int ID;
    int StartModuleID;
    int EndModuleID;
    int StartPinIndex = 0;
    int EndPinIndex = 0;
};
