#pragma once

#include <juce_core/juce_core.h>

enum class AnalyzerInputMode
{
    stereoSum = 0,
    left,
    right,
    mid,
    side,
    count
};

inline juce::StringArray getAnalyzerInputModeChoices()
{
    return { "Stereo Sum", "Left", "Right", "Mid", "Side" };
}

inline AnalyzerInputMode analyzerInputModeFromIndex (int index) noexcept
{
    constexpr auto maxIndex = static_cast<int> (AnalyzerInputMode::count) - 1;
    return static_cast<AnalyzerInputMode> (juce::jlimit (0, maxIndex, index));
}

inline AnalyzerInputMode analyzerInputModeFromParameterValue (float value) noexcept
{
    return analyzerInputModeFromIndex (juce::roundToInt (value));
}

inline float makeAnalyzerMonoSample (float left, float right, AnalyzerInputMode mode) noexcept
{
    switch (mode)
    {
        case AnalyzerInputMode::left:
            return left;

        case AnalyzerInputMode::right:
            return right;

        case AnalyzerInputMode::mid:
            return (left + right) * 0.5f;

        case AnalyzerInputMode::side:
            return (left - right) * 0.5f;

        case AnalyzerInputMode::stereoSum:
        default:
            return (left + right) * 0.5f;
    }
}