#include "AnalyzerEngine.h"
#include "AnalyzerFrequencyRange.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace
{
    constexpr float hannCoherentGain = 0.5f;

    using DisplayBinPowerStats = AnalyzerEngine::DisplayBinPowerStats;

    float logFrequencyAtNormalisedPosition (float normalisedPosition,
                                            float minFrequency,
                                            float maxFrequency) noexcept
    {
        const auto clampedPosition = juce::jlimit (0.0f, 1.0f, normalisedPosition);

        return minFrequency * std::pow (maxFrequency / minFrequency, clampedPosition);
    }

    float smoothLogFrequencyBlend (float frequencyHz,
                                   float lowerHz,
                                   float upperHz) noexcept
    {
        if (! std::isfinite (frequencyHz))
            return 0.0f;

        if (std::isfinite (upperHz) && upperHz > 0.0f && frequencyHz >= upperHz)
            return 1.0f;

        if (frequencyHz <= 0.0f
            || ! std::isfinite (lowerHz)
            || ! std::isfinite (upperHz)
            || lowerHz <= 0.0f
            || upperHz <= lowerHz)
        {
            return 0.0f;
        }

        if (frequencyHz <= lowerHz)
            return 0.0f;

        const auto normalisedLogPosition =
            std::log (frequencyHz / lowerHz) / std::log (upperHz / lowerHz);

        const auto t = juce::jlimit (0.0f, 1.0f, normalisedLogPosition);

        return t * t * (3.0f - 2.0f * t);
    }

    float getCalibratedFftBinAmplitude (const std::vector<float>& frequencyOnlyFftData,
                                        int fftBin,
                                        int fftSize) noexcept
    {
        if (fftSize <= 0 || fftBin < 0)
            return 0.0f;

        const auto binIndex = static_cast<size_t> (fftBin);

        if (binIndex >= frequencyOnlyFftData.size())
            return 0.0f;

        const auto singleSidedAmplitude =
            (frequencyOnlyFftData[binIndex] / static_cast<float> (fftSize)) * 2.0f;

        return juce::jmax (0.0f, singleSidedAmplitude / hannCoherentGain);
    }

    DisplayBinPowerStats getFftBinPowerStatsForRange (
        const std::vector<float>& frequencyOnlyFftData,
        int fftSize,
        int firstBin,
        int lastBin,
        float leftBin,
        float rightBin) noexcept
    {
        DisplayBinPowerStats result;

        if (fftSize <= 0)
            return result;

        const auto maxAvailableBin = (fftSize / 2) - 1;

        if (maxAvailableBin < 1)
            return result;

        const auto clampedLeftBin =
            juce::jlimit (0.5f,
                          static_cast<float> (maxAvailableBin) + 0.5f,
                          leftBin);

        const auto clampedRightBin =
            juce::jlimit (clampedLeftBin,
                          static_cast<float> (maxAvailableBin) + 0.5f,
                          rightBin);

        if (clampedRightBin <= clampedLeftBin)
            return result;

        const auto clampedFirstBin = juce::jlimit (1, maxAvailableBin, firstBin);
        const auto clampedLastBin = juce::jlimit (1, maxAvailableBin, lastBin);

        if (clampedLastBin < clampedFirstBin)
            return result;

        auto weightedPowerSum = 0.0f;
        auto weightSum = 0.0f;
        auto peakPower = 0.0f;
        auto numBinsUsed = 0;

        for (auto bin = clampedFirstBin; bin <= clampedLastBin; ++bin)
        {
            const auto binLeft = static_cast<float> (bin) - 0.5f;
            const auto binRight = static_cast<float> (bin) + 0.5f;

            const auto overlap =
                juce::jmin (binRight, clampedRightBin)
                - juce::jmax (binLeft, clampedLeftBin);

            if (overlap <= 0.0f)
                continue;

            const auto amplitude =
                getCalibratedFftBinAmplitude (frequencyOnlyFftData, bin, fftSize);

            const auto power = amplitude * amplitude;

            weightedPowerSum += power * overlap;
            weightSum += overlap;
            peakPower = juce::jmax (peakPower, power);
            ++numBinsUsed;
        }

        if (numBinsUsed <= 0 || weightSum <= 0.0f)
            return result;

        result.meanPower = weightedPowerSum / weightSum;
        result.peakPower = peakPower;
        result.numBinsUsed = numBinsUsed;

        return result;
    }

    float getPeakPreservingDisplayPower (const DisplayBinPowerStats& stats) noexcept
    {
        if (stats.numBinsUsed <= 0)
            return 0.0f;

        const auto meanPower = juce::jmax (0.0f, stats.meanPower);
        const auto peakPower = juce::jmax (meanPower, stats.peakPower);

        if (stats.numBinsUsed == 1)
            return peakPower;

        if (meanPower <= 1.0e-20f)
            return peakPower;

        const auto peakToMeanRatio = peakPower / meanPower;
        const auto contrastDb = 10.0f * std::log10 (juce::jmax (1.0f, peakToMeanRatio));

        const auto contrastNormalised =
            juce::jlimit (0.0f, 1.0f, (contrastDb - 3.0f) / 9.0f);

        const auto peakWeight = contrastNormalised * 0.85f;

        return meanPower + peakWeight * (peakPower - meanPower);
    }

    float displayBinPowerStatsToDb (const DisplayBinPowerStats& stats) noexcept
    {
        const auto displayPower = getPeakPreservingDisplayPower (stats);
        const auto displayMagnitude = std::sqrt (displayPower);

        return juce::jlimit (
            -100.0f,
            0.0f,
            juce::Decibels::gainToDecibels (displayMagnitude, -100.0f));
    }
}

AnalyzerEngine::AnalyzerEngine()
    : juce::Thread ("FullSpectrum Analyzer Engine")
{
    rawSpectrumDb.resize (displayBinCount, -100.0f);
    smoothedSpectrumDb.resize (displayBinCount, -100.0f);
    peakHoldSpectrumDb.resize (displayBinCount, -100.0f);
    rmsPowerSpectrum.resize (displayBinCount, 0.0f);
    energyPowerSpectrum.resize (displayBinCount, 0.0f);
    energyFrameMeanPower.resize (displayBinCount, 0.0f);
    displayBinCenterFrequenciesHz.resize (displayBinCount, 0.0f);
    frequencyDependentTransientAssistAmounts.resize (displayBinCount, 0.0f);
    instantaneousNotePeaks.reserve (maxInstantaneousNotePeaks);
    trackedNotePeaks.reserve (maxInstantaneousNotePeaks);
    currentNotePeaks.reserve (maxPublishedNotePeaks);
    latestNotePeaks.reserve (maxPublishedNotePeaks);

    latestSpectrumDb.resize (displayBinCount, -100.0f);
    latestPeakHoldSpectrumDb.resize (displayBinCount, -100.0f);
    latestRmsSpectrumDb.resize (displayBinCount, -100.0f);
    latestEnergySpectrumDb.resize (displayBinCount, -100.0f);

    configureFft (defaultFftOrder);
    reset();
}

AnalyzerEngine::~AnalyzerEngine()
{
    stop();
}

void AnalyzerEngine::prepare (double sampleRate, AnalyzerFifo& fifoToReadFrom)
{
    stop();

    currentSampleRate = sampleRate;
    sourceFifo = &fifoToReadFrom;
    currentFrequencyDependentResolutionEnabled =
        requestedFrequencyDependentResolutionEnabled.load (std::memory_order_relaxed);

    configureFft (requestedFftOrder.load (std::memory_order_relaxed));
    reset();
}

void AnalyzerEngine::reset()
{
    resetOverlapBuffer();

    if (currentFrequencyDependentResolutionEnabled)
    {
        for (const auto& descriptor : getFrequencyDependentSourceDescriptors())
        {
            if (descriptor.source != nullptr)
                resetFrequencyDependentFftSource (*descriptor.source);
        }
    }

    secondsSinceLastFramePublish = 0.0f;

    const auto requestedDisplayMinimum =
        requestedDisplayMinFrequencyHz.load (std::memory_order_relaxed);

    const auto requestedDisplayMaximum =
        requestedDisplayMaxFrequencyHz.load (std::memory_order_relaxed);

    currentDisplayMinFrequencyHz =
        juce::jlimit (AnalyzerFrequencyRange::minimumHz,
                      AnalyzerFrequencyRange::maximumHz - 1.0f,
                      requestedDisplayMinimum);

    currentDisplayMaxFrequencyHz =
        juce::jlimit (currentDisplayMinFrequencyHz + 1.0f,
                      AnalyzerFrequencyRange::maximumHz,
                      requestedDisplayMaximum);

    displayBinRangeSampleRate = 0.0f;
    displayBinRangeFftSize = 0;
    displayBinRangeMinFrequencyHz = 0.0f;
    displayBinRangeMaxFrequencyHz = 0.0f;

    std::fill (fftData.begin(), fftData.end(), 0.0f);
    std::fill (rawSpectrumDb.begin(), rawSpectrumDb.end(), -100.0f);
    std::fill (smoothedSpectrumDb.begin(), smoothedSpectrumDb.end(), -100.0f);
    std::fill (peakHoldSpectrumDb.begin(), peakHoldSpectrumDb.end(), -100.0f);
    std::fill (rmsPowerSpectrum.begin(), rmsPowerSpectrum.end(), 0.0f);
    std::fill (energyPowerSpectrum.begin(), energyPowerSpectrum.end(), 0.0f);
    std::fill (energyFrameMeanPower.begin(), energyFrameMeanPower.end(), 0.0f);
    if (frequencyDependentTransientAssistAmounts.size() != static_cast<size_t> (displayBinCount))
    {
        frequencyDependentTransientAssistAmounts.assign (static_cast<size_t> (displayBinCount),
                                                         0.0f);
    }
    else
    {
        std::fill (frequencyDependentTransientAssistAmounts.begin(),
                   frequencyDependentTransientAssistAmounts.end(),
                   0.0f);
    }
    energyAccumulatedActiveSeconds = 0.0f;
    displayAccumulationWarmStartRequested = false;
    instantaneousNotePeaks.clear();
    trackedNotePeaks.clear();
    currentNotePeaks.clear();

    {
        std::lock_guard<std::mutex> lock (latestSpectrumMutex);
        std::fill (latestSpectrumDb.begin(), latestSpectrumDb.end(), -100.0f);
        std::fill (latestPeakHoldSpectrumDb.begin(), latestPeakHoldSpectrumDb.end(), -100.0f);
        std::fill (latestRmsSpectrumDb.begin(), latestRmsSpectrumDb.end(), -100.0f);
        std::fill (latestEnergySpectrumDb.begin(), latestEnergySpectrumDb.end(), -100.0f);
        latestFrameMinFrequencyHz = AnalyzerFrequencyRange::minimumHz;
        latestFrameMaxFrequencyHz = AnalyzerFrequencyRange::maximumHz;
        latestNotePeaks.clear();
    }

    hasFrame.store (false, std::memory_order_relaxed);
}

void AnalyzerEngine::requestClearPeakHold() noexcept
{
    clearPeakHoldRequested.store (true, std::memory_order_relaxed);
    notify();
}

void AnalyzerEngine::requestClearEnergy() noexcept
{
    clearEnergyRequested.store (true, std::memory_order_relaxed);
    notify();
}

void AnalyzerEngine::handleClearPeakHoldRequest()
{
    if (! clearPeakHoldRequested.exchange (false, std::memory_order_relaxed))
        return;

    std::fill (peakHoldSpectrumDb.begin(), peakHoldSpectrumDb.end(), -100.0f);
    instantaneousNotePeaks.clear();
    trackedNotePeaks.clear();
    currentNotePeaks.clear();

    std::lock_guard<std::mutex> lock (latestSpectrumMutex);
    std::fill (latestPeakHoldSpectrumDb.begin(), latestPeakHoldSpectrumDb.end(), -100.0f);
    latestNotePeaks.clear();
}

void AnalyzerEngine::handleClearEnergyRequest()
{
    if (! clearEnergyRequested.exchange (false, std::memory_order_relaxed))
        return;

    std::fill (energyPowerSpectrum.begin(), energyPowerSpectrum.end(), 0.0f);
    std::fill (energyFrameMeanPower.begin(), energyFrameMeanPower.end(), 0.0f);
    energyAccumulatedActiveSeconds = 0.0f;

    std::lock_guard<std::mutex> lock (latestSpectrumMutex);
    std::fill (latestEnergySpectrumDb.begin(), latestEnergySpectrumDb.end(), -100.0f);
}

void AnalyzerEngine::setPeakHoldDecayDbPerSecond (float newDecayDbPerSecond) noexcept
{
    peakHoldDecayDbPerSecond.store (
        juce::jlimit (0.0f, 60.0f, newDecayDbPerSecond),
        std::memory_order_relaxed);
}

void AnalyzerEngine::setRmsTimeSeconds (float newRmsTimeSeconds) noexcept
{
    rmsTimeSeconds.store (
        juce::jlimit (0.010f, 10.0f, newRmsTimeSeconds),
        std::memory_order_relaxed);
}

void AnalyzerEngine::setRequestedFftOrder (int newFftOrder) noexcept
{
    requestedFftOrder.store (
        juce::jlimit (minFftOrder, maxFftOrder, newFftOrder),
        std::memory_order_relaxed);
}

void AnalyzerEngine::setFrequencyDependentResolutionEnabled (
    bool shouldUseFrequencyDependentResolution) noexcept
{
    requestedFrequencyDependentResolutionEnabled.store (
        shouldUseFrequencyDependentResolution,
        std::memory_order_relaxed);
}

void AnalyzerEngine::setDisplayFrequencyRange (float minimumHz, float maximumHz) noexcept
{
    const auto clampedMinimum =
        juce::jlimit (AnalyzerFrequencyRange::minimumHz,
                      AnalyzerFrequencyRange::maximumHz - 1.0f,
                      minimumHz);

    const auto clampedMaximum =
        juce::jlimit (clampedMinimum + 1.0f,
                      AnalyzerFrequencyRange::maximumHz,
                      maximumHz);

    requestedDisplayMinFrequencyHz.store (clampedMinimum, std::memory_order_relaxed);
    requestedDisplayMaxFrequencyHz.store (clampedMaximum, std::memory_order_relaxed);
}

std::array<AnalyzerEngine::FrequencyDependentSourceDescriptor, 4>
AnalyzerEngine::getFrequencyDependentSourceDescriptors() noexcept
{
    return {{
        {
            FrequencyDependentSourceRole::bass,
            &frequencyDependentBassPath,
            frequencyDependentBassFftOrder,
            frequencyDependentBassFftSize
        },
        {
            FrequencyDependentSourceRole::midBass,
            &frequencyDependentMidBassPath,
            frequencyDependentMidBassFftOrder,
            frequencyDependentMidBassFftSize
        },
        {
            FrequencyDependentSourceRole::high,
            &frequencyDependentHighPath,
            frequencyDependentHighFftOrder,
            frequencyDependentHighFftSize
        },
        {
            FrequencyDependentSourceRole::veryHigh,
            &frequencyDependentVeryHighPath,
            frequencyDependentVeryHighFftOrder,
            frequencyDependentVeryHighFftSize
        }
    }};
}

bool AnalyzerEngine::canUseFrequencyDependentSource (
    const FrequencyDependentFftSource& source,
    int fftSize) const noexcept
{
    return fftSize > 0
           && source.hasValidFftData
           && source.fftData.size() >= static_cast<size_t> (fftSize * 2)
           && source.displayBinFftRanges.size() == static_cast<size_t> (displayBinCount)
           && source.rangeSampleRate > 0.0f
           && source.rangeMinFrequencyHz > 0.0f
           && source.rangeMaxFrequencyHz > source.rangeMinFrequencyHz
           && displayBinCenterFrequenciesHz.size() == static_cast<size_t> (displayBinCount);
}

AnalyzerEngine::DisplayBinPowerStats
AnalyzerEngine::getFrequencyDependentSourceStatsForDisplayBin (
    const FrequencyDependentFftSource& source,
    int fftSize,
    size_t displayBinIndex) const noexcept
{
    if (fftSize <= 0)
        return {};

    if (source.displayBinFftRanges.size() <= displayBinIndex)
        return {};

    if (source.fftData.size() < static_cast<size_t> (fftSize * 2))
        return {};

    const auto& sourceDisplayBinRange =
        source.displayBinFftRanges[displayBinIndex];

    return getFftBinPowerStatsForRange (source.fftData,
                                        fftSize,
                                        sourceDisplayBinRange.firstBin,
                                        sourceDisplayBinRange.lastBin,
                                        sourceDisplayBinRange.leftBin,
                                        sourceDisplayBinRange.rightBin);
}

void AnalyzerEngine::configureFft (int newFftOrder)
{
    currentFftOrder = juce::jlimit (minFftOrder, maxFftOrder, newFftOrder);
    currentFftSize = analyzerFftSizeFromOrder (currentFftOrder);

    forwardFFT = std::make_unique<juce::dsp::FFT> (currentFftOrder);

    window = std::make_unique<juce::dsp::WindowingFunction<float>> (
        static_cast<size_t> (currentFftSize),
        juce::dsp::WindowingFunction<float>::hann,
        false);

    timeDomainBlock.assign (static_cast<size_t> (currentFftSize), 0.0f);
    hopBuffer.assign (static_cast<size_t> (getFftHopSize()), 0.0f);
    fftData.assign (static_cast<size_t> (currentFftSize * 2), 0.0f);
    notePeakBinDecibels.assign (static_cast<size_t> (currentFftSize / 2), notePeakMinAbsoluteDb);

    displayBinFftRanges.assign (static_cast<size_t> (displayBinCount), {});
    displayBinRangeSampleRate = 0.0f;
    displayBinRangeFftSize = 0;
    displayBinRangeMinFrequencyHz = 0.0f;
    displayBinRangeMaxFrequencyHz = 0.0f;

    overlapBufferPrimed = false;

    if (currentFrequencyDependentResolutionEnabled)
    {
        for (const auto& descriptor : getFrequencyDependentSourceDescriptors())
        {
            if (descriptor.source != nullptr)
            {
                configureFrequencyDependentFftSource (*descriptor.source,
                                                       descriptor.fftOrder,
                                                       descriptor.fftSize);
            }
        }
    }
    else
    {
        for (const auto& descriptor : getFrequencyDependentSourceDescriptors())
        {
            if (descriptor.source != nullptr)
                descriptor.source->hasValidFftData = false;
        }
    }
}

void AnalyzerEngine::resetOverlapBuffer()
{
    std::fill (timeDomainBlock.begin(), timeDomainBlock.end(), 0.0f);
    std::fill (hopBuffer.begin(), hopBuffer.end(), 0.0f);

    overlapBufferPrimed = false;
}

void AnalyzerEngine::configureFrequencyDependentFftSource (
    FrequencyDependentFftSource& source,
    int fftOrder,
    int fftSize)
{
    source.fft = std::make_unique<juce::dsp::FFT> (fftOrder);

    source.window =
        std::make_unique<juce::dsp::WindowingFunction<float>> (
            static_cast<size_t> (fftSize),
            juce::dsp::WindowingFunction<float>::hann,
            false);

    source.timeDomainBlock.assign (static_cast<size_t> (fftSize), 0.0f);

    source.fftData.assign (static_cast<size_t> (fftSize * 2), 0.0f);

    source.displayBinFftRanges.assign (static_cast<size_t> (displayBinCount), {});

    source.fftOrder = fftOrder;
    source.fftSize = fftSize;
    source.nominalWindowSeconds =
        currentSampleRate > 0.0 ? static_cast<float> (fftSize / currentSampleRate) : 0.0f;
    source.rangeSampleRate = 0.0f;
    source.rangeMinFrequencyHz = 0.0f;
    source.rangeMaxFrequencyHz = 0.0f;
    source.samplesCollected = 0;
    source.hasValidFftData = false;
}

void AnalyzerEngine::resetFrequencyDependentFftSource (FrequencyDependentFftSource& source)
{
    std::fill (source.timeDomainBlock.begin(), source.timeDomainBlock.end(), 0.0f);

    std::fill (source.fftData.begin(), source.fftData.end(), 0.0f);

    source.rangeSampleRate = 0.0f;
    source.rangeMinFrequencyHz = 0.0f;
    source.rangeMaxFrequencyHz = 0.0f;
    source.samplesCollected = 0;
    source.hasValidFftData = false;
}

void AnalyzerEngine::configureFrequencyDependentBassPath()
{
    configureFrequencyDependentFftSource (frequencyDependentBassPath,
                                          frequencyDependentBassFftOrder,
                                          frequencyDependentBassFftSize);
}

void AnalyzerEngine::resetFrequencyDependentBassPath()
{
    resetFrequencyDependentFftSource (frequencyDependentBassPath);
}

void AnalyzerEngine::configureFrequencyDependentHighPath()
{
    configureFrequencyDependentFftSource (frequencyDependentHighPath,
                                          frequencyDependentHighFftOrder,
                                          frequencyDependentHighFftSize);
}

void AnalyzerEngine::resetFrequencyDependentHighPath()
{
    resetFrequencyDependentFftSource (frequencyDependentHighPath);
}

void AnalyzerEngine::requestDisplayAccumulationWarmStartForRangeChange() noexcept
{
    displayAccumulationWarmStartRequested = true;
    secondsSinceLastFramePublish = 1.0f / latestFramePublishRateHz;
}

void AnalyzerEngine::updateDisplayBinFftRangesIfNeeded()
{
    const auto fftSizeForRanges = currentFftSize;
    const auto sampleRateForRanges = static_cast<float> (currentSampleRate);

    if (fftSizeForRanges <= 0 || sampleRateForRanges <= 0.0f)
        return;

    const auto nyquistLimitedMaximum =
        AnalyzerFrequencyRange::getMaximumHzForSampleRate (sampleRateForRanges);

    if (nyquistLimitedMaximum <= AnalyzerFrequencyRange::minimumHz)
        return;

    const auto requestedMinimum =
        requestedDisplayMinFrequencyHz.load (std::memory_order_relaxed);

    const auto requestedMaximum =
        requestedDisplayMaxFrequencyHz.load (std::memory_order_relaxed);

    const auto clampedMinimum =
        juce::jlimit (AnalyzerFrequencyRange::minimumHz,
                      nyquistLimitedMaximum - 1.0f,
                      requestedMinimum);

    const auto clampedMaximum =
        juce::jlimit (clampedMinimum + 1.0f,
                      nyquistLimitedMaximum,
                      requestedMaximum);

    currentDisplayMinFrequencyHz = clampedMinimum;
    currentDisplayMaxFrequencyHz = clampedMaximum;

    const auto hadValidDisplayRangeCache =
        displayBinFftRanges.size() == static_cast<size_t> (displayBinCount)
        && displayBinRangeFftSize > 0
        && displayBinRangeSampleRate > 0.0f
        && displayBinRangeMinFrequencyHz > 0.0f
        && displayBinRangeMaxFrequencyHz > displayBinRangeMinFrequencyHz;

    const auto displayMappingChanged =
        hadValidDisplayRangeCache
        && (displayBinRangeFftSize != fftSizeForRanges
            || std::abs (displayBinRangeSampleRate - sampleRateForRanges) >= 0.001f
            || std::abs (displayBinRangeMinFrequencyHz - currentDisplayMinFrequencyHz) >= 0.001f
            || std::abs (displayBinRangeMaxFrequencyHz - currentDisplayMaxFrequencyHz) >= 0.001f);

    if (displayBinFftRanges.size() == static_cast<size_t> (displayBinCount)
        && displayBinRangeFftSize == fftSizeForRanges
        && std::abs (displayBinRangeSampleRate - sampleRateForRanges) < 0.001f
        && std::abs (displayBinRangeMinFrequencyHz - currentDisplayMinFrequencyHz) < 0.001f
        && std::abs (displayBinRangeMaxFrequencyHz - currentDisplayMaxFrequencyHz) < 0.001f)
    {
        return;
    }

    if (displayMappingChanged)
    {
        requestDisplayAccumulationWarmStartForRangeChange();

        if (currentFrequencyDependentResolutionEnabled)
        {
            for (const auto& descriptor : getFrequencyDependentSourceDescriptors())
            {
                if (descriptor.source != nullptr)
                    descriptor.source->hasValidFftData = false;
            }
        }
    }

    displayBinFftRanges.assign (static_cast<size_t> (displayBinCount), {});

    const auto minFrequency = currentDisplayMinFrequencyHz;
    const auto maxFrequency = currentDisplayMaxFrequencyHz;

    const auto maxAvailableBin = (fftSizeForRanges / 2) - 1;

    if (maxAvailableBin < 1)
    {
        displayBinRangeSampleRate = sampleRateForRanges;
        displayBinRangeFftSize = fftSizeForRanges;
        displayBinRangeMinFrequencyHz = currentDisplayMinFrequencyHz;
        displayBinRangeMaxFrequencyHz = currentDisplayMaxFrequencyHz;
        return;
    }

    const auto displayDenominator = static_cast<float> (displayBinCount - 1);
    const auto fftBinsPerHz =
        static_cast<float> (fftSizeForRanges) / sampleRateForRanges;

    for (int i = 0; i < displayBinCount; ++i)
    {
        const auto leftNormalised =
            (static_cast<float> (i) - 0.5f) / displayDenominator;

        const auto centerNormalised =
            static_cast<float> (i) / displayDenominator;

        const auto rightNormalised =
            (static_cast<float> (i) + 0.5f) / displayDenominator;

        const auto leftFrequency =
            logFrequencyAtNormalisedPosition (leftNormalised, minFrequency, maxFrequency);

        const auto centerFrequency =
            logFrequencyAtNormalisedPosition (centerNormalised, minFrequency, maxFrequency);

        const auto rightFrequency =
            logFrequencyAtNormalisedPosition (rightNormalised, minFrequency, maxFrequency);

        const auto leftBin =
            juce::jlimit (0.5f,
                          static_cast<float> (maxAvailableBin) + 0.5f,
                          leftFrequency * fftBinsPerHz);

        const auto rightBin =
            juce::jlimit (leftBin,
                          static_cast<float> (maxAvailableBin) + 0.5f,
                          rightFrequency * fftBinsPerHz);

        const auto firstBin =
            juce::jlimit (1,
                          maxAvailableBin,
                          static_cast<int> (std::floor (leftBin - 0.5f)));

        const auto lastBin =
            juce::jlimit (1,
                          maxAvailableBin,
                          static_cast<int> (std::ceil (rightBin + 0.5f)));

        auto& range = displayBinFftRanges[static_cast<size_t> (i)];
        range.leftBin = leftBin;
        range.rightBin = rightBin;
        range.firstBin = firstBin;
        range.lastBin = juce::jmax (firstBin, lastBin);

        if (displayBinCenterFrequenciesHz.size() == static_cast<size_t> (displayBinCount))
            displayBinCenterFrequenciesHz[static_cast<size_t> (i)] = centerFrequency;
    }

    displayBinRangeSampleRate = sampleRateForRanges;
    displayBinRangeFftSize = fftSizeForRanges;
    displayBinRangeMinFrequencyHz = currentDisplayMinFrequencyHz;
    displayBinRangeMaxFrequencyHz = currentDisplayMaxFrequencyHz;
}

void AnalyzerEngine::appendSamplesToFrequencyDependentFftSource (
    FrequencyDependentFftSource& source,
    const float* samples,
    int numSamples,
    int fftSize)
{
    if (samples == nullptr || numSamples <= 0 || fftSize <= 0)
        return;

    if (source.timeDomainBlock.size() != static_cast<size_t> (fftSize))
    {
        return;
    }

    if (numSamples >= fftSize)
    {
        std::copy (samples + (numSamples - fftSize),
                   samples + numSamples,
                   source.timeDomainBlock.begin());

        source.samplesCollected = fftSize;
        source.hasValidFftData = false;
        return;
    }

    std::copy (source.timeDomainBlock.begin() + numSamples,
               source.timeDomainBlock.end(),
               source.timeDomainBlock.begin());

    std::copy (samples,
               samples + numSamples,
               source.timeDomainBlock.begin() + (fftSize - numSamples));

    source.samplesCollected =
        juce::jmin (fftSize, source.samplesCollected + numSamples);

    source.hasValidFftData = false;
}

void AnalyzerEngine::processFrequencyDependentFftSourceIfReady (
    FrequencyDependentFftSource& source,
    int fftSize)
{
    if (source.samplesCollected < fftSize)
    {
        source.hasValidFftData = false;
        return;
    }

    if (source.fft == nullptr
        || source.window == nullptr
        || source.timeDomainBlock.size() < static_cast<size_t> (fftSize)
        || source.fftData.size() < static_cast<size_t> (fftSize * 2))
    {
        source.hasValidFftData = false;
        return;
    }

    std::fill (source.fftData.begin(),
               source.fftData.end(),
               0.0f);

    std::copy (source.timeDomainBlock.begin(),
               source.timeDomainBlock.end(),
               source.fftData.begin());

    source.window->multiplyWithWindowingTable (source.fftData.data(),
                                               static_cast<size_t> (fftSize));

    source.fft->performFrequencyOnlyForwardTransform (source.fftData.data());

    source.hasValidFftData = true;
}

void AnalyzerEngine::appendSamplesToFrequencyDependentBassPath (
    const float* samples,
    int numSamples)
{
    appendSamplesToFrequencyDependentFftSource (frequencyDependentBassPath,
                                                samples,
                                                numSamples,
                                                frequencyDependentBassFftSize);
}

void AnalyzerEngine::processFrequencyDependentBassPathIfReady()
{
    processFrequencyDependentFftSourceIfReady (frequencyDependentBassPath,
                                               frequencyDependentBassFftSize);
}

void AnalyzerEngine::appendSamplesToFrequencyDependentHighPath (
    const float* samples,
    int numSamples)
{
    appendSamplesToFrequencyDependentFftSource (frequencyDependentHighPath,
                                                samples,
                                                numSamples,
                                                frequencyDependentHighFftSize);
}

void AnalyzerEngine::processFrequencyDependentHighPathIfReady()
{
    processFrequencyDependentFftSourceIfReady (frequencyDependentHighPath,
                                               frequencyDependentHighFftSize);
}

void AnalyzerEngine::updateFrequencyDependentSourceBinFftRangesIfNeeded (
    FrequencyDependentFftSource& source,
    int fftSize)
{
    const auto sampleRateForRanges = static_cast<float> (currentSampleRate);

    if (fftSize <= 0 || sampleRateForRanges <= 0.0f)
        return;

    const auto nyquistLimitedMaximum =
        AnalyzerFrequencyRange::getMaximumHzForSampleRate (sampleRateForRanges);

    if (nyquistLimitedMaximum <= AnalyzerFrequencyRange::minimumHz)
        return;

    const auto clampedMinimum =
        juce::jlimit (AnalyzerFrequencyRange::minimumHz,
                      nyquistLimitedMaximum - 1.0f,
                      currentDisplayMinFrequencyHz);

    const auto clampedMaximum =
        juce::jlimit (clampedMinimum + 1.0f,
                      nyquistLimitedMaximum,
                      currentDisplayMaxFrequencyHz);

    if (source.displayBinFftRanges.size()
            == static_cast<size_t> (displayBinCount)
        && std::abs (source.rangeSampleRate - sampleRateForRanges) < 0.001f
        && std::abs (source.rangeMinFrequencyHz - clampedMinimum) < 0.001f
        && std::abs (source.rangeMaxFrequencyHz - clampedMaximum) < 0.001f)
    {
        return;
    }

    source.displayBinFftRanges.assign (static_cast<size_t> (displayBinCount), {});

    const auto maxAvailableBin = (fftSize / 2) - 1;

    if (maxAvailableBin < 1)
    {
        source.rangeSampleRate = sampleRateForRanges;
        source.rangeMinFrequencyHz = clampedMinimum;
        source.rangeMaxFrequencyHz = clampedMaximum;
        return;
    }

    const auto displayDenominator = static_cast<float> (displayBinCount - 1);
    const auto fftBinsPerHz =
        static_cast<float> (fftSize) / sampleRateForRanges;

    for (int i = 0; i < displayBinCount; ++i)
    {
        const auto leftNormalised =
            (static_cast<float> (i) - 0.5f) / displayDenominator;

        const auto rightNormalised =
            (static_cast<float> (i) + 0.5f) / displayDenominator;

        const auto leftFrequency =
            logFrequencyAtNormalisedPosition (leftNormalised, clampedMinimum, clampedMaximum);

        const auto rightFrequency =
            logFrequencyAtNormalisedPosition (rightNormalised, clampedMinimum, clampedMaximum);

        const auto leftBin =
            juce::jlimit (0.5f,
                          static_cast<float> (maxAvailableBin) + 0.5f,
                          leftFrequency * fftBinsPerHz);

        const auto rightBin =
            juce::jlimit (leftBin,
                          static_cast<float> (maxAvailableBin) + 0.5f,
                          rightFrequency * fftBinsPerHz);

        const auto firstBin =
            juce::jlimit (1,
                          maxAvailableBin,
                          static_cast<int> (std::floor (leftBin - 0.5f)));

        const auto lastBin =
            juce::jlimit (1,
                          maxAvailableBin,
                          static_cast<int> (std::ceil (rightBin + 0.5f)));

        auto& range = source.displayBinFftRanges[static_cast<size_t> (i)];

        range.leftBin = leftBin;
        range.rightBin = rightBin;
        range.firstBin = firstBin;
        range.lastBin = juce::jmax (firstBin, lastBin);
    }

    source.rangeSampleRate = sampleRateForRanges;
    source.rangeMinFrequencyHz = clampedMinimum;
    source.rangeMaxFrequencyHz = clampedMaximum;
}

void AnalyzerEngine::updateFrequencyDependentBassBinFftRangesIfNeeded()
{
    updateFrequencyDependentSourceBinFftRangesIfNeeded (frequencyDependentBassPath,
                                                        frequencyDependentBassFftSize);
}

void AnalyzerEngine::updateFrequencyDependentHighBinFftRangesIfNeeded()
{
    updateFrequencyDependentSourceBinFftRangesIfNeeded (frequencyDependentHighPath,
                                                        frequencyDependentHighFftSize);
}

float AnalyzerEngine::getFrequencyDependentMidBassBlendForFrequency (
    float frequencyHz) const noexcept
{
    return smoothLogFrequencyBlend (frequencyHz,
                                    frequencyDependentDeepBassOnlyMaxHz,
                                    frequencyDependentMidBassOnlyMinHz);
}

float AnalyzerEngine::getFrequencyDependentMainBlendForFrequency (
    float frequencyHz) const noexcept
{
    return smoothLogFrequencyBlend (frequencyHz,
                                    frequencyDependentBassOnlyMaxHz,
                                    frequencyDependentMainOnlyMinHz);
}

float AnalyzerEngine::getFrequencyDependentHighBlendForFrequency (
    float frequencyHz) const noexcept
{
    return smoothLogFrequencyBlend (frequencyHz,
                                    frequencyDependentMainOnlyMaxHz,
                                    frequencyDependentHighOnlyMinHz);
}

float AnalyzerEngine::getFrequencyDependentVeryHighBlendForFrequency (
    float frequencyHz) const noexcept
{
    return smoothLogFrequencyBlend (frequencyHz,
                                    frequencyDependentHighOnlyMinHz,
                                    frequencyDependentVeryHighOnlyMinHz);
}

AnalyzerEngine::FrequencyDependentLowCompositeResult
AnalyzerEngine::getFrequencyDependentLowCompositeForDisplayBin (
    size_t displayBinIndex,
    float centerFrequencyHz,
    const DisplayBinPowerStats& mainStats,
    bool canUseFrequencyDependentBassPath,
    bool canUseFrequencyDependentMidBassPath) const
{
    FrequencyDependentLowCompositeResult result;

    if (canUseFrequencyDependentBassPath
        && frequencyDependentBassPath.displayBinFftRanges.size() > displayBinIndex)
    {
        result.lowCompositeStats =
            getFrequencyDependentSourceStatsForDisplayBin (
                frequencyDependentBassPath,
                frequencyDependentBassFftSize,
                displayBinIndex);

        result.hasLowCompositeStats =
            result.lowCompositeStats.numBinsUsed > 0;
    }

    if (canUseFrequencyDependentMidBassPath
        && frequencyDependentMidBassPath.displayBinFftRanges.size() > displayBinIndex)
    {
        const auto midBassBinPowerStats =
            getFrequencyDependentSourceStatsForDisplayBin (
                frequencyDependentMidBassPath,
                frequencyDependentMidBassFftSize,
                displayBinIndex);

        if (midBassBinPowerStats.numBinsUsed > 0)
        {
            if (centerFrequencyHz <= frequencyDependentMainOnlyMinHz)
            {
                const auto transientReferenceMainBlend =
                    getFrequencyDependentMainBlendForFrequency (centerFrequencyHz);

                result.transientReferenceStats =
                    blendDisplayBinPowerStats (midBassBinPowerStats,
                                               mainStats,
                                               transientReferenceMainBlend);
                result.hasTransientReferenceStats = true;
            }

            if (result.hasLowCompositeStats)
            {
                const auto midBassBlend =
                    getFrequencyDependentMidBassBlendForFrequency (
                        centerFrequencyHz);

                result.lowCompositeStats =
                    blendDisplayBinPowerStats (result.lowCompositeStats,
                                               midBassBinPowerStats,
                                               midBassBlend);
            }
            else
            {
                result.lowCompositeStats = midBassBinPowerStats;
                result.hasLowCompositeStats = true;
            }
        }
    }

    return result;
}

AnalyzerEngine::DisplayBinPowerStats
AnalyzerEngine::applyFrequencyDependentHighBlendForDisplayBin (
    size_t displayBinIndex,
    float centerFrequencyHz,
    const DisplayBinPowerStats& baseStats,
    bool canUseFrequencyDependentHighPath,
    bool canUseFrequencyDependentVeryHighPath) const
{
    if (centerFrequencyHz <= 0.0f)
        return baseStats;

    auto highCompositeStats = baseStats;

    if (canUseFrequencyDependentHighPath
        && frequencyDependentHighPath.displayBinFftRanges.size() > displayBinIndex)
    {
        const auto highBlend =
            getFrequencyDependentHighBlendForFrequency (centerFrequencyHz);

        if (highBlend > 0.0f)
        {
            const auto highBinPowerStats =
                getFrequencyDependentSourceStatsForDisplayBin (
                    frequencyDependentHighPath,
                    frequencyDependentHighFftSize,
                    displayBinIndex);

            highCompositeStats =
                blendDisplayBinPowerStats (highCompositeStats,
                                           highBinPowerStats,
                                           highBlend);
        }
    }

    if (canUseFrequencyDependentVeryHighPath
        && frequencyDependentVeryHighPath.displayBinFftRanges.size() > displayBinIndex)
    {
        const auto veryHighBlend =
            getFrequencyDependentVeryHighBlendForFrequency (centerFrequencyHz);

        if (veryHighBlend > 0.0f)
        {
            const auto veryHighBinPowerStats =
                getFrequencyDependentSourceStatsForDisplayBin (
                    frequencyDependentVeryHighPath,
                    frequencyDependentVeryHighFftSize,
                    displayBinIndex);

            highCompositeStats =
                blendDisplayBinPowerStats (highCompositeStats,
                                           veryHighBinPowerStats,
                                           veryHighBlend);
        }
    }

    return highCompositeStats;
}

AnalyzerEngine::FrequencyDependentBinStats
AnalyzerEngine::getFrequencyDependentBinStatsForDisplayBin (
    int displayBinIndex,
    int fftSizeForBlock,
    bool canUseFrequencyDependentBassPath,
    bool canUseFrequencyDependentMidBassPath,
    bool canUseFrequencyDependentHighPath,
    bool canUseFrequencyDependentVeryHighPath) const
{
    FrequencyDependentBinStats result;

    if (displayBinIndex < 0 || fftSizeForBlock <= 0)
        return result;

    const auto index = static_cast<size_t> (displayBinIndex);

    if (displayBinFftRanges.size() <= index)
        return result;

    const auto& displayBinRange = displayBinFftRanges[index];

    result.mainStats =
        getFftBinPowerStatsForRange (fftData,
                                     fftSizeForBlock,
                                     displayBinRange.firstBin,
                                     displayBinRange.lastBin,
                                     displayBinRange.leftBin,
                                     displayBinRange.rightBin);

    result.compositeStats = result.mainStats;
    result.transientReferenceStats = result.mainStats;
    result.hasTransientReferenceStats = result.mainStats.numBinsUsed > 0;

    if (displayBinCenterFrequenciesHz.size() > index)
    {
        result.centerFrequencyHz = displayBinCenterFrequenciesHz[index];
        result.hasCenterFrequency = result.centerFrequencyHz > 0.0f;
    }

    if (result.hasCenterFrequency)
    {
        const auto lowCompositeResult =
            getFrequencyDependentLowCompositeForDisplayBin (
                index,
                result.centerFrequencyHz,
                result.mainStats,
                canUseFrequencyDependentBassPath,
                canUseFrequencyDependentMidBassPath);

        if (lowCompositeResult.hasTransientReferenceStats)
        {
            result.transientReferenceStats =
                lowCompositeResult.transientReferenceStats;
            result.hasTransientReferenceStats = true;
        }

        if (lowCompositeResult.hasLowCompositeStats)
        {
            const auto mainBlend =
                getFrequencyDependentMainBlendForFrequency (
                    result.centerFrequencyHz);

            result.compositeStats =
                blendDisplayBinPowerStats (lowCompositeResult.lowCompositeStats,
                                           result.mainStats,
                                           mainBlend);
        }

        result.compositeStats =
            applyFrequencyDependentHighBlendForDisplayBin (
                index,
                result.centerFrequencyHz,
                result.compositeStats,
                canUseFrequencyDependentHighPath,
                canUseFrequencyDependentVeryHighPath);
    }

    return result;
}

AnalyzerEngine::DisplayBinPowerStats AnalyzerEngine::blendDisplayBinPowerStats (
    const DisplayBinPowerStats& bassStats,
    const DisplayBinPowerStats& mainStats,
    float mainBlend) const noexcept
{
    const auto sanitizePower = [] (float power) noexcept
    {
        return std::isfinite (power) ? juce::jmax (0.0f, power) : 0.0f;
    };

    if (bassStats.numBinsUsed <= 0)
    {
        return {
            sanitizePower (mainStats.meanPower),
            sanitizePower (mainStats.peakPower),
            juce::jmax (0, mainStats.numBinsUsed)
        };
    }

    if (mainStats.numBinsUsed <= 0)
    {
        return {
            sanitizePower (bassStats.meanPower),
            sanitizePower (bassStats.peakPower),
            juce::jmax (0, bassStats.numBinsUsed)
        };
    }

    const auto clampedMainBlend = juce::jlimit (0.0f, 1.0f, mainBlend);
    const auto bassBlend = 1.0f - clampedMainBlend;

    return {
        sanitizePower (bassStats.meanPower) * bassBlend
            + sanitizePower (mainStats.meanPower) * clampedMainBlend,
        sanitizePower (bassStats.peakPower) * bassBlend
            + sanitizePower (mainStats.peakPower) * clampedMainBlend,
        juce::jmax (bassStats.numBinsUsed, mainStats.numBinsUsed)
    };
}

AnalyzerEngine::DisplayBinPowerStats AnalyzerEngine::applyFrequencyDependentTransientAssist (
    const DisplayBinPowerStats& frequencyDependentStats,
    const DisplayBinPowerStats& mainStats,
    float centerFrequencyHz,
    float assistAmount) const noexcept
{
    if (centerFrequencyHz <= 0.0f
        || centerFrequencyHz > frequencyDependentTransientAssistMaxHz)
    {
        return frequencyDependentStats;
    }

    const auto frequencyDependentHasBins = frequencyDependentStats.numBinsUsed > 0;
    const auto mainHasBins = mainStats.numBinsUsed > 0;

    if (! frequencyDependentHasBins)
        return mainHasBins ? blendDisplayBinPowerStats (frequencyDependentStats,
                                                        mainStats,
                                                        1.0f)
                           : frequencyDependentStats;

    if (! mainHasBins)
        return frequencyDependentStats;

    if (! std::isfinite (assistAmount) || assistAmount <= 0.0f)
        return frequencyDependentStats;

    const auto assistBlend =
        juce::jlimit (0.0f, 1.0f, assistAmount);

    return blendDisplayBinPowerStats (frequencyDependentStats,
                                      mainStats,
                                      assistBlend);
}

AnalyzerEngine::FrequencyDependentLiveAssistResult
AnalyzerEngine::applyFrequencyDependentLiveAssistForDisplayBin (
    const DisplayBinPowerStats& compositeStats,
    const DisplayBinPowerStats& transientReferenceStats,
    float centerFrequencyHz,
    bool hasCenterFrequency,
    float& storedAssistAmount,
    float frameAdvanceSeconds,
    float transientAssistReleaseSmoothing) const noexcept
{
    FrequencyDependentLiveAssistResult result;
    result.liveVisualStats = compositeStats;

    const auto getLowBassTailReleaseBlend = [this] (float frequencyHz) noexcept
    {
        if (! std::isfinite (frequencyHz) || frequencyHz <= 0.0f)
            return 0.0f;

        return juce::jlimit (
            0.0f,
            1.0f,
            1.0f - getFrequencyDependentMainBlendForFrequency (frequencyHz));
    };

    if (! hasCenterFrequency
        || centerFrequencyHz > frequencyDependentTransientAssistMaxHz)
    {
        storedAssistAmount = 0.0f;
        return result;
    }

    const auto frequencyDependentDisplayDb =
        displayBinPowerStatsToDb (compositeStats);

    const auto referenceDisplayDb =
        displayBinPowerStatsToDb (transientReferenceStats);

    const auto referenceAboveFrequencyDependentDb =
        referenceDisplayDb - frequencyDependentDisplayDb;

    const auto frequencyDependentAboveReferenceDb =
        frequencyDependentDisplayDb - referenceDisplayDb;

    auto desiredAssistAmount = 0.0f;

    if (referenceAboveFrequencyDependentDb >= frequencyDependentTransientAssistMinRiseDb)
    {
        desiredAssistAmount = frequencyDependentTransientAttackBlend;
    }
    else if (frequencyDependentAboveReferenceDb
            >= frequencyDependentTransientTailSuppressMinExcessDb)
    {
        desiredAssistAmount = frequencyDependentTransientTailSuppressBlend;
        result.lowBassTailReleaseBlend =
            getLowBassTailReleaseBlend (centerFrequencyHz);
    }

    desiredAssistAmount =
        juce::jlimit (0.0f,
                      1.0f,
                      std::isfinite (desiredAssistAmount)
                          ? desiredAssistAmount
                          : 0.0f);

    storedAssistAmount =
        std::isfinite (storedAssistAmount) ? storedAssistAmount : 0.0f;

    if (desiredAssistAmount > storedAssistAmount)
    {
        storedAssistAmount = desiredAssistAmount;
    }
    else if (frameAdvanceSeconds > 0.0f)
    {
        storedAssistAmount +=
            transientAssistReleaseSmoothing
                * (desiredAssistAmount - storedAssistAmount);
    }

    storedAssistAmount =
        juce::jlimit (0.0f,
                      1.0f,
                      std::isfinite (storedAssistAmount)
                          ? storedAssistAmount
                          : 0.0f);

    result.assistAmount = storedAssistAmount;

    if (centerFrequencyHz <= frequencyDependentMainOnlyMinHz
        && storedAssistAmount > 0.001f
        && frequencyDependentAboveReferenceDb > 0.0f)
    {
        result.lowBassTailReleaseBlend =
            juce::jmax (result.lowBassTailReleaseBlend,
                        getLowBassTailReleaseBlend (centerFrequencyHz));
    }

    result.liveVisualStats =
        applyFrequencyDependentTransientAssist (compositeStats,
                                                transientReferenceStats,
                                                centerFrequencyHz,
                                                result.assistAmount);

    return result;
}

int AnalyzerEngine::getFftHopSize() const noexcept
{
    return juce::jmax (1,
                       juce::jmin (currentFftSize / fftOverlapFactor,
                                   maximumFftHopSizeSamples));
}

void AnalyzerEngine::publishLatestFrame()
{
    {
        std::lock_guard<std::mutex> lock (latestSpectrumMutex);

        latestSpectrumDb = smoothedSpectrumDb;
        latestPeakHoldSpectrumDb = peakHoldSpectrumDb;
        latestFrameMinFrequencyHz = currentDisplayMinFrequencyHz;
        latestFrameMaxFrequencyHz = currentDisplayMaxFrequencyHz;
        latestNotePeaks = currentNotePeaks;

        for (size_t i = 0; i < latestRmsSpectrumDb.size(); ++i)
        {
            const auto rmsMagnitude = std::sqrt (rmsPowerSpectrum[i]);
            const auto energyMagnitude = std::sqrt (energyPowerSpectrum[i]);

            latestRmsSpectrumDb[i] =
                juce::jlimit (
                    -100.0f,
                    0.0f,
                    juce::Decibels::gainToDecibels (rmsMagnitude, -100.0f));

            latestEnergySpectrumDb[i] =
                juce::jlimit (
                    -100.0f,
                    0.0f,
                    juce::Decibels::gainToDecibels (energyMagnitude, -100.0f));
        }
    }

    hasFrame.store (true, std::memory_order_relaxed);
}

void AnalyzerEngine::updateFftSizeIfNeeded()
{
    const auto requestedOrder = requestedFftOrder.load (std::memory_order_relaxed);
    const auto requestedFrequencyDependentResolution =
        requestedFrequencyDependentResolutionEnabled.load (std::memory_order_relaxed);

    if (requestedOrder == currentFftOrder
        && requestedFrequencyDependentResolution == currentFrequencyDependentResolutionEnabled)
    {
        return;
    }

    currentFrequencyDependentResolutionEnabled = requestedFrequencyDependentResolution;
    configureFft (requestedOrder);
    reset();
}

void AnalyzerEngine::start()
{
    if (! isThreadRunning())
        startThread();
}

void AnalyzerEngine::stop()
{
    if (isThreadRunning())
    {
        signalThreadShouldExit();
        notify();
        stopThread (1000);
    }
}

bool AnalyzerEngine::copyLatestSpectrumDb (std::vector<float>& destination)
{
    if (! hasFrame.load (std::memory_order_relaxed))
        return false;

    std::lock_guard<std::mutex> lock (latestSpectrumMutex);
    destination = latestSpectrumDb;

    return true;
}

bool AnalyzerEngine::copyLatestPeakHoldSpectrumDb (std::vector<float>& destination)
{
    if (! hasFrame.load (std::memory_order_relaxed))
        return false;

    std::lock_guard<std::mutex> lock (latestSpectrumMutex);
    destination = latestPeakHoldSpectrumDb;

    return true;
}

bool AnalyzerEngine::copyLatestRmsSpectrumDb (std::vector<float>& destination)
{
    if (! hasFrame.load (std::memory_order_relaxed))
        return false;

    std::lock_guard<std::mutex> lock (latestSpectrumMutex);
    destination = latestRmsSpectrumDb;

    return true;
}

bool AnalyzerEngine::copyLatestFrame (Frame& destination)
{
    if (! hasFrame.load (std::memory_order_relaxed))
        return false;

    std::lock_guard<std::mutex> lock (latestSpectrumMutex);

    destination.liveDb = latestSpectrumDb;
    destination.peakHoldDb = latestPeakHoldSpectrumDb;
    destination.rmsDb = latestRmsSpectrumDb;
    destination.energyDb = latestEnergySpectrumDb;
    destination.notePeaks = latestNotePeaks;
    destination.dataMinFrequencyHz = latestFrameMinFrequencyHz;
    destination.dataMaxFrequencyHz = latestFrameMaxFrequencyHz;

    return true;
}

void AnalyzerEngine::run()
{
    while (! threadShouldExit())
    {
        if (sourceFifo == nullptr)
        {
            handleClearPeakHoldRequest();
            handleClearEnergyRequest();
            wait (20);
            continue;
        }

        handleClearPeakHoldRequest();
        handleClearEnergyRequest();
        updateFftSizeIfNeeded();

        const auto requiredSamples =
            overlapBufferPrimed ? getFftHopSize() : currentFftSize;

        if (sourceFifo->getNumAvailableForReading() >= requiredSamples)
        {
            processOneFftBlock();
            continue;
        }

        wait (5);
    }
}

void AnalyzerEngine::processOneFftBlock()
{
    if (sourceFifo == nullptr || forwardFFT == nullptr || window == nullptr)
        return;

    const auto fftSizeForBlock = currentFftSize;
    const auto hopSize = getFftHopSize();

    if (fftSizeForBlock <= 0 || hopSize <= 0)
        return;

    if (currentSampleRate <= 0.0)
        return;

    if (timeDomainBlock.size() < static_cast<size_t> (fftSizeForBlock)
        || hopBuffer.size() < static_cast<size_t> (hopSize)
        || fftData.size() < static_cast<size_t> (fftSizeForBlock * 2))
    {
        return;
    }

    const float* newSamples = nullptr;
    auto numNewSamples = 0;

    if (! overlapBufferPrimed)
    {
        const auto numRead = sourceFifo->pop (timeDomainBlock.data(), fftSizeForBlock);

        if (numRead != fftSizeForBlock)
            return;

        newSamples = timeDomainBlock.data();
        numNewSamples = fftSizeForBlock;
        overlapBufferPrimed = true;
    }
    else
    {
        const auto numRead = sourceFifo->pop (hopBuffer.data(), hopSize);

        if (numRead != hopSize)
            return;

        newSamples = hopBuffer.data();
        numNewSamples = hopSize;

        std::copy (timeDomainBlock.begin() + hopSize,
                   timeDomainBlock.begin() + fftSizeForBlock,
                   timeDomainBlock.begin());

        std::copy (hopBuffer.begin(),
                   hopBuffer.begin() + hopSize,
                   timeDomainBlock.begin() + (fftSizeForBlock - hopSize));
    }

    if (currentFrequencyDependentResolutionEnabled)
    {
        const auto frequencyDependentSourceDescriptors =
            getFrequencyDependentSourceDescriptors();

        for (const auto& descriptor : frequencyDependentSourceDescriptors)
        {
            if (descriptor.source != nullptr)
            {
                appendSamplesToFrequencyDependentFftSource (*descriptor.source,
                                                            newSamples,
                                                            numNewSamples,
                                                            descriptor.fftSize);
            }
        }

        for (const auto& descriptor : frequencyDependentSourceDescriptors)
        {
            if (descriptor.source != nullptr)
            {
                processFrequencyDependentFftSourceIfReady (*descriptor.source,
                                                           descriptor.fftSize);
            }
        }
    }

    std::fill (fftData.begin(), fftData.end(), 0.0f);
    std::copy (timeDomainBlock.begin(), timeDomainBlock.end(), fftData.begin());

    window->multiplyWithWindowingTable (
        fftData.data(),
        static_cast<size_t> (fftSizeForBlock));

    forwardFFT->performFrequencyOnlyForwardTransform (fftData.data());

    const auto sampleRate = static_cast<float> (currentSampleRate);

    const auto frameAdvanceSeconds =
        static_cast<float> (hopSize) / sampleRate;

    const auto currentPeakHoldDecayDbPerSecond =
        peakHoldDecayDbPerSecond.load (std::memory_order_relaxed);

    extractInstantaneousNotePeaksFromFftData (fftSizeForBlock);
    updateTrackedNotePeaks (frameAdvanceSeconds, currentPeakHoldDecayDbPerSecond);
    publishStableNotePeaks();

    updateDisplayBinFftRangesIfNeeded();

    if (currentFrequencyDependentResolutionEnabled)
    {
        for (const auto& descriptor : getFrequencyDependentSourceDescriptors())
        {
            if (descriptor.source != nullptr)
            {
                updateFrequencyDependentSourceBinFftRangesIfNeeded (*descriptor.source,
                                                                    descriptor.fftSize);
            }
        }
    }

    if (displayBinFftRanges.size() != static_cast<size_t> (displayBinCount))
        return;

    if (energyFrameMeanPower.size() != static_cast<size_t> (displayBinCount)
        || energyPowerSpectrum.size() != static_cast<size_t> (displayBinCount))
    {
        return;
    }

    if (frequencyDependentTransientAssistAmounts.size()
        != static_cast<size_t> (displayBinCount))
    {
        return;
    }

    const auto shouldWarmStartDisplayAccumulation =
        displayAccumulationWarmStartRequested;

    displayAccumulationWarmStartRequested = false;

    if (shouldWarmStartDisplayAccumulation)
    {
        std::fill (frequencyDependentTransientAssistAmounts.begin(),
                   frequencyDependentTransientAssistAmounts.end(),
                   0.0f);
    }

    const auto decayPerFrame =
        currentPeakHoldDecayDbPerSecond * frameAdvanceSeconds;

    const auto currentRmsTimeSeconds =
        rmsTimeSeconds.load (std::memory_order_relaxed);

    const auto rmsAlpha =
        smoothingCoefficientForTimeConstant (frameAdvanceSeconds, currentRmsTimeSeconds);

    const auto liveAttackSmoothing =
        smoothingCoefficientForTimeConstant (frameAdvanceSeconds, liveAttackTimeSeconds);

    const auto liveReleaseSmoothing =
        smoothingCoefficientForTimeConstant (frameAdvanceSeconds, liveReleaseTimeSeconds);

    const auto lowBassTailReleaseSmoothing =
        smoothingCoefficientForTimeConstant (
            frameAdvanceSeconds,
            frequencyDependentLowBassTailReleaseTimeSeconds);

    const auto veryHighReleaseSmoothing =
        smoothingCoefficientForTimeConstant (
            frameAdvanceSeconds,
            frequencyDependentVeryHighReleaseTimeSeconds);

    const auto transientAssistReleaseSmoothing =
        smoothingCoefficientForTimeConstant (
            frameAdvanceSeconds,
            frequencyDependentTransientAssistReleaseSeconds);

    auto energyFramePeakPower = 0.0f;
    const auto canUseFrequencyDependentBassPath =
        currentFrequencyDependentResolutionEnabled
        && canUseFrequencyDependentSource (frequencyDependentBassPath,
                                           frequencyDependentBassFftSize);

    const auto canUseFrequencyDependentMidBassPath =
        currentFrequencyDependentResolutionEnabled
        && canUseFrequencyDependentSource (frequencyDependentMidBassPath,
                                           frequencyDependentMidBassFftSize);

    const auto canUseFrequencyDependentHighPath =
        currentFrequencyDependentResolutionEnabled
        && canUseFrequencyDependentSource (frequencyDependentHighPath,
                                           frequencyDependentHighFftSize);

    const auto canUseFrequencyDependentVeryHighPath =
        currentFrequencyDependentResolutionEnabled
        && canUseFrequencyDependentSource (frequencyDependentVeryHighPath,
                                           frequencyDependentVeryHighFftSize);

    for (int i = 0; i < displayBinCount; ++i)
    {
        const auto index = static_cast<size_t> (i);
        const auto binStats = getFrequencyDependentBinStatsForDisplayBin (
            i,
            fftSizeForBlock,
            canUseFrequencyDependentBassPath,
            canUseFrequencyDependentMidBassPath,
            canUseFrequencyDependentHighPath,
            canUseFrequencyDependentVeryHighPath);

        const auto& transientReferenceBinPowerStats =
            binStats.hasTransientReferenceStats
                ? binStats.transientReferenceStats
                : binStats.mainStats;
        const auto& binPowerStats = binStats.compositeStats;

        auto liveVisualBinPowerStats = binPowerStats;
        const auto peakHoldVisualBinPowerStats = binPowerStats;
        auto lowBassTailReleaseBlend = 0.0f;
        auto veryHighReleaseBlend = 0.0f;

        if (currentFrequencyDependentResolutionEnabled)
        {
            auto& storedAssistAmount = frequencyDependentTransientAssistAmounts[index];

            const auto assistResult =
                applyFrequencyDependentLiveAssistForDisplayBin (
                    binPowerStats,
                    transientReferenceBinPowerStats,
                    binStats.centerFrequencyHz,
                    binStats.hasCenterFrequency,
                    storedAssistAmount,
                    frameAdvanceSeconds,
                    transientAssistReleaseSmoothing);

            liveVisualBinPowerStats = assistResult.liveVisualStats;
            lowBassTailReleaseBlend = assistResult.lowBassTailReleaseBlend;

            if (canUseFrequencyDependentVeryHighPath
                && binStats.hasCenterFrequency)
            {
                veryHighReleaseBlend =
                    getFrequencyDependentVeryHighBlendForFrequency (
                        binStats.centerFrequencyHz);

                if (veryHighReleaseBlend <= 0.0f)
                    veryHighReleaseBlend = 0.0f;
            }
        }

        const auto liveTargetDb = displayBinPowerStatsToDb (liveVisualBinPowerStats);
        const auto peakHoldTargetDb =
            displayBinPowerStatsToDb (peakHoldVisualBinPowerStats);

        energyFrameMeanPower[index] = binPowerStats.meanPower;
        energyFramePeakPower = juce::jmax (energyFramePeakPower, binPowerStats.meanPower);

        rawSpectrumDb[index] = liveTargetDb;

        if (shouldWarmStartDisplayAccumulation)
        {
            smoothedSpectrumDb[index] = liveTargetDb;
            peakHoldSpectrumDb[index] = peakHoldTargetDb;
            rmsPowerSpectrum[index] = binPowerStats.meanPower;
            continue;
        }

        const auto previousDb = smoothedSpectrumDb[index];

        const auto clampedLowBassTailReleaseBlend =
            juce::jlimit (0.0f, 1.0f, lowBassTailReleaseBlend);

        const auto clampedVeryHighReleaseBlend =
            juce::jlimit (0.0f, 1.0f, veryHighReleaseBlend);

        auto releaseSmoothing =
            liveReleaseSmoothing
            + clampedLowBassTailReleaseBlend
                * (lowBassTailReleaseSmoothing - liveReleaseSmoothing);

        releaseSmoothing +=
            clampedVeryHighReleaseBlend
            * (veryHighReleaseSmoothing - releaseSmoothing);

        const auto smoothing =
            liveTargetDb > previousDb ? liveAttackSmoothing : releaseSmoothing;

        smoothedSpectrumDb[index] =
            previousDb + smoothing * (liveTargetDb - previousDb);

        if (peakHoldTargetDb > peakHoldSpectrumDb[index])
            peakHoldSpectrumDb[index] = peakHoldTargetDb;
        else
            peakHoldSpectrumDb[index] =
                juce::jmax (-100.0f, peakHoldSpectrumDb[index] - decayPerFrame);

        rmsPowerSpectrum[index] =
            rmsPowerSpectrum[index]
            + rmsAlpha * (binPowerStats.meanPower - rmsPowerSpectrum[index]);
    }

    const auto activityThresholdGain =
        juce::Decibels::decibelsToGain (energyActivityThresholdDb);

    const auto activityThresholdPower = activityThresholdGain * activityThresholdGain;
    const auto frameIsActiveForEnergy = energyFramePeakPower > activityThresholdPower;

    if (shouldWarmStartDisplayAccumulation)
    {
        std::fill (energyPowerSpectrum.begin(), energyPowerSpectrum.end(), 0.0f);
        energyAccumulatedActiveSeconds = 0.0f;
    }

    if (frameAdvanceSeconds > 0.0f && frameIsActiveForEnergy)
    {
        const auto previousAccumulatedSeconds = energyAccumulatedActiveSeconds;
        const auto nextAccumulatedSeconds =
            juce::jmin (energyAveragingWindowSeconds,
                        previousAccumulatedSeconds + frameAdvanceSeconds);

        const auto energyWeight =
            previousAccumulatedSeconds <= 0.0f
                ? 1.0f
                : juce::jlimit (0.0f,
                                1.0f,
                                frameAdvanceSeconds
                                    / juce::jmax (frameAdvanceSeconds,
                                                  nextAccumulatedSeconds));

        for (int i = 0; i < displayBinCount; ++i)
        {
            const auto index = static_cast<size_t> (i);

            energyPowerSpectrum[index] =
                energyPowerSpectrum[index]
                + energyWeight * (energyFrameMeanPower[index] - energyPowerSpectrum[index]);
        }

        energyAccumulatedActiveSeconds = nextAccumulatedSeconds;
    }

    secondsSinceLastFramePublish += frameAdvanceSeconds;

    const auto publishIntervalSeconds =
        1.0f / latestFramePublishRateHz;

    const auto shouldPublishFrame =
        ! hasFrame.load (std::memory_order_relaxed)
        || secondsSinceLastFramePublish >= publishIntervalSeconds;

    if (shouldPublishFrame)
    {
        publishLatestFrame();
        secondsSinceLastFramePublish = 0.0f;
    }
}

int AnalyzerEngine::frequencyToMidiNote (float frequencyHz) const noexcept
{
    if (frequencyHz <= 0.0f)
        return -1;

    const auto midi =
        juce::roundToInt (69.0f + 12.0f * std::log2 (frequencyHz / 440.0f));

    return juce::jlimit (0, 127, midi);
}

int AnalyzerEngine::midiNoteToPitchClass (int midiNote) const noexcept
{
    if (midiNote < 0)
        return -1;

    return midiNote % 12;
}

void AnalyzerEngine::extractInstantaneousNotePeaksFromFftData (int fftSizeForBlock)
{
    instantaneousNotePeaks.clear();

    if (fftSizeForBlock <= 0 || currentSampleRate <= 0.0)
        return;

    const auto maxAvailableBin = (fftSizeForBlock / 2) - 1;

    if (maxAvailableBin < 3)
        return;

    const auto sampleRate = static_cast<float> (currentSampleRate);
    const auto maxFrequency =
        juce::jmin (maxNotePeakFrequencyHz, sampleRate * 0.5f);

    const auto minBin =
        juce::jlimit (1,
                      maxAvailableBin,
                      static_cast<int> (std::ceil (minNotePeakFrequencyHz
                                                   * static_cast<float> (fftSizeForBlock)
                                                   / sampleRate)));

    const auto maxBin =
        juce::jlimit (1,
                      maxAvailableBin,
                      static_cast<int> (std::floor (maxFrequency
                                                    * static_cast<float> (fftSizeForBlock)
                                                    / sampleRate)));

    if (maxBin - minBin < 2)
        return;

    const auto numBins = static_cast<size_t> (maxBin - minBin + 1);

    if (notePeakBinDecibels.size() < numBins)
        notePeakBinDecibels.resize (numBins, notePeakMinAbsoluteDb);

    auto maxDb = std::numeric_limits<float>::lowest();

    for (auto bin = minBin; bin <= maxBin; ++bin)
    {
        const auto magnitude =
            (fftData[static_cast<size_t> (bin)] / static_cast<float> (fftSizeForBlock)) * 2.0f;

        const auto db = juce::Decibels::gainToDecibels (magnitude, notePeakMinAbsoluteDb);

        notePeakBinDecibels[static_cast<size_t> (bin - minBin)] = db;
        maxDb = std::max (maxDb, db);
    }

    if (maxDb <= notePeakMinAbsoluteDb)
        return;

    const auto thresholdDb =
        std::max (notePeakMinAbsoluteDb, maxDb - notePeakRelativeThresholdDb);

    for (auto bin = minBin + 1; bin <= maxBin - 1; ++bin)
    {
        const auto binIndex = static_cast<size_t> (bin - minBin);
        const auto previousDb = notePeakBinDecibels[binIndex - 1];
        const auto currentDb = notePeakBinDecibels[binIndex];
        const auto nextDb = notePeakBinDecibels[binIndex + 1];

        if (currentDb < thresholdDb)
            continue;

        if (! (currentDb >= previousDb
               && currentDb >= nextDb
               && (currentDb > previousDb || currentDb > nextDb)))
        {
            continue;
        }

        if (currentDb - std::max (previousDb, nextDb) < notePeakMinProminenceDb)
            continue;

        const auto denominator = previousDb - 2.0f * currentDb + nextDb;
        auto offset = 0.0f;

        if (std::abs (denominator) > 0.000001f)
            offset = 0.5f * (previousDb - nextDb) / denominator;

        offset = juce::jlimit (-0.5f, 0.5f, offset);

        const auto refinedBin = static_cast<float> (bin) + offset;
        const auto frequencyHz =
            refinedBin * sampleRate / static_cast<float> (fftSizeForBlock);

        if (frequencyHz < minNotePeakFrequencyHz || frequencyHz > maxNotePeakFrequencyHz)
            continue;

        const auto midiNote = frequencyToMidiNote (frequencyHz);
        const auto pitchClass = midiNoteToPitchClass (midiNote);

        if (midiNote < 0 || pitchClass < 0)
            continue;

        const auto refinedDb =
            currentDb - 0.25f * (previousDb - nextDb) * offset;

        instantaneousNotePeaks.push_back ({
            frequencyHz,
            refinedDb,
            midiNote,
            pitchClass
        });
    }

    std::sort (instantaneousNotePeaks.begin(),
               instantaneousNotePeaks.end(),
               [] (const auto& first, const auto& second)
               {
                   return first.decibels > second.decibels;
               });

    std::array<bool, 128> usedMidiNotes {};
    auto writeIndex = static_cast<size_t> (0);

    for (const auto& candidate : instantaneousNotePeaks)
    {
        if (candidate.midiNote < 0 || candidate.midiNote >= static_cast<int> (usedMidiNotes.size()))
            continue;

        const auto midiIndex = static_cast<size_t> (candidate.midiNote);

        if (usedMidiNotes[midiIndex])
            continue;

        usedMidiNotes[midiIndex] = true;
        instantaneousNotePeaks[writeIndex] = candidate;
        ++writeIndex;

        if (writeIndex >= static_cast<size_t> (maxInstantaneousNotePeaks))
            break;
    }

    instantaneousNotePeaks.resize (writeIndex);

    std::sort (instantaneousNotePeaks.begin(),
               instantaneousNotePeaks.end(),
               [] (const auto& first, const auto& second)
               {
                   return first.frequencyHz < second.frequencyHz;
               });
}

float AnalyzerEngine::smoothingCoefficientForTimeConstant (float frameDurationSeconds,
                                                           float timeConstantSeconds) noexcept
{
    if (frameDurationSeconds <= 0.0f)
        return 0.0f;

    if (timeConstantSeconds <= 0.0f)
        return 1.0f;

    return 1.0f - std::exp (-frameDurationSeconds / timeConstantSeconds);
}

void AnalyzerEngine::updateTrackedNotePeaks (float frameDurationSeconds,
                                             float peakHoldDecayDbPerSecondForFrame)
{
    const auto confidenceAttack =
        smoothingCoefficientForTimeConstant (frameDurationSeconds,
                                             notePeakPublishAttackSeconds);

    const auto confidenceRelease =
        smoothingCoefficientForTimeConstant (frameDurationSeconds,
                                             notePeakReleaseSeconds);

    const auto frequencySmoothing =
        smoothingCoefficientForTimeConstant (frameDurationSeconds,
                                             notePeakFrequencySmoothingSeconds);

    const auto dbAttack =
        smoothingCoefficientForTimeConstant (frameDurationSeconds,
                                             notePeakDbAttackSeconds);

    const auto dbRelease =
        smoothingCoefficientForTimeConstant (frameDurationSeconds,
                                             notePeakDbReleaseSeconds);

    const auto heldDecayDb =
        peakHoldDecayDbPerSecondForFrame * frameDurationSeconds;

    for (auto& trackedPeak : trackedNotePeaks)
    {
        ++trackedPeak.framesSinceSeen;
        trackedPeak.secondsSinceSeen += frameDurationSeconds;

        trackedPeak.heldDecibels =
            juce::jmax (notePeakMinAbsoluteDb,
                        trackedPeak.heldDecibels - heldDecayDb);
    }

    for (const auto& candidate : instantaneousNotePeaks)
    {
        auto matchingPeak =
            std::find_if (trackedNotePeaks.begin(),
                          trackedNotePeaks.end(),
                          [&candidate] (const auto& trackedPeak)
                          {
                              return trackedPeak.midiNote == candidate.midiNote;
                          });

        if (matchingPeak != trackedNotePeaks.end())
        {
            matchingPeak->framesSinceSeen = 0;
            matchingPeak->secondsSinceSeen = 0.0f;
            matchingPeak->hitCount += 1;

            matchingPeak->confidence +=
                confidenceAttack * (1.0f - matchingPeak->confidence);

            matchingPeak->frequencyHz +=
                frequencySmoothing * (candidate.frequencyHz - matchingPeak->frequencyHz);

            const auto dbSmoothing =
                candidate.decibels > matchingPeak->decibels
                    ? dbAttack
                    : dbRelease;

            matchingPeak->decibels +=
                dbSmoothing * (candidate.decibels - matchingPeak->decibels);

            matchingPeak->heldDecibels =
                juce::jmax (matchingPeak->heldDecibels, candidate.decibels);

            matchingPeak->pitchClass = candidate.pitchClass;

            if (matchingPeak->hitCount >= notePeakMinimumHitCount
                && matchingPeak->confidence >= notePeakPublishConfidence)
            {
                matchingPeak->hasBecomeStable = true;
            }

            continue;
        }

        TrackedNotePeak newPeak;
        newPeak.frequencyHz = candidate.frequencyHz;
        newPeak.decibels = candidate.decibels;
        newPeak.heldDecibels = candidate.decibels;
        newPeak.midiNote = candidate.midiNote;
        newPeak.pitchClass = candidate.pitchClass;
        newPeak.hitCount = 1;
        newPeak.framesSinceSeen = 0;
        newPeak.secondsSinceSeen = 0.0f;
        newPeak.confidence = confidenceAttack;
        newPeak.hasBecomeStable =
            newPeak.hitCount >= notePeakMinimumHitCount
            && newPeak.confidence >= notePeakPublishConfidence;

        trackedNotePeaks.push_back (newPeak);
    }

    for (auto& trackedPeak : trackedNotePeaks)
    {
        if (trackedPeak.framesSinceSeen == 0)
            continue;

        trackedPeak.confidence +=
            confidenceRelease * (0.0f - trackedPeak.confidence);
    }

    trackedNotePeaks.erase (
        std::remove_if (trackedNotePeaks.begin(),
                        trackedNotePeaks.end(),
                        [] (const auto& trackedPeak)
                        {
                            const auto unstablePeakIsGone =
                                ! trackedPeak.hasBecomeStable
                                && (trackedPeak.secondsSinceSeen > notePeakReleaseSeconds
                                    || trackedPeak.confidence <= notePeakRemoveConfidence);

                            const auto stablePeakHasDecayed =
                                trackedPeak.hasBecomeStable
                                && trackedPeak.heldDecibels <= notePeakMinAbsoluteDb + 0.001f
                                && trackedPeak.confidence <= notePeakRemoveConfidence;

                            return unstablePeakIsGone || stablePeakHasDecayed;
                        }),
        trackedNotePeaks.end());
}

void AnalyzerEngine::publishStableNotePeaks()
{
    currentNotePeaks.clear();

    for (const auto& trackedPeak : trackedNotePeaks)
    {
        if (! trackedPeak.hasBecomeStable
            || trackedPeak.heldDecibels <= notePeakMinAbsoluteDb + 0.001f
            || trackedPeak.frequencyHz < minNotePeakFrequencyHz
            || trackedPeak.frequencyHz > maxNotePeakFrequencyHz
            || trackedPeak.midiNote < 0
            || trackedPeak.pitchClass < 0)
        {
            continue;
        }

        currentNotePeaks.push_back ({
            trackedPeak.frequencyHz,
            trackedPeak.heldDecibels,
            trackedPeak.midiNote,
            trackedPeak.pitchClass
        });
    }

    std::sort (currentNotePeaks.begin(),
               currentNotePeaks.end(),
               [] (const auto& first, const auto& second)
               {
                   return first.decibels > second.decibels;
               });

    if (currentNotePeaks.size() > static_cast<size_t> (maxPublishedNotePeaks))
        currentNotePeaks.resize (static_cast<size_t> (maxPublishedNotePeaks));

    std::sort (currentNotePeaks.begin(),
               currentNotePeaks.end(),
               [] (const auto& first, const auto& second)
               {
                   return first.frequencyHz < second.frequencyHz;
               });
}
