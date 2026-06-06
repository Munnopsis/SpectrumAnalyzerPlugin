#pragma once

#include <juce_core/juce_core.h>

namespace AnalyzerFrequencyRange
{
    inline constexpr float minimumHz = 20.0f;
    inline constexpr float maximumHz = 20000.0f;

    inline float getMaximumHzForSampleRate (float sampleRate) noexcept
    {
        if (sampleRate <= 0.0f)
            return maximumHz;

        return juce::jmax (minimumHz + 1.0f,
                           juce::jmin (maximumHz, sampleRate * 0.5f));
    }
}
