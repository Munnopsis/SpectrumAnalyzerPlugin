#include "LoudnessMeter.h"

#include <algorithm>
#include <cmath>

namespace
{
    constexpr double kWeightingShelfFrequencyHz = 1681.974450955533;
    constexpr double kWeightingShelfGainDb = 3.999843853973347;
    constexpr double kWeightingShelfQ = 0.7071752369554196;
    constexpr double kWeightingHighPassFrequencyHz = 38.13547087613982;
    constexpr double kWeightingHighPassQ = 0.5003270373238773;
    constexpr double absoluteGateLufs = -70.0;
    constexpr double localSilenceMeanSquare = 1.0e-12;

    double safeLog10 (double value) noexcept
    {
        return std::log10 (juce::jmax (value, localSilenceMeanSquare));
    }
}

float LoudnessMeter::Biquad::process (float input) noexcept
{
    const auto output =
        b0 * static_cast<double> (input) + z1;

    z1 = b1 * static_cast<double> (input) - a1 * output + z2;
    z2 = b2 * static_cast<double> (input) - a2 * output;

    return static_cast<float> (output);
}

void LoudnessMeter::Biquad::reset() noexcept
{
    z1 = 0.0;
    z2 = 0.0;
}

void LoudnessMeter::Biquad::setCoefficients (double newB0,
                                             double newB1,
                                             double newB2,
                                             double newA1,
                                             double newA2) noexcept
{
    b0 = newB0;
    b1 = newB1;
    b2 = newB2;
    a1 = newA1;
    a2 = newA2;
}

void LoudnessMeter::prepare (double sampleRate, int maximumBlockSize)
{
    juce::ignoreUnused (maximumBlockSize);

    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    samplesPerMeasurementBlock =
        juce::jmax (1, juce::roundToInt (currentSampleRate * 0.1));

    updateKWeightingCoefficients();
    reset();
}

void LoudnessMeter::reset() noexcept
{
    for (auto& filter : highShelfFilters)
        filter.reset();

    for (auto& filter : highPassFilters)
        filter.reset();

    std::fill (recentMeanSquares.begin(), recentMeanSquares.end(), 0.0);
    std::fill (integratedMeanSquares.begin(), integratedMeanSquares.end(), 0.0);
    std::fill (lraShortTermMeanSquares.begin(), lraShortTermMeanSquares.end(), 0.0);
    std::fill (lraLoudnessScratch.begin(), lraLoudnessScratch.end(), 0.0);

    recentWriteIndex = 0;
    recentBlockCount = 0;
    integratedWriteIndex = 0;
    integratedBlockCount = 0;
    lraWriteIndex = 0;
    lraBlockCount = 0;
    lraBlocksSinceLastUpdate = 0;

    samplesInCurrentBlock = 0;
    rawRmsSamplesInCurrentBlock = 0;
    currentKWeightedEnergySum = 0.0;
    currentRawRmsSum = 0.0;
    currentBlockSamplePeak = 0.0f;
    peakHoldLinear = 0.0f;

    momentaryLufs.store (-100.0f, std::memory_order_relaxed);
    shortTermLufs.store (-100.0f, std::memory_order_relaxed);
    integratedLufs.store (-100.0f, std::memory_order_relaxed);
    loudnessRangeLu.store (0.0f, std::memory_order_relaxed);
    samplePeakDb.store (-100.0f, std::memory_order_relaxed);
    truePeakDb.store (-100.0f, std::memory_order_relaxed);
    rmsDb.store (-100.0f, std::memory_order_relaxed);
    crestDb.store (0.0f, std::memory_order_relaxed);
    peakHoldDb.store (-100.0f, std::memory_order_relaxed);
    hasIntegratedMeasurement.store (false, std::memory_order_relaxed);
    hasLoudnessRange.store (false, std::memory_order_relaxed);
    hasTruePeak.store (false, std::memory_order_relaxed);
}

void LoudnessMeter::processBlock (
    const juce::AudioBuffer<float>& buffer) noexcept
{
    processBlock (buffer, buffer.getNumChannels());
}

void LoudnessMeter::processBlock (
    const juce::AudioBuffer<float>& buffer,
    int numInputChannels) noexcept
{
    const auto numSamples = buffer.getNumSamples();
    const auto numChannels =
        juce::jmin (maxChannels,
                    juce::jmin (numInputChannels, buffer.getNumChannels()));

    if (numSamples <= 0 || numChannels <= 0)
        return;

    const auto* left = buffer.getReadPointer (0);
    const auto* right = numChannels > 1 ? buffer.getReadPointer (1) : nullptr;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const auto leftSample = left[sample];
        const auto rightSample = right != nullptr ? right[sample] : 0.0f;

        const std::array<float, maxChannels> inputSamples {{
            leftSample,
            rightSample
        }};

        auto kWeightedEnergyForSample = 0.0;
        auto rawEnergyForSample = 0.0;
        auto samplePeak = 0.0f;

        for (int channel = 0; channel < numChannels; ++channel)
        {
            const auto input = inputSamples[static_cast<size_t> (channel)];
            const auto shelfed =
                highShelfFilters[static_cast<size_t> (channel)].process (input);

            const auto weighted =
                highPassFilters[static_cast<size_t> (channel)].process (shelfed);

            kWeightedEnergyForSample +=
                static_cast<double> (weighted) * weighted;

            rawEnergyForSample +=
                static_cast<double> (input) * input;

            samplePeak = juce::jmax (samplePeak, std::abs (input));
        }

        currentKWeightedEnergySum += kWeightedEnergyForSample;
        currentRawRmsSum += rawEnergyForSample;
        rawRmsSamplesInCurrentBlock += numChannels;
        currentBlockSamplePeak =
            juce::jmax (currentBlockSamplePeak, samplePeak);
        peakHoldLinear = juce::jmax (peakHoldLinear, samplePeak);

        ++samplesInCurrentBlock;

        if (samplesInCurrentBlock >= samplesPerMeasurementBlock)
            finishMeasurementBlock();
    }
}

LoudnessMeter::Snapshot LoudnessMeter::getSnapshot() const noexcept
{
    Snapshot snapshot;
    snapshot.momentaryLufs = momentaryLufs.load (std::memory_order_relaxed);
    snapshot.shortTermLufs = shortTermLufs.load (std::memory_order_relaxed);
    snapshot.integratedLufs = integratedLufs.load (std::memory_order_relaxed);
    snapshot.loudnessRangeLu = loudnessRangeLu.load (std::memory_order_relaxed);
    snapshot.samplePeakDb = samplePeakDb.load (std::memory_order_relaxed);
    snapshot.truePeakDb = truePeakDb.load (std::memory_order_relaxed);
    snapshot.rmsDb = rmsDb.load (std::memory_order_relaxed);
    snapshot.crestDb = crestDb.load (std::memory_order_relaxed);
    snapshot.peakHoldDb = peakHoldDb.load (std::memory_order_relaxed);
    snapshot.hasIntegratedMeasurement =
        hasIntegratedMeasurement.load (std::memory_order_relaxed);
    snapshot.hasLoudnessRange =
        hasLoudnessRange.load (std::memory_order_relaxed);
    snapshot.hasTruePeak = hasTruePeak.load (std::memory_order_relaxed);

    return snapshot;
}

double LoudnessMeter::lufsFromMeanSquare (double meanSquare) noexcept
{
    if (!std::isfinite (meanSquare) || meanSquare <= silenceMeanSquare)
        return -100.0;

    return juce::jmax (-100.0, -0.691 + 10.0 * safeLog10 (meanSquare));
}

float LoudnessMeter::decibelsFromGain (float gain) noexcept
{
    return juce::Decibels::gainToDecibels (
        std::isfinite (gain) ? gain : 0.0f,
        -100.0f);
}

void LoudnessMeter::updateKWeightingCoefficients() noexcept
{
    const auto makeHighPass =
        [] (double sampleRate, double frequencyHz, double q)
        {
            const auto omega =
                juce::MathConstants<double>::twoPi * frequencyHz / sampleRate;

            const auto sinOmega = std::sin (omega);
            const auto cosOmega = std::cos (omega);
            const auto alpha = sinOmega / (2.0 * q);

            const auto b0 = (1.0 + cosOmega) * 0.5;
            const auto b1 = -(1.0 + cosOmega);
            const auto b2 = (1.0 + cosOmega) * 0.5;
            const auto a0 = 1.0 + alpha;
            const auto a1 = -2.0 * cosOmega;
            const auto a2 = 1.0 - alpha;

            std::array<double, 5> result {{
                b0 / a0,
                b1 / a0,
                b2 / a0,
                a1 / a0,
                a2 / a0
            }};

            return result;
        };

    const auto makeHighShelf =
        [] (double sampleRate, double frequencyHz, double gainDb, double q)
        {
            const auto a = std::pow (10.0, gainDb / 40.0);
            const auto omega =
                juce::MathConstants<double>::twoPi * frequencyHz / sampleRate;

            const auto sinOmega = std::sin (omega);
            const auto cosOmega = std::cos (omega);
            const auto alpha = sinOmega / (2.0 * q);
            const auto sqrtA = std::sqrt (a);

            const auto b0 =
                a * ((a + 1.0)
                     + (a - 1.0) * cosOmega
                     + 2.0 * sqrtA * alpha);

            const auto b1 =
                -2.0 * a * ((a - 1.0) + (a + 1.0) * cosOmega);

            const auto b2 =
                a * ((a + 1.0)
                     + (a - 1.0) * cosOmega
                     - 2.0 * sqrtA * alpha);

            const auto a0 =
                (a + 1.0)
                - (a - 1.0) * cosOmega
                + 2.0 * sqrtA * alpha;

            const auto a1 =
                2.0 * ((a - 1.0) - (a + 1.0) * cosOmega);

            const auto a2 =
                (a + 1.0)
                - (a - 1.0) * cosOmega
                - 2.0 * sqrtA * alpha;

            std::array<double, 5> result {{
                b0 / a0,
                b1 / a0,
                b2 / a0,
                a1 / a0,
                a2 / a0
            }};

            return result;
        };

    const auto shelf = makeHighShelf (currentSampleRate,
                                      kWeightingShelfFrequencyHz,
                                      kWeightingShelfGainDb,
                                      kWeightingShelfQ);

    const auto highPass = makeHighPass (currentSampleRate,
                                        kWeightingHighPassFrequencyHz,
                                        kWeightingHighPassQ);

    for (auto& filter : highShelfFilters)
    {
        filter.setCoefficients (shelf[0], shelf[1], shelf[2], shelf[3], shelf[4]);
        filter.reset();
    }

    for (auto& filter : highPassFilters)
    {
        filter.setCoefficients (highPass[0],
                                highPass[1],
                                highPass[2],
                                highPass[3],
                                highPass[4]);
        filter.reset();
    }
}

void LoudnessMeter::finishMeasurementBlock() noexcept
{
    if (samplesInCurrentBlock <= 0)
        return;

    const auto meanSquare =
        currentKWeightedEnergySum
        / static_cast<double> (samplesInCurrentBlock);

    recentMeanSquares[static_cast<size_t> (recentWriteIndex)] = meanSquare;
    recentWriteIndex = (recentWriteIndex + 1) % shortTermBlockCount;
    recentBlockCount = juce::jmin (shortTermBlockCount, recentBlockCount + 1);

    const auto momentaryMeanSquare =
        recentBlockCount >= momentaryBlockCount
            ? getRecentMeanSquare (momentaryBlockCount)
            : 0.0;

    const auto shortTermMeanSquare =
        recentBlockCount >= shortTermBlockCount
            ? getRecentMeanSquare (shortTermBlockCount)
            : getRecentMeanSquare (recentBlockCount);

    storeSnapshotValue (momentaryLufs, lufsFromMeanSquare (momentaryMeanSquare));
    storeSnapshotValue (shortTermLufs, lufsFromMeanSquare (shortTermMeanSquare));

    if (recentBlockCount >= momentaryBlockCount)
        pushIntegratedBlock (momentaryMeanSquare);

    if (recentBlockCount >= shortTermBlockCount)
        pushLoudnessRangeBlock (shortTermMeanSquare);
    else
    {
        loudnessRangeLu.store (0.0f, std::memory_order_relaxed);
        hasLoudnessRange.store (false, std::memory_order_relaxed);
    }

    const auto rmsGain =
        rawRmsSamplesInCurrentBlock > 0
            ? std::sqrt (currentRawRmsSum
                         / static_cast<double> (rawRmsSamplesInCurrentBlock))
            : 0.0;

    const auto samplePeak = currentBlockSamplePeak;
    const auto samplePeakDecibels = decibelsFromGain (samplePeak);
    const auto rmsDecibels = decibelsFromGain (static_cast<float> (rmsGain));
    const auto heldPeakDb = decibelsFromGain (peakHoldLinear);

    samplePeakDb.store (samplePeakDecibels, std::memory_order_relaxed);

    // True Peak wird im nächsten Patch durch validiertes 4x Oversampling ersetzt.
    // Bis dahin bleibt truePeakDb bewusst Sample-Peak-Fallback und hasTruePeak false.
    truePeakDb.store (samplePeakDecibels, std::memory_order_relaxed);
    hasTruePeak.store (false, std::memory_order_relaxed);

    rmsDb.store (rmsDecibels, std::memory_order_relaxed);
    peakHoldDb.store (heldPeakDb, std::memory_order_relaxed);
    crestDb.store (juce::jmax (0.0f, heldPeakDb - rmsDecibels),
                   std::memory_order_relaxed);

    samplesInCurrentBlock = 0;
    rawRmsSamplesInCurrentBlock = 0;
    currentKWeightedEnergySum = 0.0;
    currentRawRmsSum = 0.0;
    currentBlockSamplePeak = 0.0f;
}

double LoudnessMeter::getRecentMeanSquare (int numBlocks) const noexcept
{
    const auto blocksToUse =
        juce::jlimit (0, recentBlockCount, numBlocks);

    if (blocksToUse <= 0)
        return 0.0;

    auto sum = 0.0;

    for (int i = 0; i < blocksToUse; ++i)
    {
        auto index = recentWriteIndex - 1 - i;

        while (index < 0)
            index += shortTermBlockCount;

        sum += recentMeanSquares[static_cast<size_t> (index)];
    }

    return sum / static_cast<double> (blocksToUse);
}

void LoudnessMeter::pushIntegratedBlock (double meanSquare) noexcept
{
    const auto blockLufs = lufsFromMeanSquare (meanSquare);

    if (blockLufs < absoluteGateLufs)
    {
        updateIntegratedLoudness();
        return;
    }

    integratedMeanSquares[static_cast<size_t> (integratedWriteIndex)] =
        meanSquare;

    integratedWriteIndex =
        (integratedWriteIndex + 1) % maxIntegratedBlocks;

    integratedBlockCount =
        juce::jmin (maxIntegratedBlocks, integratedBlockCount + 1);

    updateIntegratedLoudness();
}

void LoudnessMeter::updateIntegratedLoudness() noexcept
{
    if (integratedBlockCount <= 0)
    {
        integratedLufs.store (-100.0f, std::memory_order_relaxed);
        hasIntegratedMeasurement.store (false, std::memory_order_relaxed);
        return;
    }

    auto preliminaryEnergySum = 0.0;
    auto preliminaryCount = 0;

    for (int i = 0; i < integratedBlockCount; ++i)
    {
        const auto energy = integratedMeanSquares[static_cast<size_t> (i)];

        if (lufsFromMeanSquare (energy) >= absoluteGateLufs)
        {
            preliminaryEnergySum += energy;
            ++preliminaryCount;
        }
    }

    if (preliminaryCount <= 0)
    {
        integratedLufs.store (-100.0f, std::memory_order_relaxed);
        hasIntegratedMeasurement.store (false, std::memory_order_relaxed);
        return;
    }

    const auto preliminaryMeanSquare =
        preliminaryEnergySum / static_cast<double> (preliminaryCount);

    const auto relativeGate =
        lufsFromMeanSquare (preliminaryMeanSquare) - 10.0;

    auto gatedEnergySum = 0.0;
    auto gatedCount = 0;

    for (int i = 0; i < integratedBlockCount; ++i)
    {
        const auto energy = integratedMeanSquares[static_cast<size_t> (i)];
        const auto blockLufs = lufsFromMeanSquare (energy);

        if (blockLufs >= absoluteGateLufs && blockLufs >= relativeGate)
        {
            gatedEnergySum += energy;
            ++gatedCount;
        }
    }

    if (gatedCount <= 0)
    {
        integratedLufs.store (-100.0f, std::memory_order_relaxed);
        hasIntegratedMeasurement.store (false, std::memory_order_relaxed);
        return;
    }

    const auto integratedMeanSquare =
        gatedEnergySum / static_cast<double> (gatedCount);

    storeSnapshotValue (integratedLufs,
                        lufsFromMeanSquare (integratedMeanSquare));

    hasIntegratedMeasurement.store (true, std::memory_order_relaxed);
}

void LoudnessMeter::pushLoudnessRangeBlock (
    double shortTermMeanSquare) noexcept
{
    const auto shortTermBlockLufs =
        lufsFromMeanSquare (shortTermMeanSquare);

    if (!std::isfinite (shortTermMeanSquare)
        || shortTermMeanSquare <= silenceMeanSquare
        || shortTermBlockLufs < absoluteGateLufs)
    {
        return;
    }

    lraShortTermMeanSquares[static_cast<size_t> (lraWriteIndex)] =
        shortTermMeanSquare;

    lraWriteIndex = (lraWriteIndex + 1) % maxLraBlocks;
    lraBlockCount = juce::jmin (maxLraBlocks, lraBlockCount + 1);

    ++lraBlocksSinceLastUpdate;

    if (lraBlockCount < 10
        || lraBlocksSinceLastUpdate >= lraUpdateIntervalBlocks)
    {
        updateLoudnessRange();
    }
}

void LoudnessMeter::updateLoudnessRange() noexcept
{
    lraBlocksSinceLastUpdate = 0;

    if (lraBlockCount < 2)
    {
        loudnessRangeLu.store (0.0f, std::memory_order_relaxed);
        hasLoudnessRange.store (false, std::memory_order_relaxed);
        return;
    }

    auto absoluteGatedEnergySum = 0.0;
    auto absoluteGatedCount = 0;

    for (int i = 0; i < lraBlockCount; ++i)
    {
        const auto energy =
            lraShortTermMeanSquares[static_cast<size_t> (i)];

        const auto loudness = lufsFromMeanSquare (energy);

        if (loudness >= absoluteGateLufs)
        {
            absoluteGatedEnergySum += energy;
            ++absoluteGatedCount;
        }
    }

    if (absoluteGatedCount < 2)
    {
        loudnessRangeLu.store (0.0f, std::memory_order_relaxed);
        hasLoudnessRange.store (false, std::memory_order_relaxed);
        return;
    }

    const auto absoluteGatedMeanSquare =
        absoluteGatedEnergySum / static_cast<double> (absoluteGatedCount);

    const auto relativeGate =
        lufsFromMeanSquare (absoluteGatedMeanSquare) - 20.0;

    auto gatedLoudnessCount = 0;

    for (int i = 0; i < lraBlockCount; ++i)
    {
        const auto energy =
            lraShortTermMeanSquares[static_cast<size_t> (i)];

        const auto loudness = lufsFromMeanSquare (energy);

        if (loudness >= absoluteGateLufs && loudness >= relativeGate)
        {
            lraLoudnessScratch[static_cast<size_t> (gatedLoudnessCount)] =
                loudness;

            ++gatedLoudnessCount;
        }
    }

    if (gatedLoudnessCount < 2)
    {
        loudnessRangeLu.store (0.0f, std::memory_order_relaxed);
        hasLoudnessRange.store (false, std::memory_order_relaxed);
        return;
    }

    std::sort (lraLoudnessScratch.begin(),
               lraLoudnessScratch.begin() + gatedLoudnessCount);

    const auto lower =
        percentileFromSortedValues (lraLoudnessScratch,
                                    gatedLoudnessCount,
                                    0.10);

    const auto upper =
        percentileFromSortedValues (lraLoudnessScratch,
                                    gatedLoudnessCount,
                                    0.95);

    const auto range =
        juce::jmax (0.0, upper - lower);

    loudnessRangeLu.store (
        static_cast<float> (std::isfinite (range) ? range : 0.0),
        std::memory_order_relaxed);

    hasLoudnessRange.store (true, std::memory_order_relaxed);
}

double LoudnessMeter::percentileFromSortedValues (
    const std::array<double, maxLraBlocks>& sortedValues,
    int count,
    double percentile0To1) noexcept
{
    if (count <= 0)
        return 0.0;

    if (count == 1)
        return sortedValues[0];

    const auto clampedPercentile =
        juce::jlimit (0.0, 1.0, percentile0To1);

    const auto position =
        clampedPercentile * static_cast<double> (count - 1);

    const auto lowerIndex =
        static_cast<int> (std::floor (position));

    const auto upperIndex =
        juce::jmin (count - 1, lowerIndex + 1);

    const auto alpha =
        position - static_cast<double> (lowerIndex);

    const auto lower =
        sortedValues[static_cast<size_t> (lowerIndex)];

    const auto upper =
        sortedValues[static_cast<size_t> (upperIndex)];

    return lower + alpha * (upper - lower);
}

void LoudnessMeter::storeSnapshotValue (
    std::atomic<float>& target,
    double value) noexcept
{
    target.store (static_cast<float> (std::isfinite (value) ? value : -100.0),
                  std::memory_order_relaxed);
}


