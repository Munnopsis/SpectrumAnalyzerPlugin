#include "AnalyzerFifo.h"

AnalyzerFifo::AnalyzerFifo()
{
    prepare (44100.0, 512);
}

void AnalyzerFifo::prepare (double sampleRate, int maximumExpectedBlockSize)
{
    juce::ignoreUnused (maximumExpectedBlockSize);

    // Erstmal 2 Sekunden Analysepuffer.
    // Das ist mehr als genug für FFT-Blöcke und verhindert zu frühes Überlaufen.
    capacitySamples = juce::jmax (8192, static_cast<int> (sampleRate * 2.0));

    fifoBuffer.setSize (1, capacitySamples, false, false, true);
    fifoBuffer.clear();

    abstractFifo.setTotalSize (capacitySamples);
    abstractFifo.reset();
}

void AnalyzerFifo::reset()
{
    fifoBuffer.clear();
    abstractFifo.reset();
}

int AnalyzerFifo::pushMonoFromBuffer (const juce::AudioBuffer<float>& buffer,
                                      int numInputChannels,
                                      AnalyzerInputMode inputMode)
{
    const auto numSamples = buffer.getNumSamples();

    if (numSamples <= 0 || numInputChannels <= 0)
        return 0;

    const auto channelsToUse = juce::jmin (numInputChannels, buffer.getNumChannels());

    if (channelsToUse <= 0)
        return 0;

    int start1 = 0;
    int size1 = 0;
    int start2 = 0;
    int size2 = 0;

    abstractFifo.prepareToWrite (numSamples, start1, size1, start2, size2);

    if (size1 > 0)
        writeMonoSection (buffer, channelsToUse, 0, start1, size1, inputMode);

    if (size2 > 0)
        writeMonoSection (buffer, channelsToUse, size1, start2, size2, inputMode);

    const auto written = size1 + size2;
    abstractFifo.finishedWrite (written);

    return written;
}

int AnalyzerFifo::pop (float* destination, int numSamples)
{
    if (destination == nullptr || numSamples <= 0)
        return 0;

    int start1 = 0;
    int size1 = 0;
    int start2 = 0;
    int size2 = 0;

    abstractFifo.prepareToRead (numSamples, start1, size1, start2, size2);

    if (size1 > 0)
        juce::FloatVectorOperations::copy (destination,
                                           fifoBuffer.getReadPointer (0, start1),
                                           size1);

    if (size2 > 0)
        juce::FloatVectorOperations::copy (destination + size1,
                                           fifoBuffer.getReadPointer (0, start2),
                                           size2);

    const auto read = size1 + size2;
    abstractFifo.finishedRead (read);

    return read;
}

int AnalyzerFifo::getNumAvailableForReading() const noexcept
{
    return abstractFifo.getNumReady();
}

int AnalyzerFifo::getFreeSpace() const noexcept
{
    return abstractFifo.getFreeSpace();
}

int AnalyzerFifo::getCapacity() const noexcept
{
    return capacitySamples;
}

void AnalyzerFifo::writeMonoSection (const juce::AudioBuffer<float>& source,
                                     int numInputChannels,
                                     int sourceStartSample,
                                     int fifoStartSample,
                                     int numSamples,
                                     AnalyzerInputMode inputMode)
{
    auto* destination = fifoBuffer.getWritePointer (0, fifoStartSample);

    const auto* left = source.getReadPointer (0, sourceStartSample);
    const auto* right = numInputChannels > 1
                            ? source.getReadPointer (1, sourceStartSample)
                            : left;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        destination[sample] = makeAnalyzerMonoSample (
            left[sample],
            right[sample],
            inputMode);
    }
}