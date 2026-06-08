#pragma once

#include <juce_core/juce_core.h>

enum class AnalyzerSlope
{
    flat = 0,
    musicTilt,
    whiteFlat,
    count
};

inline juce::StringArray getAnalyzerSlopeChoices()
{
    return {
        "Weight: None",
        "Weight: Music Tilt",
        "Weight: White Flat"
    };
}

inline float analyzerSlopeDbPerOctaveFromIndex (int index) noexcept
{
    switch (juce::jlimit (0, static_cast<int> (AnalyzerSlope::count) - 1, index))
    {
        case static_cast<int> (AnalyzerSlope::flat):      return 0.0f;
        case static_cast<int> (AnalyzerSlope::musicTilt): return 4.5f;
        case static_cast<int> (AnalyzerSlope::whiteFlat): return -3.0f;
        default:                                          return 0.0f;
    }
}

inline float analyzerSlopeDbPerOctaveFromParameterValue (float value) noexcept
{
    return analyzerSlopeDbPerOctaveFromIndex (juce::roundToInt (value));
}
