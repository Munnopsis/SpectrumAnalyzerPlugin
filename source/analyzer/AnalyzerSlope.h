#pragma once

#include <juce_core/juce_core.h>

enum class AnalyzerSlope
{
    flat = 0,
    db3PerOctave,
    db45PerOctave,
    count
};

inline juce::StringArray getAnalyzerSlopeChoices()
{
    return { "0 dB/oct", "3 dB/oct", "4.5 dB/oct" };
}

inline float analyzerSlopeDbPerOctaveFromIndex (int index) noexcept
{
    switch (juce::jlimit (0, static_cast<int> (AnalyzerSlope::count) - 1, index))
    {
        case static_cast<int> (AnalyzerSlope::flat):          return 0.0f;
        case static_cast<int> (AnalyzerSlope::db3PerOctave):  return 3.0f;
        case static_cast<int> (AnalyzerSlope::db45PerOctave): return 4.5f;
        default:                                              return 0.0f;
    }
}

inline float analyzerSlopeDbPerOctaveFromParameterValue (float value) noexcept
{
    return analyzerSlopeDbPerOctaveFromIndex (juce::roundToInt (value));
}
