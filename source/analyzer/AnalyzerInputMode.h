#pragma once

#include <juce_core/juce_core.h>

enum class AnalyzerInputMode
{
    stereoSum = 0,
    left,
    right,
    mid,
    side,
    leftRightDual,
    midSideDual,
    count
};

inline juce::StringArray getAnalyzerInputModeChoices()
{
    return { "Stereo Sum", "Left", "Right", "Mid", "Side", "L/R Dual", "M/S Dual" };
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

inline bool analyzerInputModeIsDual (AnalyzerInputMode mode) noexcept
{
    return mode == AnalyzerInputMode::leftRightDual
           || mode == AnalyzerInputMode::midSideDual;
}

inline AnalyzerInputMode analyzerInputModeGetPrimaryMode (
    AnalyzerInputMode mode) noexcept
{
    switch (mode)
    {
        case AnalyzerInputMode::stereoSum:
        case AnalyzerInputMode::left:
        case AnalyzerInputMode::right:
        case AnalyzerInputMode::mid:
        case AnalyzerInputMode::side:
            return mode;

        case AnalyzerInputMode::leftRightDual:
            return AnalyzerInputMode::left;

        case AnalyzerInputMode::midSideDual:
            return AnalyzerInputMode::mid;

        case AnalyzerInputMode::count:
            break;
    }

    return AnalyzerInputMode::stereoSum;
}

inline AnalyzerInputMode analyzerInputModeGetSecondaryMode (
    AnalyzerInputMode mode) noexcept
{
    switch (mode)
    {
        case AnalyzerInputMode::leftRightDual:
            return AnalyzerInputMode::right;

        case AnalyzerInputMode::midSideDual:
            return AnalyzerInputMode::side;

        case AnalyzerInputMode::stereoSum:
        case AnalyzerInputMode::left:
        case AnalyzerInputMode::right:
        case AnalyzerInputMode::mid:
        case AnalyzerInputMode::side:
        case AnalyzerInputMode::count:
            break;
    }

    return AnalyzerInputMode::stereoSum;
}

inline float makeAnalyzerMonoSample (float left, float right, AnalyzerInputMode mode) noexcept
{
    switch (mode)
    {
        case AnalyzerInputMode::stereoSum:
            return (left + right) * 0.5f;

        case AnalyzerInputMode::left:
            return left;

        case AnalyzerInputMode::right:
            return right;

        case AnalyzerInputMode::mid:
            return (left + right) * 0.5f;

        case AnalyzerInputMode::side:
            return (left - right) * 0.5f;

        case AnalyzerInputMode::leftRightDual:
            return left;

        case AnalyzerInputMode::midSideDual:
            return (left + right) * 0.5f;

        case AnalyzerInputMode::count:
            break;
    }

    return (left + right) * 0.5f;
}
