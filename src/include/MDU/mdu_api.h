#pragma once 

#include <cstddef>
#include <string>
#include <vector>

using namespace std;

// Noo wayy is the MDU namespace lmao
// --Module Development Utility (MDU)--
// Basicly a tool kit for making your own modules and a better way for me to manage the modules

namespace MDU
{
    enum class ParameterType
    {
        Knob,
        Slider,
        Stepped,
        Toggle,
        Combo
    };

    // For pins being IN and OUT shoud probs delecated a range for in out as this could cause issues when 
    // adding pins. For now well be okay because i know what im doing
    struct PinDefinition
    {
        string ID;
        string label;
    };

    struct ParameterDefinition
    {
        string ID;
        string label;
        ParameterType type = ParameterType::Knob;
        float minValue = 0.0f;
        float maxValue = 1.0f;
        float defaultValue = 0.0f;
        vector<string> options; 
    };

    struct MetaData
    {
        string ModuleName;
        string ModuleType;
        string ModuleVersion;
        string Author;

        vector<PinDefinition> InputPins;
        vector<PinDefinition> OutputPins;
        vector<ParameterDefinition> Parameters;
        vector<string> Dependancies;
    };

    // Pointers to the MetaData and Param
    struct BufferView 
    {
        vector<const float*> InputPins;
        vector<float*> OutputPins; 
        size_t NumberOfSamples = 0;
        int SampleRate = 44100;
        int VoltageRange = 0;  // 0: ±5V, 1: ±10V, 2: ±12V, 3: ±15V
    };

    class Module
    {
        public:
            virtual ~Module() = default;

            virtual void Process(const BufferView& ID, float value) = 0;
            virtual void DrawEditor() = 0;
    };

    using CreateFn = Module* (*)();
    using DestroyFn = void (*)(Module*);
    using GetMetaDataFn = const MetaData* (*)();
}

// As C defines \ to designate end of line
#define MDU_REGISTER(ModuleClass) \
extern "C" MDU::Module* mdu_create() \
{ \
    return new ModuleClass(); \
} \
extern "C" void mdu_destroy(MDU::Module* module) \
{ \
    delete module; \
} \
extern "C" const MDU::MetaData* mdu_get_metadata() \
{ \
    static MDU::MetaData metadata = ModuleClass::BuildMetadata(); \
    return &metadata; \
}
