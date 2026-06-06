#pragma once

#include <juce_core/juce_core.h>

enum class AnalyzerDbRange
{
    db60 = 0,
    db80,
    db100,
    count
};

inline juce::StringArray getAnalyzerDbRangeChoices()
{
    return { "60 dB", "80 dB", "100 dB" };
}

inline AnalyzerDbRange analyzerDbRangeFromIndex (int index) noexcept
{
    constexpr auto maxIndex = static_cast<int> (AnalyzerDbRange::count) - 1;
    return static_cast<AnalyzerDbRange> (juce::jlimit (0, maxIndex, index));
}

inline AnalyzerDbRange analyzerDbRangeFromParameterValue (float value) noexcept
{
    return analyzerDbRangeFromIndex (juce::roundToInt (value));
}

inline float analyzerDbRangeMinimumDecibelsFromIndex (int index) noexcept
{
    switch (juce::jlimit (0, static_cast<int> (AnalyzerDbRange::count) - 1, index))
    {
        case static_cast<int> (AnalyzerDbRange::db60):  return -60.0f;
        case static_cast<int> (AnalyzerDbRange::db80):  return -80.0f;
        case static_cast<int> (AnalyzerDbRange::db100): return -100.0f;
        default:                                        return -100.0f;
    }
}

inline float analyzerDbRangeMinimumDecibelsFromParameterValue (float value) noexcept
{
    return analyzerDbRangeMinimumDecibelsFromIndex (juce::roundToInt (value));
}
