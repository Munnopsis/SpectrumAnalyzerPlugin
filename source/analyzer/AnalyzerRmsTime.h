#pragma once

#include <juce_core/juce_core.h>

enum class AnalyzerRmsTime
{
    ms100 = 0,
    ms300,
    ms600,
    s1,
    s2,
    count
};

inline juce::StringArray getAnalyzerRmsTimeChoices()
{
    return {
        "RMS: 100 ms",
        "RMS: 300 ms",
        "RMS: 600 ms",
        "RMS: 1 s",
        "RMS: 2 s"
    };
}

inline float analyzerRmsTimeSecondsFromIndex (int index) noexcept
{
    switch (juce::jlimit (0, static_cast<int> (AnalyzerRmsTime::count) - 1, index))
    {
        case static_cast<int> (AnalyzerRmsTime::ms100): return 0.100f;
        case static_cast<int> (AnalyzerRmsTime::ms300): return 0.300f;
        case static_cast<int> (AnalyzerRmsTime::ms600): return 0.600f;
        case static_cast<int> (AnalyzerRmsTime::s1):    return 1.000f;
        case static_cast<int> (AnalyzerRmsTime::s2):    return 2.000f;
        default:                                        return 0.300f;
    }
}

inline float analyzerRmsTimeSecondsFromParameterValue (float value) noexcept
{
    return analyzerRmsTimeSecondsFromIndex (juce::roundToInt (value));
}