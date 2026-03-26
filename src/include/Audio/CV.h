#pragma once

namespace CV
{
    enum class CVFunction
    {
        Linear,
        Exponential
    };


    struct Range 
    {
        float min;
        float max;
    };  

    // --Core--
    float ClampCV(float value, Range range);

    // --Normalized Conversion-- 
    float NormalizeCV(float value, Range range);
    float DenormalizeCV(float normalizedValue, Range range);

    // --Modular Controll--
    float ScaleCV(float cvValue, float amount);
    float AttuneCV(float cvValue, float attenuverterAmount);
    float OffsetCV(float cvValue, float offsetValue);

    // --Mapping param--
    float MapCV(float value, Range inRange, Range outRange, CVFunction curve = CVFunction::Linear);

    // --Modulation safty--
    float ModulateParameter(
        float baseValue,
        float cvValue,
        float cvAmount,
        Range cvRange,
        Range paramRange,
        CVFunction curve = CVFunction::Linear);
} 
