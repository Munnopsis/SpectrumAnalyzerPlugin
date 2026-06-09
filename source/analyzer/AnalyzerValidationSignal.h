#pragma once

#include <juce_core/juce_core.h>

enum class AnalyzerValidationSignal
{
    off = 0,
    monoSine,
    stereoSineInPhase,
    stereoSineOutOfPhase,
    leftOnlySine,
    rightOnlySine,
    midOnlySine,
    sideOnlySine,
    stereoMusicLike,
    count
};

inline juce::StringArray getAnalyzerValidationSignalChoices()
{
    return {
        "Off",
        "Mono",
        "Stereo +",
        "Stereo -",
        "L only",
        "R only",
        "Mid",
        "Side",
        "Music-like"
    };
}

inline AnalyzerValidationSignal analyzerValidationSignalFromIndex (int index) noexcept
{
    constexpr auto maxIndex = static_cast<int> (AnalyzerValidationSignal::count) - 1;
    return static_cast<AnalyzerValidationSignal> (juce::jlimit (0, maxIndex, index));
}

inline AnalyzerValidationSignal analyzerValidationSignalFromParameterValue (
    float value) noexcept
{
    return analyzerValidationSignalFromIndex (juce::roundToInt (value));
}

inline juce::String getAnalyzerValidationSignalLabel (
    AnalyzerValidationSignal signal)
{
    switch (signal)
    {
        case AnalyzerValidationSignal::off:                  return {};
        case AnalyzerValidationSignal::monoSine:             return "Mono Sine";
        case AnalyzerValidationSignal::stereoSineInPhase:    return "Stereo In Phase";
        case AnalyzerValidationSignal::stereoSineOutOfPhase: return "Stereo Out Of Phase";
        case AnalyzerValidationSignal::leftOnlySine:         return "Left Only Sine";
        case AnalyzerValidationSignal::rightOnlySine:        return "Right Only Sine";
        case AnalyzerValidationSignal::midOnlySine:          return "Mid Only Sine";
        case AnalyzerValidationSignal::sideOnlySine:         return "Side Only Sine";
        case AnalyzerValidationSignal::stereoMusicLike:      return "Music-like Stereo";
        case AnalyzerValidationSignal::count:                break;
    }

    return {};
}

inline juce::String getAnalyzerValidationExpectedLabel (
    AnalyzerValidationSignal signal)
{
    switch (signal)
    {
        case AnalyzerValidationSignal::monoSine:
        case AnalyzerValidationSignal::stereoSineInPhase:
        case AnalyzerValidationSignal::midOnlySine:
            return "Expected: Corr +1, Width 0%, M active, S silent";

        case AnalyzerValidationSignal::stereoSineOutOfPhase:
        case AnalyzerValidationSignal::sideOnlySine:
            return "Expected: Corr -1, Width high, M silent, S active";

        case AnalyzerValidationSignal::leftOnlySine:
            return "Expected: Balance left, Corr invalid/near 0, L active, R silent";

        case AnalyzerValidationSignal::rightOnlySine:
            return "Expected: Balance right, Corr invalid/near 0, R active, L silent";

        case AnalyzerValidationSignal::stereoMusicLike:
            return "Expected: Corr between -1 and +1, frequency correlation varies by band";

        case AnalyzerValidationSignal::off:
        case AnalyzerValidationSignal::count:
            break;
    }

    return {};
}
