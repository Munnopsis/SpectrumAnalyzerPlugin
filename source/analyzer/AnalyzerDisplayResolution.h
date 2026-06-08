#pragma once

#include <juce_core/juce_core.h>

enum class AnalyzerDisplayResolution
{
    highResolution = 0,
    detailed,
    balanced,
    smooth,
    oneSixthOctave,
    oneThirdOctave,
    octave,
    count
};

inline juce::StringArray getAnalyzerDisplayResolutionChoices()
{
    return {
        "Res: High",
        "Res: Detailed",
        "Res: Balanced",
        "Res: Smooth",
        "Res: 1/6 Oct",
        "Res: 1/3 Oct",
        "Res: Octave"
    };
}

inline AnalyzerDisplayResolution analyzerDisplayResolutionFromIndex (int index) noexcept
{
    constexpr auto maxIndex =
        static_cast<int> (AnalyzerDisplayResolution::count) - 1;

    return static_cast<AnalyzerDisplayResolution> (
        juce::jlimit (0, maxIndex, index));
}

inline AnalyzerDisplayResolution analyzerDisplayResolutionFromParameterValue (
    float value) noexcept
{
    return analyzerDisplayResolutionFromIndex (juce::roundToInt (value));
}

inline int analyzerDisplayResolutionToIndex (
    AnalyzerDisplayResolution resolution) noexcept
{
    return juce::jlimit (
        0,
        static_cast<int> (AnalyzerDisplayResolution::count) - 1,
        static_cast<int> (resolution));
}
