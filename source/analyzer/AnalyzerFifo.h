#pragma once

#include "AnalyzerInputMode.h"

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>

class AnalyzerFifo
{
public:
    AnalyzerFifo();

    void prepare (double sampleRate, int maximumExpectedBlockSize);
    void reset();

    int pushMonoFromBuffer (const juce::AudioBuffer<float>& buffer,
                            int numInputChannels,
                            AnalyzerInputMode inputMode);

    int pop (float* destination, int numSamples);

    int getNumAvailableForReading() const noexcept;
    int getFreeSpace() const noexcept;
    int getCapacity() const noexcept;

private:
    void writeMonoSection (const juce::AudioBuffer<float>& source,
                           int numInputChannels,
                           int sourceStartSample,
                           int fifoStartSample,
                           int numSamples,
                           AnalyzerInputMode inputMode);

    juce::AudioBuffer<float> fifoBuffer;
    juce::AbstractFifo abstractFifo { 1 };

    int capacitySamples = 1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AnalyzerFifo)
};