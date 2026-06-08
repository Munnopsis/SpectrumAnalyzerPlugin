#pragma once

#include <juce_core/juce_core.h>

enum class AnalyzerCurveSource
{
    live = 0,
    rms,
    energy,
    peakHold,
    count
};

inline juce::StringArray getAnalyzerCurveSourceChoices()
{
    return { "Live", "RMS", "Energy", "Peak" };
}

inline AnalyzerCurveSource analyzerCurveSourceFromIndex (int index) noexcept
{
    constexpr auto maxIndex = static_cast<int> (AnalyzerCurveSource::count) - 1;

    return static_cast<AnalyzerCurveSource> (
        juce::jlimit (0, maxIndex, index));
}

inline AnalyzerCurveSource analyzerCurveSourceFromParameterValue (float value) noexcept
{
    return analyzerCurveSourceFromIndex (juce::roundToInt (value));
}

inline int analyzerCurveSourceToIndex (AnalyzerCurveSource source) noexcept
{
    return juce::jlimit (
        0,
        static_cast<int> (AnalyzerCurveSource::count) - 1,
        static_cast<int> (source));
}
