#pragma once

#include <juce_core/juce_core.h>

enum class AnalyzerVqtLiveCurveProfile
{
    smooth = 0,
    balanced,
    detailed,
    count
};

inline juce::StringArray getAnalyzerVqtLiveCurveProfileChoices()
{
    return { "Live: Smooth", "Live: Balanced", "Live: Detailed" };
}

inline AnalyzerVqtLiveCurveProfile analyzerVqtLiveCurveProfileFromIndex (int index) noexcept
{
    constexpr auto maxIndex =
        static_cast<int> (AnalyzerVqtLiveCurveProfile::count) - 1;

    return static_cast<AnalyzerVqtLiveCurveProfile> (
        juce::jlimit (0, maxIndex, index));
}

inline AnalyzerVqtLiveCurveProfile analyzerVqtLiveCurveProfileFromParameterValue (
    float value) noexcept
{
    return analyzerVqtLiveCurveProfileFromIndex (juce::roundToInt (value));
}

inline int analyzerVqtLiveCurveProfileToIndex (
    AnalyzerVqtLiveCurveProfile profile) noexcept
{
    return juce::jlimit (
        0,
        static_cast<int> (AnalyzerVqtLiveCurveProfile::count) - 1,
        static_cast<int> (profile));
}
