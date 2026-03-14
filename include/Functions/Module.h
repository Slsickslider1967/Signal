#pragma once 

#include <string>
#include <vector>

#include "WaveForm.h"
#include "VoltageControllFilter.h"
#include "MDU/mduParser.h"


// File will most likely be rewritten to be worked with .mdu

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

// This is the old Module struct probs keep for backwards compat
struct Module
{
    ModuleType Type;
    int ID;
    std::string Name;
    bool Active = true;

    int OutPins = 1;
    int InPins = 0;

    Module(ModuleType type = MODULE_VCO) : Type(type) {
        if (type == MODULE_VCO) {
            InPins = 4;   // V/Oct, FM CV, PWM CV, Sync
            OutPins = 3;
        }
        else if (type == MODULE_LFO) {
            InPins = 2;   // Rate CV, Reset/Sync
            OutPins = 3;
        }
        else if (type == MODULE_VCF) {
            InPins = 3;   // Audio In, Cutoff CV, Resonance CV
            OutPins = 1;
        }
        else if (type == MODULE_VCA) {
            InPins = 2;   // Audio In, CV/Envelope In
            OutPins = 1;
        }
        else if (type == MODULE_OUTPUT) {
            InPins = 1;   // Audio In
            OutPins = 0;
        }
        else {
            InPins = 1;
            OutPins = 1;
        }
    }

    VCOConfig vcoConfig;
    LFOConfig lfoConfig;
    VCFConfig vcfConfig;
    VCAConfig vcaConfig;
    OutputConfig outputConfig;
};

// New dynamic module struct to be used with .mdu
struct DynamicModule
{
    int ID = -1;
    std::string Name;
    bool Active = true;

    std::string SourcePath;
    MDU::MetaData Metadata;

    MDU::Module* Instance = nullptr;
    MDU::DestroyFn Destroy = nullptr;

    int InPins = 0;
    int OutPins = 0;
};