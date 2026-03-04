#pragma once 

#include <string>
#include <vector>

#include "WaveForm.h"

// -----VCO Module Config-----
struct VCOConfig
{
    WaveForm waveform{};
};

// -----LFO Module Config-----
struct LFOConfig
{
    WaveForm waveform{};
};

// -----VCF Module Config-----
struct VCFConfig
{
    FilterType filterType = FILTER_LowPass;
};

// -----VCA Module Config-----
struct VCAConfig
{
    float baseGain = 0.8f;
    float cvAmount = 0.5f;
    float cvInput = 0.5f;

    float attackTime = 10.0f;
    float releaseTime = 50.0f;
    float envelopeState = 0.0f;
    bool envelopeActive = false;
    bool useAttackandRelease = false;

    // Dynamic Range Control
    float rangeMin = 0.0f;
    float rangeMax = 1.0f;
};

// -----Output Module Config-----
struct OutputConfig
{
    float outputLevel = 0.8f;
};

// -----Main Module Struct-----
struct Module
{
    ModuleType Type;
    int ID;
    std::string Name;
    bool Active = true;

    int OutPins = 1;
    int InPins = 0;

    Module() {}
    
    Module(ModuleType type) : Type(type) {
        InPins = (type == MODULE_VCO) ? 0 : 1;
    }

    VCOConfig vcoConfig;
    LFOConfig lfoConfig;
    VCFConfig vcfConfig;
    VCAConfig vcaConfig;
    OutputConfig outputConfig;
};