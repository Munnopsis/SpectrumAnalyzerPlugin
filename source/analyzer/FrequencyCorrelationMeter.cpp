#include "FrequencyCorrelationMeter.h"

#include <algorithm>
#include <cmath>

float FrequencyCorrelationMeter::Biquad::process (float input) noexcept
{
    const auto output =
        b0 * static_cast<double> (input) + z1;

    z1 = b1 * static_cast<double> (input) - a1 * output + z2;
    z2 = b2 * static_cast<double> (input) - a2 * output;

    return static_cast<float> (output);
}

void FrequencyCorrelationMeter::Biquad::reset() noexcept
{
    z1 = 0.0;
    z2 = 0.0;
}

void FrequencyCorrelationMeter::Biquad::setBandPass (
    double sampleRate,
    double centreFrequencyHz,
    double q) noexcept
{
    const auto safeSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    const auto nyquist = safeSampleRate * 0.5;
    const auto safeFrequency =
        juce::jlimit (10.0, nyquist * 0.95, centreFrequencyHz);

    const auto safeQ = juce::jlimit (0.1, 30.0, q);
    const auto omega =
        juce::MathConstants<double>::twoPi * safeFrequency / safeSampleRate;

    const auto sinOmega = std::sin (omega);
    const auto cosOmega = std::cos (omega);
    const auto alpha = sinOmega / (2.0 * safeQ);

    const auto rawB0 = alpha;
    const auto rawB1 = 0.0;
    const auto rawB2 = -alpha;
    const auto rawA0 = 1.0 + alpha;
    const auto rawA1 = -2.0 * cosOmega;
    const auto rawA2 = 1.0 - alpha;

    b0 = rawB0 / rawA0;
    b1 = rawB1 / rawA0;
    b2 = rawB2 / rawA0;
    a1 = rawA1 / rawA0;
    a2 = rawA2 / rawA0;

    reset();
}

void FrequencyCorrelationMeter::prepare (
    double sampleRate,
    int maximumBlockSize)
{
    juce::ignoreUnused (maximumBlockSize);

    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    samplesPerMeasurementBlock =
        juce::jmax (1, juce::roundToInt (currentSampleRate * 0.1));

    updateBands();
    updateFilterCoefficients();
    reset();
}

void FrequencyCorrelationMeter::reset() noexcept
{
    samplesInCurrentBlock = 0;

    for (auto& channelFilters : filters)
        for (auto& filter : channelFilters)
            filter.reset();

    std::fill (blockCross.begin(), blockCross.end(), 0.0);
    std::fill (blockPowerL.begin(), blockPowerL.end(), 0.0);
    std::fill (blockPowerR.begin(), blockPowerR.end(), 0.0);

    for (int band = 0; band < numBands; ++band)
    {
        bandCorrelations[static_cast<size_t> (band)].store (
            0.0f,
            std::memory_order_relaxed);

        bandSmoothedCorrelations[static_cast<size_t> (band)].store (
            0.0f,
            std::memory_order_relaxed);

        bandEnergyDb[static_cast<size_t> (band)].store (
            -100.0f,
            std::memory_order_relaxed);

        bandValid[static_cast<size_t> (band)].store (
            false,
            std::memory_order_relaxed);
    }
}

void FrequencyCorrelationMeter::processBlock (
    const juce::AudioBuffer<float>& buffer) noexcept
{
    processBlock (buffer, buffer.getNumChannels());
}

void FrequencyCorrelationMeter::processBlock (
    const juce::AudioBuffer<float>& buffer,
    int numInputChannels) noexcept
{
    const auto numSamples = buffer.getNumSamples();
    const auto numChannels =
        juce::jmin (numInputChannels, buffer.getNumChannels());

    if (numSamples <= 0 || numChannels <= 0)
        return;

    const auto* left = buffer.getReadPointer (0);
    const auto* right = numChannels > 1 ? buffer.getReadPointer (1) : left;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const auto leftSample = left[sample];
        const auto rightSample = right[sample];

        for (int band = 0; band < numBands; ++band)
        {
            const auto bandIndex = static_cast<size_t> (band);
            const auto leftBand =
                filters[0][bandIndex].process (leftSample);

            const auto rightBand =
                filters[1][bandIndex].process (rightSample);

            blockCross[bandIndex] +=
                static_cast<double> (leftBand) * rightBand;

            blockPowerL[bandIndex] +=
                static_cast<double> (leftBand) * leftBand;

            blockPowerR[bandIndex] +=
                static_cast<double> (rightBand) * rightBand;
        }

        ++samplesInCurrentBlock;

        if (samplesInCurrentBlock >= samplesPerMeasurementBlock)
            finishMeasurementBlock();
    }
}

FrequencyCorrelationMeter::Snapshot
FrequencyCorrelationMeter::getSnapshot() const noexcept
{
    Snapshot snapshot;

    for (int band = 0; band < numBands; ++band)
    {
        auto& destination = snapshot.bands[static_cast<size_t> (band)];
        destination.centreFrequencyHz = bandCentres[static_cast<size_t> (band)];
        destination.minimumFrequencyHz = bandMinimums[static_cast<size_t> (band)];
        destination.maximumFrequencyHz = bandMaximums[static_cast<size_t> (band)];
        destination.correlation =
            bandCorrelations[static_cast<size_t> (band)].load (
                std::memory_order_relaxed);

        destination.smoothedCorrelation =
            bandSmoothedCorrelations[static_cast<size_t> (band)].load (
                std::memory_order_relaxed);

        destination.energyDb =
            bandEnergyDb[static_cast<size_t> (band)].load (
                std::memory_order_relaxed);

        destination.valid =
            bandValid[static_cast<size_t> (band)].load (
                std::memory_order_relaxed);
    }

    return snapshot;
}

void FrequencyCorrelationMeter::updateBands() noexcept
{
    for (int band = 0; band < numBands; ++band)
    {
        const auto normalised =
            static_cast<float> (band)
            / static_cast<float> (numBands - 1);

        bandCentres[static_cast<size_t> (band)] =
            minimumFrequencyHz
            * std::pow (maximumFrequencyHz / minimumFrequencyHz, normalised);
    }

    for (int band = 0; band < numBands; ++band)
    {
        const auto centre = bandCentres[static_cast<size_t> (band)];

        bandMinimums[static_cast<size_t> (band)] =
            band == 0
                ? minimumFrequencyHz
                : std::sqrt (bandCentres[static_cast<size_t> (band - 1)]
                             * centre);

        bandMaximums[static_cast<size_t> (band)] =
            band == numBands - 1
                ? maximumFrequencyHz
                : std::sqrt (centre
                             * bandCentres[static_cast<size_t> (band + 1)]);
    }
}

void FrequencyCorrelationMeter::updateFilterCoefficients() noexcept
{
    const auto nyquist = currentSampleRate * 0.5;

    for (int band = 0; band < numBands; ++band)
    {
        const auto bandIndex = static_cast<size_t> (band);
        const auto minimumHz =
            juce::jlimit (10.0,
                          nyquist * 0.94,
                          static_cast<double> (bandMinimums[bandIndex]));

        const auto maximumHz =
            juce::jlimit (minimumHz + 1.0,
                          nyquist * 0.96,
                          static_cast<double> (bandMaximums[bandIndex]));

        const auto centreHz =
            juce::jlimit (minimumHz,
                          maximumHz,
                          static_cast<double> (bandCentres[bandIndex]));

        const auto bandwidthHz = juce::jmax (1.0, maximumHz - minimumHz);
        const auto q = centreHz / bandwidthHz;

        for (auto& channelFilters : filters)
            channelFilters[bandIndex].setBandPass (
                currentSampleRate,
                centreHz,
                q);
    }
}

void FrequencyCorrelationMeter::finishMeasurementBlock() noexcept
{
    if (samplesInCurrentBlock <= 0)
        return;

    const auto samplesAsDouble =
        static_cast<double> (samplesInCurrentBlock);

    for (int band = 0; band < numBands; ++band)
    {
        const auto bandIndex = static_cast<size_t> (band);
        const auto meanPowerL = blockPowerL[bandIndex] / samplesAsDouble;
        const auto meanPowerR = blockPowerR[bandIndex] / samplesAsDouble;
        const auto meanCross = blockCross[bandIndex] / samplesAsDouble;

        const auto previous =
            bandSmoothedCorrelations[bandIndex].load (
                std::memory_order_relaxed);

        const auto valid =
            meanPowerL > lowEnergyMeanSquare
            && meanPowerR > lowEnergyMeanSquare;

        auto correlation = 0.0f;

        if (valid)
        {
            correlation = juce::jlimit (
                -1.0f,
                1.0f,
                static_cast<float> (
                    meanCross / std::sqrt (meanPowerL * meanPowerR)));
        }

        const auto alpha = valid ? smoothingAlpha : invalidReleaseAlpha;
        const auto target = valid ? correlation : 0.0f;
        const auto smoothed =
            previous + alpha * (target - previous);

        bandCorrelations[bandIndex].store (
            correlation,
            std::memory_order_relaxed);

        bandSmoothedCorrelations[bandIndex].store (
            smoothed,
            std::memory_order_relaxed);

        bandEnergyDb[bandIndex].store (
            decibelsFromBandEnergy (0.5 * (meanPowerL + meanPowerR)),
            std::memory_order_relaxed);

        bandValid[bandIndex].store (valid, std::memory_order_relaxed);
    }

    std::fill (blockCross.begin(), blockCross.end(), 0.0);
    std::fill (blockPowerL.begin(), blockPowerL.end(), 0.0);
    std::fill (blockPowerR.begin(), blockPowerR.end(), 0.0);
    samplesInCurrentBlock = 0;
}

float FrequencyCorrelationMeter::decibelsFromBandEnergy (
    double meanPower) noexcept
{
    if (!std::isfinite (meanPower) || meanPower <= 0.0)
        return -100.0f;

    return juce::Decibels::gainToDecibels (
        static_cast<float> (std::sqrt (meanPower)),
        -100.0f);
}
