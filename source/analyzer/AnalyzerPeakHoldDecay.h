#pragma once

#include <juce_core/juce_core.h>

enum class AnalyzerPeakHoldDecay
{
    hold = 0,
    slow,
    mediumSlow,
    medium,
    fast,
    veryFast,
    count
};

inline juce::StringArray getAnalyzerPeakHoldDecayChoices()
{
    return {
        "Peak: Hold",
        "Peak: 2 dB/s",
        "Peak: 4 dB/s",
        "Peak: 8 dB/s",
        "Peak: 16 dB/s",
        "Peak: 32 dB/s"
    };
}

inline float analyzerPeakHoldDecayFromIndex (int index) noexcept
{
    switch (juce::jlimit (0, static_cast<int> (AnalyzerPeakHoldDecay::count) - 1, index))
    {
        case static_cast<int> (AnalyzerPeakHoldDecay::hold):       return 0.0f;
        case static_cast<int> (AnalyzerPeakHoldDecay::slow):       return 2.0f;
        case static_cast<int> (AnalyzerPeakHoldDecay::mediumSlow): return 4.0f;
        case static_cast<int> (AnalyzerPeakHoldDecay::medium):     return 8.0f;
        case static_cast<int> (AnalyzerPeakHoldDecay::fast):       return 16.0f;
        case static_cast<int> (AnalyzerPeakHoldDecay::veryFast):   return 32.0f;
        default:                                                   return 8.0f;
    }
}

inline float analyzerPeakHoldDecayFromParameterValue (float value) noexcept
{
    return analyzerPeakHoldDecayFromIndex (juce::roundToInt (value));
}