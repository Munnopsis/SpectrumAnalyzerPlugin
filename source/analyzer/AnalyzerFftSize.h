#pragma once

#include <juce_core/juce_core.h>

enum class AnalyzerFftSize
{
    size1024 = 0,
    size2048,
    size4096,
    size8192,
    count
};

inline juce::StringArray getAnalyzerFftSizeChoices()
{
    return { "1024", "2048", "4096", "8192" };
}

inline int analyzerFftOrderFromIndex (int index) noexcept
{
    switch (juce::jlimit (0, static_cast<int> (AnalyzerFftSize::count) - 1, index))
    {
        case static_cast<int> (AnalyzerFftSize::size1024): return 10;
        case static_cast<int> (AnalyzerFftSize::size2048): return 11;
        case static_cast<int> (AnalyzerFftSize::size4096): return 12;
        case static_cast<int> (AnalyzerFftSize::size8192): return 13;
        default: return 11;
    }
}

inline int analyzerFftOrderFromParameterValue (float value) noexcept
{
    return analyzerFftOrderFromIndex (juce::roundToInt (value));
}

inline int analyzerFftSizeFromOrder (int fftOrder) noexcept
{
    return 1 << fftOrder;
}