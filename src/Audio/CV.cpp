#include <iostream>
#include <vector>
#include <list>
#include <cmath>

#include "Audio/CV.h"
#include "WaveForm.h"
#include "Audio/Audio.h"

namespace CV
{
    // --Variables--

    // --structs and enums--

    // --Prototypes--

    // --Functions--
    // Clamp a control value to the provided numeric range.
    float ClampCV(float value, Range range)
    {
        if (value < range.min)
            return range.min;
        if (value > range.max)
            return range.max;
        return value;
    }

    // Convert a value from its native range into normalized 0..1 space.
    float NormalizeCV(float value, Range range)
    {
        if (range.max <= range.min)
            return 0.0f;

        const float normalized = (value - range.min) / (range.max - range.min);
        return ClampCV(normalized, {0.0f, 1.0f});
    }

    // Convert a normalized 0..1 value back into a target range.
    float DenormalizeCV(float normalizedValue, Range range)
    {
        const float clamped = ClampCV(normalizedValue, {0.0f, 1.0f});
        return range.min + (range.max - range.min) * clamped;
    }

    // Apply a simple scalar gain to a CV signal.
    float ScaleCV(float cvValue, float amount)
    {
        return cvValue * amount;
    }

    // Apply attenuverter-style scaling, including inversion when negative.
    float AttuneCV(float cvValue, float attenuverterAmount)
    {
        return cvValue * attenuverterAmount;
    }

    // Add a DC offset to a CV signal.
    float OffsetCV(float cvValue, float offsetValue)
    {
        return cvValue + offsetValue;
    }

    // Map a value from one range to another using linear or exponential shaping.
    float MapCV(float value, Range inRange, Range outRange, CVFunction curve)
    {
        float normalized = NormalizeCV(value, inRange);

        if (curve == CVFunction::Exponential)
        {
            normalized = normalized * normalized;
        }

        return DenormalizeCV(normalized, outRange);
    }

    // Turn an incoming CV into a bipolar modulation amount and apply it to a parameter.
    float ModulateParameter(
        float baseValue,
        float cvValue,
        float cvAmount,
        Range cvRange,
        Range paramRange,
        CVFunction curve)
    {
        const float normalizedCV = NormalizeCV(cvValue, cvRange);
        const float modulation = (normalizedCV - 0.5f) * 2.0f * cvAmount;
        const float modulatedNormalized = ClampCV(0.5f + (baseValue + modulation - 0.5f), {0.0f, 1.0f});

        if (curve == CVFunction::Exponential)
        {
            return MapCV(modulatedNormalized, {0.0f, 1.0f}, paramRange, CVFunction::Exponential);
        }

        return MapCV(modulatedNormalized, {0.0f, 1.0f}, paramRange, CVFunction::Linear);
    }

}