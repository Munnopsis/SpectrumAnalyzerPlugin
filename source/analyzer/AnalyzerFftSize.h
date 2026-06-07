#pragma once

#include <juce_core/juce_core.h>

enum class AnalyzerFftSize
{
    size1024 = 0,
    size2048,
    size4096,
    size8192,
    size16384,
    size32768,
    frequencyDependent,
    frequencyDependentTuned,
    count
};

inline juce::StringArray getAnalyzerFftSizeChoices()
{
    return {"1024", "2048", "4096", "8192", "16384", "32768", "Frequency Dependent", "Frequency Dependent Tuned"};
}

inline bool analyzerFftSizeIsFrequencyDependent(float value) noexcept
{
    const auto index = juce::roundToInt(value);
    return index == static_cast<int>(AnalyzerFftSize::frequencyDependent) || index == static_cast<int>(AnalyzerFftSize::frequencyDependentTuned);
}

inline bool analyzerFftSizeIsFrequencyDependentTuned(float value) noexcept
{
    return juce::roundToInt(value) == static_cast<int>(AnalyzerFftSize::frequencyDependentTuned);
}

inline int analyzerFftOrderFromIndex(int index) noexcept
{
    switch (juce::jlimit(0, static_cast<int>(AnalyzerFftSize::count) - 1, index))
    {
        case static_cast<int>(AnalyzerFftSize::size1024):
            return 10;
        case static_cast<int>(AnalyzerFftSize::size2048):
            return 11;
        case static_cast<int>(AnalyzerFftSize::size4096):
            return 12;
        case static_cast<int>(AnalyzerFftSize::size8192):
            return 13;
        case static_cast<int>(AnalyzerFftSize::size16384):
            return 14;
        case static_cast<int>(AnalyzerFftSize::size32768):
            return 15;
        case static_cast<int>(AnalyzerFftSize::frequencyDependent):
            return 13;
        case static_cast<int>(AnalyzerFftSize::frequencyDependentTuned):
            return 13;
        default:
            return 11;
    }
}

inline int analyzerFftOrderFromParameterValue(float value) noexcept
{
    return analyzerFftOrderFromIndex(juce::roundToInt(value));
}

inline int analyzerFftSizeFromOrder(int fftOrder) noexcept
{
    return 1 << fftOrder;
}
