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
        if (!std::isfinite (frequencyHz))
            return 0.0f;

        if (std::isfinite (upperHz) && upperHz > 0.0f && frequencyHz >= upperHz)
            return 1.0f;

        if (frequencyHz <= 0.0f
            || !std::isfinite (lowerHz)
            || !std::isfinite (upperHz)
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

    float safePolicyRatio (int numerator, int denominator) noexcept
    {
        if (denominator <= 0 || numerator <= 0)
            return 0.0f;

        return juce::jlimit (
            0.0f,
            1.0f,
            static_cast<float> (numerator) / static_cast<float> (denominator));
    }

    float clampAnalyzerDisplayDb (float value) noexcept
    {
        return juce::jlimit (
            -100.0f,
            0.0f,
            std::isfinite (value) ? value : -100.0f);
    }

    float smoothLinearBlend (float value, float lower, float upper) noexcept
    {
        if (!std::isfinite (value)
            || !std::isfinite (lower)
            || !std::isfinite (upper)
            || upper <= lower)
        {
            return 0.0f;
        }

        if (value <= lower)
            return 0.0f;

        if (value >= upper)
            return 1.0f;

        const auto t = juce::jlimit (0.0f, 1.0f, (value - lower) / (upper - lower));

        return t * t * (3.0f - 2.0f * t);
    }

    float blendFrequencyDependentLiveReleaseSmoothing (
        float liveReleaseSmoothing,
        float lowBassTailReleaseSmoothing,
        float lowBassTailReleaseBlend,
        float veryHighReleaseSmoothing,
        float veryHighReleaseBlend) noexcept
    {
        const auto sanitizeSmoothing = [] (float value, float fallback) noexcept {
            return std::isfinite (value) ? value : fallback;
        };

        const auto baseReleaseSmoothing =
            sanitizeSmoothing (liveReleaseSmoothing, 0.0f);

        const auto safeLowBassTailReleaseSmoothing =
            sanitizeSmoothing (lowBassTailReleaseSmoothing, baseReleaseSmoothing);

        const auto safeVeryHighReleaseSmoothing =
            sanitizeSmoothing (veryHighReleaseSmoothing, baseReleaseSmoothing);

        const auto safeLowBassTailReleaseBlend =
            std::isfinite (lowBassTailReleaseBlend) ? lowBassTailReleaseBlend : 0.0f;

        const auto safeVeryHighReleaseBlend =
            std::isfinite (veryHighReleaseBlend) ? veryHighReleaseBlend : 0.0f;

        const auto clampedLowBassTailReleaseBlend =
            juce::jlimit (0.0f, 1.0f, safeLowBassTailReleaseBlend);

        const auto clampedVeryHighReleaseBlend =
            juce::jlimit (0.0f, 1.0f, safeVeryHighReleaseBlend);

        auto releaseSmoothing =
            baseReleaseSmoothing
            + clampedLowBassTailReleaseBlend
                  * (safeLowBassTailReleaseSmoothing - baseReleaseSmoothing);

        releaseSmoothing +=
            clampedVeryHighReleaseBlend
            * (safeVeryHighReleaseSmoothing - releaseSmoothing);

        return releaseSmoothing;
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
    frequencyDependentTunedLowBandAlignmentAmounts.resize (displayBinCount, 0.0f);
    frequencyDependentTunedLowBandPreviousReferenceDb.resize (displayBinCount, -100.0f);
    frequencyDependentBinPolicySnapshots.resize (static_cast<size_t> (displayBinCount));
    vqtLikeFilterBands.resize (static_cast<size_t> (vqtLikeAnalysisBandCount));
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

    currentFrequencyDependentTunedResolutionEnabled =
        requestedFrequencyDependentTunedResolutionEnabled.load (std::memory_order_relaxed);

    currentVqtLikeFilterbankEnabled =
        requestedVqtLikeFilterbankEnabled.load (std::memory_order_relaxed);

    configureFft (requestedFftOrder.load (std::memory_order_relaxed));
    reset();

    if constexpr (enableVqtLikeInternalValidation)
    {
        static bool hasRunVqtValidation = false;

        if (!hasRunVqtValidation)
        {
            hasRunVqtValidation = true;

            updateDisplayBinFftRangesIfNeeded();
            runVqtLikeInternalCalibrationValidation();

            juce::StringArray lines;

            lines.add ("================ VQT-like internal validation ================");
            lines.add ("Result count: " + juce::String (static_cast<int> (lastVqtLikeValidationResults.size())));

            for (const auto& result : lastVqtLikeValidationResults)
            {
                lines.add ("------------------------------------------------------------");
                lines.add ("targetFrequencyHz: " + juce::String (result.targetFrequencyHz));
                lines.add ("measuredPeakFrequencyHz: " + juce::String (result.measuredPeakFrequencyHz));
                lines.add ("expectedDb: " + juce::String (result.expectedDb));
                lines.add ("measuredMetricDb: " + juce::String (result.measuredMetricDb));
                lines.add ("measuredLiveDb: " + juce::String (result.measuredLiveDb));
                lines.add ("metricErrorDb: " + juce::String (result.metricErrorDb));
                lines.add ("liveErrorDb: " + juce::String (result.liveErrorDb));
                lines.add ("peakWidthBinsAboveMinus3Db: " + juce::String (result.peakWidthBinsAboveMinus3Db));
                lines.add ("peakWidthHzAboveMinus3Db: " + juce::String (result.peakWidthHzAboveMinus3Db));
                lines.add ("averageAbsMetricReferenceErrorDb: " + juce::String (result.averageAbsMetricReferenceErrorDb));
                lines.add ("maxAbsMetricReferenceErrorDb: " + juce::String (result.maxAbsMetricReferenceErrorDb));
                lines.add ("isValid: " + juce::String (static_cast<int> (result.isValid)));
            }

            lines.add ("================ end VQT-like validation =====================");

            const auto outputFile =
                juce::File::getSpecialLocation (juce::File::userDesktopDirectory)
                    .getChildFile ("VQT_like_validation_results.txt");

            const auto writeSucceeded =
                outputFile.replaceWithText (lines.joinIntoString ("\n"));

            juce::Logger::writeToLog ("VQT-like validation output path: "
                                      + outputFile.getFullPathName());

            juce::Logger::writeToLog ("VQT-like validation file write success: "
                                      + juce::String (static_cast<int> (writeSucceeded)));
        }
    }
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

    resetVqtLikeFilterbankState();
    vqtLikeFilterbankNeedsReset = true;

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

    if (frequencyDependentTunedLowBandAlignmentAmounts.size()
        != static_cast<size_t> (displayBinCount))
    {
        frequencyDependentTunedLowBandAlignmentAmounts.assign (
            static_cast<size_t> (displayBinCount),
            0.0f);
    }
    else
    {
        std::fill (frequencyDependentTunedLowBandAlignmentAmounts.begin(),
            frequencyDependentTunedLowBandAlignmentAmounts.end(),
            0.0f);
    }

    if (frequencyDependentTunedLowBandPreviousReferenceDb.size()
        != static_cast<size_t> (displayBinCount))
    {
        frequencyDependentTunedLowBandPreviousReferenceDb.assign (
            static_cast<size_t> (displayBinCount),
            -100.0f);
    }
    else
    {
        std::fill (frequencyDependentTunedLowBandPreviousReferenceDb.begin(),
            frequencyDependentTunedLowBandPreviousReferenceDb.end(),
            -100.0f);
    }

    if (frequencyDependentBinPolicySnapshots.size() != static_cast<size_t> (displayBinCount))
    {
        frequencyDependentBinPolicySnapshots.resize (
            static_cast<size_t> (displayBinCount));
    }

    std::fill (frequencyDependentBinPolicySnapshots.begin(),
        frequencyDependentBinPolicySnapshots.end(),
        FrequencyDependentBinPolicySnapshot {});
    resetFrequencyDependentPolicyFrameSummary();
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
    if (!clearPeakHoldRequested.exchange (false, std::memory_order_relaxed))
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
    if (!clearEnergyRequested.exchange (false, std::memory_order_relaxed))
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

void AnalyzerEngine::setFrequencyDependentTunedResolutionEnabled (bool shouldUseFrequencyDependentTunedResolution) noexcept { requestedFrequencyDependentTunedResolutionEnabled.store (shouldUseFrequencyDependentTunedResolution, std::memory_order_relaxed); }

void AnalyzerEngine::setVqtLikeFilterbankEnabled (bool shouldUseVqtLikeFilterbank) noexcept
{
    requestedVqtLikeFilterbankEnabled.store (
        shouldUseVqtLikeFilterbank,
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
    return { { { FrequencyDependentSourceRole::bass,
                   &frequencyDependentBassPath,
                   frequencyDependentBassFftOrder,
                   frequencyDependentBassFftSize },
        { FrequencyDependentSourceRole::midBass,
            &frequencyDependentMidBassPath,
            frequencyDependentMidBassFftOrder,
            frequencyDependentMidBassFftSize },
        { FrequencyDependentSourceRole::high,
            &frequencyDependentHighPath,
            frequencyDependentHighFftOrder,
            frequencyDependentHighFftSize },
        { FrequencyDependentSourceRole::veryHigh,
            &frequencyDependentVeryHighPath,
            frequencyDependentVeryHighFftOrder,
            frequencyDependentVeryHighFftSize } } };
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

AnalyzerEngine::FrequencyDependentSourceAvailability
    AnalyzerEngine::getFrequencyDependentSourceAvailability() const noexcept
{
    FrequencyDependentSourceAvailability result;

    if (!currentFrequencyDependentResolutionEnabled)
        return result;

    result.canUseBass =
        canUseFrequencyDependentSource (frequencyDependentBassPath,
            frequencyDependentBassFftSize);

    result.canUseMidBass =
        canUseFrequencyDependentSource (frequencyDependentMidBassPath,
            frequencyDependentMidBassFftSize);

    result.canUseHigh =
        canUseFrequencyDependentSource (frequencyDependentHighPath,
            frequencyDependentHighFftSize);

    result.canUseVeryHigh =
        canUseFrequencyDependentSource (frequencyDependentVeryHighPath,
            frequencyDependentVeryHighFftSize);

    return result;
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
                {
                    descriptor.source->rangeSampleRate = 0.0f;
                    descriptor.source->rangeMinFrequencyHz = 0.0f;
                    descriptor.source->rangeMaxFrequencyHz = 0.0f;
                }
            }
        }

        if (currentVqtLikeFilterbankEnabled)
        {
            vqtLikeFilterbankSampleRate = 0.0f;
            vqtLikeFilterbankMinFrequencyHz = 0.0f;
            vqtLikeFilterbankMaxFrequencyHz = 0.0f;
            vqtLikeFilterbankNeedsReset = true;
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

AnalyzerEngine::FrequencyDependentBlendWeights
    AnalyzerEngine::getFrequencyDependentBlendWeightsForFrequency (
        float frequencyHz) const noexcept
{
    FrequencyDependentBlendWeights result;

    if (!std::isfinite (frequencyHz) || frequencyHz <= 0.0f)
        return result;

    result.midBassBlend =
        getFrequencyDependentMidBassBlendForFrequency (frequencyHz);

    result.mainBlend =
        getFrequencyDependentMainBlendForFrequency (frequencyHz);

    result.highBlend =
        getFrequencyDependentHighBlendForFrequency (frequencyHz);

    result.veryHighBlend =
        getFrequencyDependentVeryHighBlendForFrequency (frequencyHz);

    result.lowBassTailReleaseBlend =
        juce::jlimit (0.0f, 1.0f, 1.0f - result.mainBlend);

    return result;
}

AnalyzerEngine::FrequencyDependentLowCompositeResult
    AnalyzerEngine::getFrequencyDependentLowCompositeForDisplayBin (
        size_t displayBinIndex,
        float centerFrequencyHz,
        const DisplayBinPowerStats& mainStats,
        const FrequencyDependentSourceAvailability& sourceAvailability,
        const FrequencyDependentBlendWeights& blendWeights) const
{
    FrequencyDependentLowCompositeResult result;

    if (sourceAvailability.canUseBass
        && frequencyDependentBassPath.displayBinFftRanges.size() > displayBinIndex)
    {
        result.lowCompositeStats =
            getFrequencyDependentSourceStatsForDisplayBin (
                frequencyDependentBassPath,
                frequencyDependentBassFftSize,
                displayBinIndex);

        result.hasLowCompositeStats =
            result.lowCompositeStats.numBinsUsed > 0;
        result.usedBassComposite = result.hasLowCompositeStats;
    }

    if (sourceAvailability.canUseMidBass
        && frequencyDependentMidBassPath.displayBinFftRanges.size() > displayBinIndex)
    {
        const auto midBassBinPowerStats =
            getFrequencyDependentSourceStatsForDisplayBin (
                frequencyDependentMidBassPath,
                frequencyDependentMidBassFftSize,
                displayBinIndex);

        if (midBassBinPowerStats.numBinsUsed > 0)
        {
            result.usedMidBassComposite = true;

            if (centerFrequencyHz <= frequencyDependentMainOnlyMinHz)
            {
                result.transientReferenceStats =
                    blendDisplayBinPowerStats (midBassBinPowerStats,
                        mainStats,
                        blendWeights.mainBlend);
                result.hasTransientReferenceStats = true;
            }

            if (result.hasLowCompositeStats)
            {
                result.lowCompositeStats =
                    blendDisplayBinPowerStats (result.lowCompositeStats,
                        midBassBinPowerStats,
                        blendWeights.midBassBlend);
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

AnalyzerEngine::FrequencyDependentHighCompositeResult
    AnalyzerEngine::applyFrequencyDependentHighBlendForDisplayBin (
        size_t displayBinIndex,
        float centerFrequencyHz,
        const DisplayBinPowerStats& baseStats,
        const FrequencyDependentSourceAvailability& sourceAvailability,
        const FrequencyDependentBlendWeights& blendWeights) const
{
    FrequencyDependentHighCompositeResult result;
    result.compositeStats = baseStats;

    if (centerFrequencyHz <= 0.0f)
        return result;

    if (sourceAvailability.canUseHigh
        && frequencyDependentHighPath.displayBinFftRanges.size() > displayBinIndex
        && blendWeights.highBlend > 0.0f)
    {
        const auto highBinPowerStats =
            getFrequencyDependentSourceStatsForDisplayBin (
                frequencyDependentHighPath,
                frequencyDependentHighFftSize,
                displayBinIndex);

        if (highBinPowerStats.numBinsUsed > 0)
        {
            result.compositeStats =
                blendDisplayBinPowerStats (result.compositeStats,
                    highBinPowerStats,
                    blendWeights.highBlend);

            result.usedHighComposite = true;
        }
    }

    if (sourceAvailability.canUseVeryHigh
        && frequencyDependentVeryHighPath.displayBinFftRanges.size() > displayBinIndex
        && blendWeights.veryHighBlend > 0.0f)
    {
        const auto veryHighBinPowerStats =
            getFrequencyDependentSourceStatsForDisplayBin (
                frequencyDependentVeryHighPath,
                frequencyDependentVeryHighFftSize,
                displayBinIndex);

        if (veryHighBinPowerStats.numBinsUsed > 0)
        {
            result.compositeStats =
                blendDisplayBinPowerStats (result.compositeStats,
                    veryHighBinPowerStats,
                    blendWeights.veryHighBlend);

            result.usedVeryHighComposite = true;
        }
    }

    return result;
}

AnalyzerEngine::FrequencyDependentBinStats
    AnalyzerEngine::getFrequencyDependentBinStatsForDisplayBin (
        int displayBinIndex,
        int fftSizeForBlock,
        const FrequencyDependentSourceAvailability& sourceAvailability) const
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
    result.usedMainComposite = result.mainStats.numBinsUsed > 0;

    if (displayBinCenterFrequenciesHz.size() > index)
    {
        result.centerFrequencyHz = displayBinCenterFrequenciesHz[index];
        result.hasCenterFrequency = result.centerFrequencyHz > 0.0f;
    }

    if (result.hasCenterFrequency)
    {
        const auto blendWeights =
            getFrequencyDependentBlendWeightsForFrequency (result.centerFrequencyHz);

        result.blendWeights = blendWeights;
        result.hasBlendWeights = true;

        const auto lowCompositeResult =
            getFrequencyDependentLowCompositeForDisplayBin (
                index,
                result.centerFrequencyHz,
                result.mainStats,
                sourceAvailability,
                blendWeights);

        result.usedBassComposite = lowCompositeResult.usedBassComposite;
        result.usedMidBassComposite = lowCompositeResult.usedMidBassComposite;

        if (lowCompositeResult.hasTransientReferenceStats)
        {
            result.transientReferenceStats =
                lowCompositeResult.transientReferenceStats;
            result.hasTransientReferenceStats = true;
        }

        if (lowCompositeResult.hasLowCompositeStats)
        {
            result.compositeStats =
                blendDisplayBinPowerStats (lowCompositeResult.lowCompositeStats,
                    result.mainStats,
                    blendWeights.mainBlend);
        }

        const auto highCompositeResult =
            applyFrequencyDependentHighBlendForDisplayBin (
                index,
                result.centerFrequencyHz,
                result.compositeStats,
                sourceAvailability,
                blendWeights);

        result.compositeStats = highCompositeResult.compositeStats;
        result.usedHighComposite = highCompositeResult.usedHighComposite;
        result.usedVeryHighComposite = highCompositeResult.usedVeryHighComposite;
        result.usedFrequencyDependentSourceComposite =
            result.usedBassComposite
            || result.usedMidBassComposite
            || result.usedHighComposite
            || result.usedVeryHighComposite;
    }

    return result;
}

AnalyzerEngine::DisplayBinPowerStats AnalyzerEngine::blendDisplayBinPowerStats (
    const DisplayBinPowerStats& bassStats,
    const DisplayBinPowerStats& mainStats,
    float mainBlend) const noexcept
{
    const auto sanitizePower = [] (float power) noexcept {
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

    if (!frequencyDependentHasBins)
        return mainHasBins ? blendDisplayBinPowerStats (frequencyDependentStats,
                                 mainStats,
                                 1.0f)
                           : frequencyDependentStats;

    if (!mainHasBins)
        return frequencyDependentStats;

    if (!std::isfinite (assistAmount) || assistAmount <= 0.0f)
        return frequencyDependentStats;

    const auto assistBlend =
        juce::jlimit (0.0f, 1.0f, assistAmount);

    return blendDisplayBinPowerStats (frequencyDependentStats,
        mainStats,
        assistBlend);
}

float AnalyzerEngine::getFrequencyDependentTransientAttackBlend() const noexcept
{
    return currentFrequencyDependentTunedResolutionEnabled
               ? frequencyDependentTunedTransientAttackBlend
               : frequencyDependentTransientAttackBlend;
}

float AnalyzerEngine::getFrequencyDependentTransientTailSuppressBlend() const noexcept
{
    return currentFrequencyDependentTunedResolutionEnabled
               ? frequencyDependentTunedTransientTailSuppressBlend
               : frequencyDependentTransientTailSuppressBlend;
}

float AnalyzerEngine::getFrequencyDependentTransientAssistReleaseSeconds() const noexcept
{
    return currentFrequencyDependentTunedResolutionEnabled
               ? frequencyDependentTunedTransientAssistReleaseSeconds
               : frequencyDependentTransientAssistReleaseSeconds;
}

float AnalyzerEngine::getFrequencyDependentLowBassTailReleaseTimeSeconds() const noexcept
{
    return currentFrequencyDependentTunedResolutionEnabled
               ? frequencyDependentTunedLowBassTailReleaseTimeSeconds
               : frequencyDependentLowBassTailReleaseTimeSeconds;
}

float AnalyzerEngine::getFrequencyDependentVeryHighReleaseTimeSeconds() const noexcept
{
    return currentFrequencyDependentTunedResolutionEnabled
               ? frequencyDependentTunedVeryHighReleaseTimeSeconds
               : frequencyDependentVeryHighReleaseTimeSeconds;
}

AnalyzerEngine::FrequencyDependentLiveAssistResult
    AnalyzerEngine::applyFrequencyDependentLiveAssistForDisplayBin (
        const DisplayBinPowerStats& compositeStats,
        const DisplayBinPowerStats& transientReferenceStats,
        float centerFrequencyHz,
        bool hasCenterFrequency,
        const FrequencyDependentBlendWeights& blendWeights,
        float& storedAssistAmount,
        float frameAdvanceSeconds,
        float transientAssistReleaseSmoothing) const noexcept
{
    FrequencyDependentLiveAssistResult result;
    result.liveVisualStats = compositeStats;

    if (!hasCenterFrequency
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
        desiredAssistAmount = getFrequencyDependentTransientAttackBlend();
    }
    else if (frequencyDependentAboveReferenceDb
             >= frequencyDependentTransientTailSuppressMinExcessDb)
    {
        desiredAssistAmount = getFrequencyDependentTransientTailSuppressBlend();
        result.lowBassTailReleaseBlend =
            blendWeights.lowBassTailReleaseBlend;
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
                blendWeights.lowBassTailReleaseBlend);
    }

    result.liveVisualStats =
        applyFrequencyDependentTransientAssist (compositeStats,
            transientReferenceStats,
            centerFrequencyHz,
            result.assistAmount);

    return result;
}

AnalyzerEngine::FrequencyDependentTunedLowBandAlignmentResult
    AnalyzerEngine::applyFrequencyDependentTunedLowBandTransientAlignmentForDisplayBin (
        const DisplayBinPowerStats& liveVisualStats,
        const DisplayBinPowerStats& transientReferenceStats,
        float centerFrequencyHz,
        bool hasCenterFrequency,
        const FrequencyDependentBlendWeights& blendWeights,
        float& storedAlignmentAmount,
        float& storedPreviousReferenceDb,
        float frameAdvanceSeconds,
        float alignmentReleaseSmoothing) const noexcept
{
    FrequencyDependentTunedLowBandAlignmentResult result;
    result.liveVisualStats = liveVisualStats;

    const auto resetStoredState = [&]() noexcept {
        storedAlignmentAmount = 0.0f;
        storedPreviousReferenceDb = -100.0f;
        result.alignmentAmount = 0.0f;
        result.onsetConfidence = 0.0f;
        result.maxLiftDb = 0.0f;
    };

    if (!currentFrequencyDependentTunedResolutionEnabled
        || !hasCenterFrequency
        || centerFrequencyHz <= 0.0f
        || centerFrequencyHz > frequencyDependentTransientAssistMaxHz
        || liveVisualStats.numBinsUsed <= 0
        || transientReferenceStats.numBinsUsed <= 0)
    {
        resetStoredState();
        return result;
    }

    const auto lowBandConfidence =
        juce::jlimit (0.0f,
            1.0f,
            std::isfinite (blendWeights.lowBassTailReleaseBlend)
                ? blendWeights.lowBassTailReleaseBlend
                : 0.0f);

    if (lowBandConfidence <= 0.0f)
    {
        resetStoredState();
        return result;
    }

    const auto liveDb =
        clampAnalyzerDisplayDb (displayBinPowerStatsToDb (liveVisualStats));

    const auto referenceDb =
        clampAnalyzerDisplayDb (displayBinPowerStatsToDb (transientReferenceStats));

    const auto previousReferenceDb =
        clampAnalyzerDisplayDb (storedPreviousReferenceDb);

    const auto referenceRiseDb = referenceDb - previousReferenceDb;
    const auto gapDb = referenceDb - liveDb;

    storedPreviousReferenceDb = referenceDb;

    auto onsetConfidence = 0.0f;

    if (referenceRiseDb > 0.0f && gapDb > 0.0f)
    {
        const auto riseConfidence =
            smoothLinearBlend (referenceRiseDb,
                frequencyDependentTunedLowBandOnsetMinRiseDb,
                frequencyDependentTunedLowBandOnsetFullRiseDb);

        const auto gapConfidence =
            smoothLinearBlend (gapDb,
                frequencyDependentTunedLowBandOnsetMinGapDb,
                frequencyDependentTunedLowBandOnsetFullGapDb);

        const auto energyConfidence =
            smoothLinearBlend (referenceDb,
                frequencyDependentTunedLowBandAlignmentMinEnergyDb,
                frequencyDependentTunedLowBandAlignmentFullEnergyDb);

        onsetConfidence =
            lowBandConfidence
            * riseConfidence
            * gapConfidence
            * energyConfidence;
    }

    onsetConfidence =
        juce::jlimit (0.0f,
            1.0f,
            std::isfinite (onsetConfidence)
                ? onsetConfidence
                : 0.0f);

    result.onsetConfidence = onsetConfidence;

    const auto maxLiftDb =
        juce::jlimit (0.0f,
            frequencyDependentTunedLowBandAlignmentMaxLiftDb,
            frequencyDependentTunedLowBandAlignmentMaxLiftDb * onsetConfidence);

    result.maxLiftDb = maxLiftDb;

    auto desiredAlignmentAmount = 0.0f;

    if (onsetConfidence > 0.0f)
        desiredAlignmentAmount =
            frequencyDependentTunedLowBandAlignmentMaxBlend * onsetConfidence;

    desiredAlignmentAmount =
        juce::jlimit (0.0f,
            frequencyDependentTunedLowBandAlignmentMaxBlend,
            std::isfinite (desiredAlignmentAmount)
                ? desiredAlignmentAmount
                : 0.0f);

    storedAlignmentAmount =
        std::isfinite (storedAlignmentAmount) ? storedAlignmentAmount : 0.0f;

    if (desiredAlignmentAmount > storedAlignmentAmount)
    {
        storedAlignmentAmount = desiredAlignmentAmount;
    }
    else if (frameAdvanceSeconds > 0.0f)
    {
        storedAlignmentAmount +=
            alignmentReleaseSmoothing
            * (desiredAlignmentAmount - storedAlignmentAmount);
    }
    else
    {
        storedAlignmentAmount = desiredAlignmentAmount;
    }

    storedAlignmentAmount =
        juce::jlimit (0.0f,
            frequencyDependentTunedLowBandAlignmentMaxBlend,
            std::isfinite (storedAlignmentAmount)
                ? storedAlignmentAmount
                : 0.0f);

    if (storedAlignmentAmount <= 0.001f
        || onsetConfidence <= 0.0f
        || maxLiftDb <= 0.0f
        || gapDb <= 0.0f)
    {
        result.alignmentAmount = 0.0f;
        return result;
    }

    const auto maximumTargetDb = clampAnalyzerDisplayDb (liveDb + maxLiftDb);

    auto effectiveAlignmentAmount = storedAlignmentAmount;
    auto candidateStats =
        blendDisplayBinPowerStats (liveVisualStats,
            transientReferenceStats,
            effectiveAlignmentAmount);

    auto candidateDb =
        clampAnalyzerDisplayDb (displayBinPowerStatsToDb (candidateStats));

    if (candidateDb <= liveDb)
    {
        result.alignmentAmount = 0.0f;
        return result;
    }

    if (candidateDb > maximumTargetDb)
    {
        auto lowerBlend = 0.0f;
        auto upperBlend = effectiveAlignmentAmount;

        for (auto iteration = 0; iteration < 6; ++iteration)
        {
            const auto midBlend = (lowerBlend + upperBlend) * 0.5f;

            const auto midStats =
                blendDisplayBinPowerStats (liveVisualStats,
                    transientReferenceStats,
                    midBlend);

            const auto midDb =
                clampAnalyzerDisplayDb (displayBinPowerStatsToDb (midStats));

            if (midDb > maximumTargetDb)
                upperBlend = midBlend;
            else
                lowerBlend = midBlend;
        }

        effectiveAlignmentAmount = lowerBlend;

        candidateStats =
            blendDisplayBinPowerStats (liveVisualStats,
                transientReferenceStats,
                effectiveAlignmentAmount);

        candidateDb =
            clampAnalyzerDisplayDb (displayBinPowerStatsToDb (candidateStats));
    }

    if (effectiveAlignmentAmount <= 0.001f || candidateDb <= liveDb)
    {
        result.alignmentAmount = 0.0f;
        return result;
    }

    storedAlignmentAmount = effectiveAlignmentAmount;
    result.alignmentAmount = effectiveAlignmentAmount;
    result.liveVisualStats = candidateStats;

    return result;
}

AnalyzerEngine::FrequencyDependentLiveReleaseBlendWeights
    AnalyzerEngine::getFrequencyDependentLiveReleaseBlendWeightsForDisplayBin (
        const FrequencyDependentBinStats& binStats,
        const FrequencyDependentLiveAssistResult& assistResult) const noexcept
{
    FrequencyDependentLiveReleaseBlendWeights result;

    result.lowBassTailReleaseBlend =
        std::isfinite (assistResult.lowBassTailReleaseBlend)
            ? assistResult.lowBassTailReleaseBlend
            : 0.0f;

    if (binStats.usedVeryHighComposite
        && binStats.hasBlendWeights)
    {
        result.veryHighReleaseBlend =
            std::isfinite (binStats.blendWeights.veryHighBlend)
                ? binStats.blendWeights.veryHighBlend
                : 0.0f;

        if (result.veryHighReleaseBlend <= 0.0f)
            result.veryHighReleaseBlend = 0.0f;
    }

    return result;
}

AnalyzerEngine::FrequencyDependentBinPolicySnapshot
    AnalyzerEngine::getFrequencyDependentBinPolicySnapshot (
        const FrequencyDependentBinStats& binStats,
        const FrequencyDependentLiveReleaseBlendWeights& liveReleaseBlendWeights,
        float tunedLowBandTransientAlignmentAmount,
        float tunedLowBandOnsetConfidence,
        float tunedLowBandAlignmentMaxLiftDb) const noexcept
{
    FrequencyDependentBinPolicySnapshot result;

    result.centerFrequencyHz = binStats.centerFrequencyHz;
    result.blendWeights = binStats.blendWeights;
    result.liveReleaseBlendWeights = liveReleaseBlendWeights;

    result.hasCenterFrequency = binStats.hasCenterFrequency;
    result.hasBlendWeights = binStats.hasBlendWeights;

    result.usedBassComposite = binStats.usedBassComposite;
    result.usedMidBassComposite = binStats.usedMidBassComposite;
    result.usedMainComposite = binStats.usedMainComposite;
    result.usedHighComposite = binStats.usedHighComposite;
    result.usedVeryHighComposite = binStats.usedVeryHighComposite;
    result.usedFrequencyDependentSourceComposite =
        binStats.usedFrequencyDependentSourceComposite;

    result.usesLowBassFastRelease =
        result.liveReleaseBlendWeights.lowBassTailReleaseBlend > 0.0f;

    result.usesVeryHighFastRelease =
        result.liveReleaseBlendWeights.veryHighReleaseBlend > 0.0f;

    result.tunedLowBandTransientAlignmentAmount =
        juce::jlimit (0.0f,
            1.0f,
            std::isfinite (tunedLowBandTransientAlignmentAmount)
                ? tunedLowBandTransientAlignmentAmount
                : 0.0f);

    result.usesTunedLowBandTransientAlignment =
        result.tunedLowBandTransientAlignmentAmount > 0.001f;

    result.tunedLowBandOnsetConfidence =
        juce::jlimit (0.0f,
            1.0f,
            std::isfinite (tunedLowBandOnsetConfidence)
                ? tunedLowBandOnsetConfidence
                : 0.0f);

    result.tunedLowBandAlignmentMaxLiftDb =
        juce::jlimit (0.0f,
            frequencyDependentTunedLowBandAlignmentMaxLiftDb,
            std::isfinite (tunedLowBandAlignmentMaxLiftDb)
                ? tunedLowBandAlignmentMaxLiftDb
                : 0.0f);

    result.hasTunedLowBandOnsetConfidence =
        result.tunedLowBandOnsetConfidence > 0.001f;

    result.policyBand = getFrequencyDependentPolicyBandForSnapshot (result);

    result.isTransitionBand =
        result.policyBand == FrequencyDependentPolicyBand::bassToMidBass
        || result.policyBand == FrequencyDependentPolicyBand::midBassToMain
        || result.policyBand == FrequencyDependentPolicyBand::mainToHigh
        || result.policyBand == FrequencyDependentPolicyBand::highToVeryHigh;

    return result;
}

AnalyzerEngine::FrequencyDependentPolicyBand
    AnalyzerEngine::getFrequencyDependentPolicyBandForSnapshot (
        const FrequencyDependentBinPolicySnapshot& snapshot) const noexcept
{
    if (!snapshot.hasCenterFrequency)
        return FrequencyDependentPolicyBand::none;

    const auto hasBlendWeights = snapshot.hasBlendWeights;
    const auto veryHighBlend =
        hasBlendWeights && std::isfinite (snapshot.blendWeights.veryHighBlend)
            ? snapshot.blendWeights.veryHighBlend
            : 0.0f;

    const auto mainBlend =
        hasBlendWeights && std::isfinite (snapshot.blendWeights.mainBlend)
            ? snapshot.blendWeights.mainBlend
            : 0.0f;

    const auto midBassBlend =
        hasBlendWeights && std::isfinite (snapshot.blendWeights.midBassBlend)
            ? snapshot.blendWeights.midBassBlend
            : 0.0f;

    constexpr auto fullBlendThreshold = 0.999f;

    if (snapshot.usedVeryHighComposite)
        return veryHighBlend >= fullBlendThreshold
                   ? FrequencyDependentPolicyBand::veryHigh
                   : FrequencyDependentPolicyBand::highToVeryHigh;

    if (snapshot.usedHighComposite)
        return FrequencyDependentPolicyBand::mainToHigh;

    if (snapshot.usedMidBassComposite
        && snapshot.usedMainComposite
        && mainBlend > 0.0f)
    {
        return FrequencyDependentPolicyBand::midBassToMain;
    }

    if (snapshot.usedBassComposite
        && snapshot.usedMidBassComposite
        && midBassBlend > 0.0f)
    {
        return FrequencyDependentPolicyBand::bassToMidBass;
    }

    if (snapshot.usedMidBassComposite)
        return FrequencyDependentPolicyBand::bassToMidBass;

    if (snapshot.usedBassComposite)
        return FrequencyDependentPolicyBand::bass;

    if (snapshot.usedMainComposite)
        return FrequencyDependentPolicyBand::main;

    return FrequencyDependentPolicyBand::none;
}

void AnalyzerEngine::resetFrequencyDependentPolicyFrameSummary() noexcept
{
    frequencyDependentPolicyFrameSummary = {};
}

void AnalyzerEngine::accumulateFrequencyDependentPolicyFrameSummary (
    const FrequencyDependentBinPolicySnapshot& snapshot) noexcept
{
    ++frequencyDependentPolicyFrameSummary.totalBins;

    switch (snapshot.policyBand)
    {
        case FrequencyDependentPolicyBand::bass:
            ++frequencyDependentPolicyFrameSummary.bassBins;
            break;

        case FrequencyDependentPolicyBand::bassToMidBass:
            ++frequencyDependentPolicyFrameSummary.bassToMidBassBins;
            break;

        case FrequencyDependentPolicyBand::midBassToMain:
            ++frequencyDependentPolicyFrameSummary.midBassToMainBins;
            break;

        case FrequencyDependentPolicyBand::main:
            ++frequencyDependentPolicyFrameSummary.mainBins;
            break;

        case FrequencyDependentPolicyBand::mainToHigh:
            ++frequencyDependentPolicyFrameSummary.mainToHighBins;
            break;

        case FrequencyDependentPolicyBand::highToVeryHigh:
            ++frequencyDependentPolicyFrameSummary.highToVeryHighBins;
            break;

        case FrequencyDependentPolicyBand::veryHigh:
            ++frequencyDependentPolicyFrameSummary.veryHighBins;
            break;

        case FrequencyDependentPolicyBand::none:
        default:
            ++frequencyDependentPolicyFrameSummary.noneBins;
            break;
    }

    if (snapshot.usedFrequencyDependentSourceComposite)
        ++frequencyDependentPolicyFrameSummary.binsUsingFrequencyDependentSources;

    if (snapshot.usesLowBassFastRelease)
        ++frequencyDependentPolicyFrameSummary.binsUsingLowBassFastRelease;

    if (snapshot.usesVeryHighFastRelease)
        ++frequencyDependentPolicyFrameSummary.binsUsingVeryHighFastRelease;

    if (snapshot.usesTunedLowBandTransientAlignment)
        ++frequencyDependentPolicyFrameSummary.binsUsingTunedLowBandTransientAlignment;

    if (snapshot.hasTunedLowBandOnsetConfidence)
        ++frequencyDependentPolicyFrameSummary.binsWithTunedLowBandOnsetConfidence;

    if (snapshot.isTransitionBand)
        ++frequencyDependentPolicyFrameSummary.transitionBins;
}

void AnalyzerEngine::finalizeFrequencyDependentPolicyFrameSummary() noexcept
{
    auto& summary = frequencyDependentPolicyFrameSummary;

    summary.classifiedBins =
        summary.noneBins
        + summary.bassBins
        + summary.bassToMidBassBins
        + summary.midBassToMainBins
        + summary.mainBins
        + summary.mainToHighBins
        + summary.highToVeryHighBins
        + summary.veryHighBins;

    summary.hasConsistentBinCounts =
        summary.classifiedBins == summary.totalBins;

    summary.noneRatio =
        safePolicyRatio (summary.noneBins, summary.totalBins);

    summary.bassRatio =
        safePolicyRatio (summary.bassBins, summary.totalBins);

    summary.bassToMidBassRatio =
        safePolicyRatio (summary.bassToMidBassBins, summary.totalBins);

    summary.midBassToMainRatio =
        safePolicyRatio (summary.midBassToMainBins, summary.totalBins);

    summary.mainRatio =
        safePolicyRatio (summary.mainBins, summary.totalBins);

    summary.mainToHighRatio =
        safePolicyRatio (summary.mainToHighBins, summary.totalBins);

    summary.highToVeryHighRatio =
        safePolicyRatio (summary.highToVeryHighBins, summary.totalBins);

    summary.veryHighRatio =
        safePolicyRatio (summary.veryHighBins, summary.totalBins);

    summary.frequencyDependentSourceRatio =
        safePolicyRatio (summary.binsUsingFrequencyDependentSources,
            summary.totalBins);

    summary.lowBassFastReleaseRatio =
        safePolicyRatio (summary.binsUsingLowBassFastRelease,
            summary.totalBins);

    summary.veryHighFastReleaseRatio =
        safePolicyRatio (summary.binsUsingVeryHighFastRelease,
            summary.totalBins);

    summary.tunedLowBandTransientAlignmentRatio =
        safePolicyRatio (summary.binsUsingTunedLowBandTransientAlignment,
            summary.totalBins);

    summary.tunedLowBandOnsetConfidenceRatio =
        safePolicyRatio (summary.binsWithTunedLowBandOnsetConfidence,
            summary.totalBins);

    summary.transitionRatio =
        safePolicyRatio (summary.transitionBins, summary.totalBins);
}

void AnalyzerEngine::resetVqtLikeFilterbankState() noexcept
{
    for (auto& band : vqtLikeFilterBands)
    {
        band.z1 = 0.0f;
        band.z2 = 0.0f;
        band.fastZ1 = 0.0f;
        band.fastZ2 = 0.0f;
        band.power = 0.0f;
        band.lastFramePower = 0.0f;
        band.peakPower = 0.0f;
        band.lastFramePeakPower = 0.0f;
        band.fastPower = 0.0f;
        band.fastPeakPower = 0.0f;
        band.lastFrameFastPower = 0.0f;
        band.lastFrameFastPeakPower = 0.0f;
        band.noiseFloorPower = 0.0f;
    }
}

void AnalyzerEngine::resetVqtLikeFrameSummary() noexcept
{
    vqtLikeFrameSummary = {};

    for (const auto& band : vqtLikeFilterBands)
    {
        if (band.isConfigured)
            ++vqtLikeFrameSummary.configuredAnalysisBands;
    }
}

void AnalyzerEngine::accumulateVqtLikeFrameSummary (
    const VqtLikeDisplayBinStats& stats) noexcept
{
    if (!stats.isConfigured)
        return;

    ++vqtLikeFrameSummary.displayBinsConfigured;
    vqtLikeFrameSummary.averageAnalysisBandsUsed +=
        static_cast<float> (stats.analysisBandsUsed);
    vqtLikeFrameSummary.averageTonalBandwidthHz += stats.tonalBandwidthHz;
    vqtLikeFrameSummary.averageFastBandwidthHz += stats.fastBandwidthHz;
    vqtLikeFrameSummary.averageTonalEffectiveQ += stats.tonalEffectiveQ;
    vqtLikeFrameSummary.averageFastEffectiveQ += stats.fastEffectiveQ;
    vqtLikeFrameSummary.averageTonalToFastMeanRatioDb +=
        stats.tonalToFastMeanRatioDb;
    vqtLikeFrameSummary.averageTonalToFastPeakRatioDb +=
        stats.tonalToFastPeakRatioDb;
    vqtLikeFrameSummary.averageLiveLiftFromFastDb += stats.liveLiftFromFastDb;
    vqtLikeFrameSummary.averagePeakHoldLiftFromFastDb +=
        stats.peakHoldLiftFromFastDb;
    vqtLikeFrameSummary.averageTonalPeakToMeanDb += stats.tonalPeakToMeanDb;
    vqtLikeFrameSummary.averageFastPeakToMeanDb += stats.fastPeakToMeanDb;
    vqtLikeFrameSummary.maxLiveLiftFromFastDb =
        juce::jmax (vqtLikeFrameSummary.maxLiveLiftFromFastDb,
            stats.liveLiftFromFastDb);
    vqtLikeFrameSummary.maxPeakHoldLiftFromFastDb =
        juce::jmax (vqtLikeFrameSummary.maxPeakHoldLiftFromFastDb,
            stats.peakHoldLiftFromFastDb);

    if (stats.liveLiftFromFastDb > 0.25f
        || stats.peakHoldLiftFromFastDb > 0.25f)
    {
        ++vqtLikeFrameSummary.binsWithFastLift;
    }

    if (stats.hasReferenceComparison)
    {
        ++vqtLikeFrameSummary.binsWithReferenceComparison;
        vqtLikeFrameSummary.averageMetricReferenceErrorDb +=
            stats.metricReferenceErrorDb;
        vqtLikeFrameSummary.averageLiveReferenceErrorDb +=
            stats.liveReferenceErrorDb;
        vqtLikeFrameSummary.averageAbsMetricReferenceErrorDb +=
            std::abs (stats.metricReferenceErrorDb);
        vqtLikeFrameSummary.averageAbsLiveReferenceErrorDb +=
            std::abs (stats.liveReferenceErrorDb);
        vqtLikeFrameSummary.maxAbsMetricReferenceErrorDb =
            juce::jmax (vqtLikeFrameSummary.maxAbsMetricReferenceErrorDb,
                std::abs (stats.metricReferenceErrorDb));
        vqtLikeFrameSummary.maxAbsLiveReferenceErrorDb =
            juce::jmax (vqtLikeFrameSummary.maxAbsLiveReferenceErrorDb,
                std::abs (stats.liveReferenceErrorDb));
    }
}

void AnalyzerEngine::finalizeVqtLikeFrameSummary() noexcept
{
    const auto sanitizeDiagnosticValue = [] (float value) noexcept {
        return std::isfinite (value) ? value : 0.0f;
    };

    const auto analysisBandCount =
        static_cast<float> (juce::jmax (1, vqtLikeAnalysisBandCount));

    vqtLikeFrameSummary.configuredAnalysisBandRatio =
        static_cast<float> (vqtLikeFrameSummary.configuredAnalysisBands)
        / analysisBandCount;

    vqtLikeFrameSummary.displayBinConfiguredRatio =
        static_cast<float> (vqtLikeFrameSummary.displayBinsConfigured)
        / static_cast<float> (juce::jmax (1, displayBinCount));

    if (vqtLikeFrameSummary.displayBinsConfigured <= 0)
        return;

    const auto inverseConfiguredDisplayBins =
        1.0f / static_cast<float> (vqtLikeFrameSummary.displayBinsConfigured);

    vqtLikeFrameSummary.averageAnalysisBandsUsed *= inverseConfiguredDisplayBins;
    vqtLikeFrameSummary.averageTonalBandwidthHz *= inverseConfiguredDisplayBins;
    vqtLikeFrameSummary.averageFastBandwidthHz *= inverseConfiguredDisplayBins;
    vqtLikeFrameSummary.averageTonalEffectiveQ *= inverseConfiguredDisplayBins;
    vqtLikeFrameSummary.averageFastEffectiveQ *= inverseConfiguredDisplayBins;
    vqtLikeFrameSummary.averageTonalToFastMeanRatioDb *= inverseConfiguredDisplayBins;
    vqtLikeFrameSummary.averageTonalToFastPeakRatioDb *= inverseConfiguredDisplayBins;
    vqtLikeFrameSummary.averageLiveLiftFromFastDb *= inverseConfiguredDisplayBins;
    vqtLikeFrameSummary.averagePeakHoldLiftFromFastDb *= inverseConfiguredDisplayBins;
    vqtLikeFrameSummary.averageTonalPeakToMeanDb *= inverseConfiguredDisplayBins;
    vqtLikeFrameSummary.averageFastPeakToMeanDb *= inverseConfiguredDisplayBins;
    vqtLikeFrameSummary.fastLiftBinRatio =
        static_cast<float> (vqtLikeFrameSummary.binsWithFastLift)
        * inverseConfiguredDisplayBins;

    if (vqtLikeFrameSummary.binsWithReferenceComparison > 0)
    {
        const auto inverseReferenceBins =
            1.0f
            / static_cast<float> (vqtLikeFrameSummary.binsWithReferenceComparison);

        vqtLikeFrameSummary.averageMetricReferenceErrorDb *= inverseReferenceBins;
        vqtLikeFrameSummary.averageLiveReferenceErrorDb *= inverseReferenceBins;
        vqtLikeFrameSummary.averageAbsMetricReferenceErrorDb *= inverseReferenceBins;
        vqtLikeFrameSummary.averageAbsLiveReferenceErrorDb *= inverseReferenceBins;
    }

    vqtLikeFrameSummary.referenceComparisonBinRatio =
        static_cast<float> (vqtLikeFrameSummary.binsWithReferenceComparison)
        * inverseConfiguredDisplayBins;

    vqtLikeFrameSummary.configuredAnalysisBandRatio =
        sanitizeDiagnosticValue (vqtLikeFrameSummary.configuredAnalysisBandRatio);
    vqtLikeFrameSummary.displayBinConfiguredRatio =
        sanitizeDiagnosticValue (vqtLikeFrameSummary.displayBinConfiguredRatio);
    vqtLikeFrameSummary.averageAnalysisBandsUsed =
        sanitizeDiagnosticValue (vqtLikeFrameSummary.averageAnalysisBandsUsed);
    vqtLikeFrameSummary.averageTonalBandwidthHz =
        sanitizeDiagnosticValue (vqtLikeFrameSummary.averageTonalBandwidthHz);
    vqtLikeFrameSummary.averageFastBandwidthHz =
        sanitizeDiagnosticValue (vqtLikeFrameSummary.averageFastBandwidthHz);
    vqtLikeFrameSummary.averageTonalEffectiveQ =
        sanitizeDiagnosticValue (vqtLikeFrameSummary.averageTonalEffectiveQ);
    vqtLikeFrameSummary.averageFastEffectiveQ =
        sanitizeDiagnosticValue (vqtLikeFrameSummary.averageFastEffectiveQ);
    vqtLikeFrameSummary.averageTonalToFastMeanRatioDb =
        sanitizeDiagnosticValue (vqtLikeFrameSummary.averageTonalToFastMeanRatioDb);
    vqtLikeFrameSummary.averageTonalToFastPeakRatioDb =
        sanitizeDiagnosticValue (vqtLikeFrameSummary.averageTonalToFastPeakRatioDb);
    vqtLikeFrameSummary.averageLiveLiftFromFastDb =
        sanitizeDiagnosticValue (vqtLikeFrameSummary.averageLiveLiftFromFastDb);
    vqtLikeFrameSummary.averagePeakHoldLiftFromFastDb =
        sanitizeDiagnosticValue (vqtLikeFrameSummary.averagePeakHoldLiftFromFastDb);
    vqtLikeFrameSummary.averageTonalPeakToMeanDb =
        sanitizeDiagnosticValue (vqtLikeFrameSummary.averageTonalPeakToMeanDb);
    vqtLikeFrameSummary.averageFastPeakToMeanDb =
        sanitizeDiagnosticValue (vqtLikeFrameSummary.averageFastPeakToMeanDb);
    vqtLikeFrameSummary.maxLiveLiftFromFastDb =
        sanitizeDiagnosticValue (vqtLikeFrameSummary.maxLiveLiftFromFastDb);
    vqtLikeFrameSummary.maxPeakHoldLiftFromFastDb =
        sanitizeDiagnosticValue (vqtLikeFrameSummary.maxPeakHoldLiftFromFastDb);
    vqtLikeFrameSummary.fastLiftBinRatio =
        sanitizeDiagnosticValue (vqtLikeFrameSummary.fastLiftBinRatio);
    vqtLikeFrameSummary.averageMetricReferenceErrorDb =
        sanitizeDiagnosticValue (vqtLikeFrameSummary.averageMetricReferenceErrorDb);
    vqtLikeFrameSummary.averageLiveReferenceErrorDb =
        sanitizeDiagnosticValue (vqtLikeFrameSummary.averageLiveReferenceErrorDb);
    vqtLikeFrameSummary.averageAbsMetricReferenceErrorDb =
        sanitizeDiagnosticValue (vqtLikeFrameSummary.averageAbsMetricReferenceErrorDb);
    vqtLikeFrameSummary.averageAbsLiveReferenceErrorDb =
        sanitizeDiagnosticValue (vqtLikeFrameSummary.averageAbsLiveReferenceErrorDb);
    vqtLikeFrameSummary.maxAbsMetricReferenceErrorDb =
        sanitizeDiagnosticValue (vqtLikeFrameSummary.maxAbsMetricReferenceErrorDb);
    vqtLikeFrameSummary.maxAbsLiveReferenceErrorDb =
        sanitizeDiagnosticValue (vqtLikeFrameSummary.maxAbsLiveReferenceErrorDb);
    vqtLikeFrameSummary.referenceComparisonBinRatio =
        sanitizeDiagnosticValue (vqtLikeFrameSummary.referenceComparisonBinRatio);
}

void AnalyzerEngine::configureVqtLikeFilterbankIfNeeded()
{
    const auto sampleRate = static_cast<float> (currentSampleRate);

    if (sampleRate <= 0.0f || !std::isfinite (sampleRate))
        return;

    if (displayBinCenterFrequenciesHz.size() != static_cast<size_t> (displayBinCount))
        return;

    const auto needsResize =
        vqtLikeFilterBands.size() != static_cast<size_t> (vqtLikeAnalysisBandCount);

    if (needsResize)
        vqtLikeFilterBands.assign (static_cast<size_t> (vqtLikeAnalysisBandCount), {});

    const auto configurationChanged =
        needsResize
        || vqtLikeFilterbankNeedsReset
        || std::abs (vqtLikeFilterbankSampleRate - sampleRate) >= 0.001f
        || std::abs (vqtLikeFilterbankMinFrequencyHz - currentDisplayMinFrequencyHz) >= 0.001f
        || std::abs (vqtLikeFilterbankMaxFrequencyHz - currentDisplayMaxFrequencyHz) >= 0.001f;

    if (!configurationChanged)
        return;

    const auto analysisDenominator =
        static_cast<float> (juce::jmax (1, vqtLikeAnalysisBandCount - 1));

    for (int i = 0; i < vqtLikeAnalysisBandCount; ++i)
    {
        const auto normalisedPosition =
            static_cast<float> (i) / analysisDenominator;

        const auto centerFrequencyHz =
            logFrequencyAtNormalisedPosition (normalisedPosition,
                currentDisplayMinFrequencyHz,
                currentDisplayMaxFrequencyHz);

        configureVqtLikeFilterBand (
            vqtLikeFilterBands[static_cast<size_t> (i)],
            centerFrequencyHz,
            sampleRate);

        auto& band = vqtLikeFilterBands[static_cast<size_t> (i)];
        const auto displayPosition =
            normalisedPosition * static_cast<float> (displayBinCount - 1);
        const auto halfAnalysisBandWidthDisplayBins =
            0.5f / static_cast<float> (vqtLikeAnalysisBandsPerDisplayBin);

        band.normalisedPosition = normalisedPosition;
        band.leftDisplayBin = displayPosition - halfAnalysisBandWidthDisplayBins;
        band.rightDisplayBin = displayPosition + halfAnalysisBandWidthDisplayBins;
    }

    vqtLikeFilterbankSampleRate = sampleRate;
    vqtLikeFilterbankMinFrequencyHz = currentDisplayMinFrequencyHz;
    vqtLikeFilterbankMaxFrequencyHz = currentDisplayMaxFrequencyHz;
    vqtLikeFilterbankNeedsReset = false;
    resetVqtLikeFilterbankState();
}

float AnalyzerEngine::powerToAnalyzerDb (float power) const noexcept
{
    const auto safePower =
        juce::jmax (0.0f, std::isfinite (power) ? power : 0.0f);

    const auto magnitude = std::sqrt (safePower);

    return juce::jlimit (-100.0f,
        0.0f,
        juce::Decibels::gainToDecibels (magnitude, -100.0f));
}

float AnalyzerEngine::safePowerRatioDb (
    float numeratorPower,
    float denominatorPower) const noexcept
{
    constexpr auto ratioPowerFloor = 1.0e-20f;

    const auto safeNumerator =
        juce::jmax (ratioPowerFloor,
            std::isfinite (numeratorPower) ? numeratorPower : ratioPowerFloor);

    const auto safeDenominator =
        juce::jmax (ratioPowerFloor,
            std::isfinite (denominatorPower) ? denominatorPower : ratioPowerFloor);

    const auto ratioDb = 10.0f * std::log10 (safeNumerator / safeDenominator);

    return juce::jlimit (-60.0f,
        60.0f,
        std::isfinite (ratioDb) ? ratioDb : 0.0f);
}

float AnalyzerEngine::getBiquadMagnitudeAtFrequency (
    float b0,
    float b1,
    float b2,
    float a1,
    float a2,
    float frequencyHz,
    float sampleRate) const noexcept
{
    if (!std::isfinite (b0)
        || !std::isfinite (b1)
        || !std::isfinite (b2)
        || !std::isfinite (a1)
        || !std::isfinite (a2)
        || !std::isfinite (frequencyHz)
        || !std::isfinite (sampleRate)
        || frequencyHz < 0.0f
        || sampleRate <= 0.0f)
    {
        return 1.0f;
    }

    const auto nyquist = sampleRate * 0.5f;
    const auto safeFrequencyHz =
        juce::jlimit (0.0f, nyquist * 0.999f, frequencyHz);

    const auto omega =
        juce::MathConstants<float>::twoPi * safeFrequencyHz / sampleRate;

    const auto z1Real = std::cos (omega);
    const auto z1Imag = -std::sin (omega);
    const auto z2Real = std::cos (2.0f * omega);
    const auto z2Imag = -std::sin (2.0f * omega);

    const auto numeratorReal = b0 + b1 * z1Real + b2 * z2Real;
    const auto numeratorImag = b1 * z1Imag + b2 * z2Imag;
    const auto denominatorReal = 1.0f + a1 * z1Real + a2 * z2Real;
    const auto denominatorImag = a1 * z1Imag + a2 * z2Imag;

    const auto numeratorMagnitudeSquared =
        numeratorReal * numeratorReal + numeratorImag * numeratorImag;

    const auto denominatorMagnitudeSquared =
        denominatorReal * denominatorReal + denominatorImag * denominatorImag;

    if (!std::isfinite (numeratorMagnitudeSquared)
        || !std::isfinite (denominatorMagnitudeSquared)
        || denominatorMagnitudeSquared <= 1.0e-12f)
    {
        return 1.0f;
    }

    const auto magnitude =
        std::sqrt (numeratorMagnitudeSquared / denominatorMagnitudeSquared);

    return juce::jlimit (1.0e-6f,
        64.0f,
        std::isfinite (magnitude) ? magnitude : 1.0f);
}

float AnalyzerEngine::getVqtLikeLayerCalibrationPowerGain (
    float centerGain,
    float trim) const noexcept
{
    const auto safeCenterGain =
        juce::jlimit (1.0e-6f,
            64.0f,
            std::isfinite (centerGain) ? centerGain : 1.0f);

    auto amplitudeGain = 1.0f / safeCenterGain;

    amplitudeGain =
        juce::jlimit (vqtLikeCalibrationMinGain,
            vqtLikeCalibrationMaxGain,
            std::isfinite (amplitudeGain) ? amplitudeGain : 1.0f);

    amplitudeGain *= std::isfinite (trim) ? trim : 1.0f;

    const auto powerGain = amplitudeGain * amplitudeGain;

    return std::isfinite (powerGain) ? powerGain : 1.0f;
}

float AnalyzerEngine::getVqtLikeTonalCalibrationTrimForFrequency (
    float frequencyHz) const noexcept
{
    if (!std::isfinite (frequencyHz) || frequencyHz <= 0.0f)
        return 1.0f;

    auto trimDb = 0.0f;

    const auto subBassFadeOut =
        1.0f - smoothLogFrequencyBlend (frequencyHz, 35.0f, 120.0f);
    trimDb += subBassFadeOut * 1.0f;

    const auto highFadeIn =
        smoothLogFrequencyBlend (frequencyHz, 8000.0f, 16000.0f);
    trimDb += highFadeIn * -0.75f;

    trimDb = juce::jlimit (-1.5f, 1.5f, std::isfinite (trimDb) ? trimDb : 0.0f);

    return juce::Decibels::decibelsToGain (trimDb);
}

float AnalyzerEngine::getVqtLikeFastCalibrationTrimForFrequency (
    float frequencyHz) const noexcept
{
    if (!std::isfinite (frequencyHz) || frequencyHz <= 0.0f)
        return 1.0f;

    auto trimDb = 0.0f;

    const auto lowFadeOut =
        1.0f - smoothLogFrequencyBlend (frequencyHz, 60.0f, 180.0f);
    trimDb += lowFadeOut * -1.0f;

    const auto highFadeIn =
        smoothLogFrequencyBlend (frequencyHz, 6000.0f, 14000.0f);
    trimDb += highFadeIn * 0.5f;

    trimDb = juce::jlimit (-2.0f, 1.0f, std::isfinite (trimDb) ? trimDb : 0.0f);

    return juce::Decibels::decibelsToGain (trimDb);
}

float AnalyzerEngine::getVqtLikeNoiseDensityTrimForFrequency (
    float frequencyHz) const noexcept
{
    juce::ignoreUnused (frequencyHz);

    // Kept neutral for now. Noise-density compensation needs a measured
    // calibration pass because it affects broad-band material differently
    // than sine calibration.
    return 1.0f;
}

bool AnalyzerEngine::configureVqtLikeBandpassLayer (
    float centerFrequencyHz,
    float sampleRate,
    float baseQ,
    float minEffectiveQ,
    float maxEffectiveQ,
    float lowBandGammaHz,
    float gammaFadeStartHz,
    float gammaFadeEndHz,
    float minBandwidthHz,
    float maxBandwidthFractionOfCenter,
    float calibrationTrim,
    float& outBandwidthHz,
    float& outEffectiveQ,
    float& outB0,
    float& outB1,
    float& outB2,
    float& outA1,
    float& outA2,
    float& outCenterGain,
    float& outEquivalentBandwidthHz,
    float& outCalibrationPowerGain) const noexcept
{
    outBandwidthHz = 0.0f;
    outEffectiveQ = 0.0f;
    outB0 = 0.0f;
    outB1 = 0.0f;
    outB2 = 0.0f;
    outA1 = 0.0f;
    outA2 = 0.0f;
    outCenterGain = 1.0f;
    outEquivalentBandwidthHz = 0.0f;
    outCalibrationPowerGain = 1.0f;

    if (!std::isfinite (centerFrequencyHz)
        || !std::isfinite (sampleRate)
        || !std::isfinite (baseQ)
        || !std::isfinite (minEffectiveQ)
        || !std::isfinite (maxEffectiveQ)
        || !std::isfinite (lowBandGammaHz)
        || !std::isfinite (minBandwidthHz)
        || !std::isfinite (maxBandwidthFractionOfCenter)
        || centerFrequencyHz <= 0.0f
        || sampleRate <= 0.0f
        || baseQ <= 0.0f
        || minEffectiveQ <= 0.0f
        || maxEffectiveQ < minEffectiveQ
        || minBandwidthHz <= 0.0f
        || maxBandwidthFractionOfCenter <= 0.0f)
    {
        return false;
    }

    const auto nyquist = sampleRate * 0.5f;
    const auto maxCenterFrequency = nyquist * 0.98f;

    if (centerFrequencyHz >= maxCenterFrequency)
        return false;

    const auto gammaFade =
        smoothLogFrequencyBlend (centerFrequencyHz,
            gammaFadeStartHz,
            gammaFadeEndHz);

    const auto gammaHz = lowBandGammaHz * (1.0f - gammaFade);

    auto bandwidthHz = (centerFrequencyHz / baseQ) + gammaHz;

    const auto maxBandwidthHz =
        juce::jmax (minBandwidthHz,
            juce::jmin (centerFrequencyHz * maxBandwidthFractionOfCenter,
                nyquist * 0.45f));

    bandwidthHz =
        juce::jlimit (minBandwidthHz,
            maxBandwidthHz,
            std::isfinite (bandwidthHz) ? bandwidthHz : minBandwidthHz);

    auto effectiveQ = centerFrequencyHz / bandwidthHz;

    effectiveQ =
        juce::jlimit (minEffectiveQ,
            maxEffectiveQ,
            std::isfinite (effectiveQ) ? effectiveQ : minEffectiveQ);

    bandwidthHz = centerFrequencyHz / effectiveQ;

    const auto omega =
        juce::MathConstants<float>::twoPi * centerFrequencyHz / sampleRate;

    const auto sinOmega = std::sin (omega);
    const auto cosOmega = std::cos (omega);
    const auto alpha = sinOmega / (2.0f * effectiveQ);
    const auto a0 = 1.0f + alpha;

    if (!std::isfinite (alpha)
        || !std::isfinite (a0)
        || a0 <= 0.0f)
    {
        return false;
    }

    outBandwidthHz = bandwidthHz;
    outEffectiveQ = effectiveQ;
    outB0 = alpha / a0;
    outB1 = 0.0f;
    outB2 = -alpha / a0;
    outA1 = (-2.0f * cosOmega) / a0;
    outA2 = (1.0f - alpha) / a0;
    outCenterGain =
        getBiquadMagnitudeAtFrequency (outB0,
            outB1,
            outB2,
            outA1,
            outA2,
            centerFrequencyHz,
            sampleRate);
    outEquivalentBandwidthHz =
        juce::jmax (vqtLikeCalibrationReferenceBandwidthHz, bandwidthHz);
    outCalibrationPowerGain =
        getVqtLikeLayerCalibrationPowerGain (outCenterGain, calibrationTrim);

    return std::isfinite (outBandwidthHz)
           && std::isfinite (outEffectiveQ)
           && outEffectiveQ > 0.0f
           && std::isfinite (outB0)
           && std::isfinite (outB1)
           && std::isfinite (outB2)
           && std::isfinite (outA1)
           && std::isfinite (outA2)
           && std::abs (outA2) < 1.0f
           && std::isfinite (outCenterGain)
           && std::isfinite (outEquivalentBandwidthHz)
           && std::isfinite (outCalibrationPowerGain);
}

void AnalyzerEngine::configureVqtLikeFilterBand (
    VqtLikeFilterBand& band,
    float centerFrequencyHz,
    float sampleRate) noexcept
{
    band = {};

    band.centerFrequencyHz = centerFrequencyHz;

    const auto tonalTrim =
        vqtLikeTonalSineCalibrationTrim
        * getVqtLikeTonalCalibrationTrimForFrequency (centerFrequencyHz);

    const auto fastTrim =
        vqtLikeFastSineCalibrationTrim
        * getVqtLikeFastCalibrationTrimForFrequency (centerFrequencyHz);

    const auto tonalLayerConfigured =
        configureVqtLikeBandpassLayer (
            centerFrequencyHz,
            sampleRate,
            vqtLikeResolutionBaseQ,
            vqtLikeResolutionMinEffectiveQ,
            vqtLikeResolutionMaxEffectiveQ,
            vqtLikeResolutionLowBandGammaHz,
            vqtLikeResolutionGammaFadeStartHz,
            vqtLikeResolutionGammaFadeEndHz,
            vqtLikeResolutionMinBandwidthHz,
            vqtLikeResolutionMaxBandwidthFractionOfCenter,
            tonalTrim,
            band.bandwidthHz,
            band.effectiveQ,
            band.b0,
            band.b1,
            band.b2,
            band.a1,
            band.a2,
            band.tonalCenterGain,
            band.tonalEquivalentBandwidthHz,
            band.tonalCalibrationPowerGain);

    const auto fastLayerConfigured =
        configureVqtLikeBandpassLayer (
            centerFrequencyHz,
            sampleRate,
            vqtLikeFastBaseQ,
            vqtLikeFastMinEffectiveQ,
            vqtLikeFastMaxEffectiveQ,
            vqtLikeFastLowBandGammaHz,
            vqtLikeFastGammaFadeStartHz,
            vqtLikeFastGammaFadeEndHz,
            vqtLikeFastMinBandwidthHz,
            vqtLikeFastMaxBandwidthFractionOfCenter,
            fastTrim,
            band.fastBandwidthHz,
            band.fastEffectiveQ,
            band.fastB0,
            band.fastB1,
            band.fastB2,
            band.fastA1,
            band.fastA2,
            band.fastCenterGain,
            band.fastEquivalentBandwidthHz,
            band.fastCalibrationPowerGain);

    band.isConfigured = tonalLayerConfigured;
    band.hasFastLayer = tonalLayerConfigured && fastLayerConfigured;
    band.calibrationGain =
        std::sqrt (juce::jmax (0.0f, band.tonalCalibrationPowerGain));
    band.fastCalibrationGain =
        std::sqrt (juce::jmax (0.0f, band.fastCalibrationPowerGain));
}

void AnalyzerEngine::processVqtLikeFilterbankSamples (
    const float* samples,
    int numSamples,
    float frameAdvanceSeconds) noexcept
{
    if (samples == nullptr
        || numSamples <= 0
        || vqtLikeFilterBands.size() != static_cast<size_t> (vqtLikeAnalysisBandCount))
    {
        return;
    }

    juce::ScopedNoDenormals noDenormals;

    const auto meanAttackSmoothing =
        smoothingCoefficientForTimeConstant (frameAdvanceSeconds,
            vqtLikeEnvelopeAttackSeconds);

    const auto meanReleaseSmoothing =
        smoothingCoefficientForTimeConstant (frameAdvanceSeconds,
            vqtLikeEnvelopeReleaseSeconds);

    const auto peakReleaseSmoothing =
        smoothingCoefficientForTimeConstant (frameAdvanceSeconds,
            vqtLikePeakEnvelopeReleaseSeconds);

    const auto fastMeanAttackSmoothing =
        smoothingCoefficientForTimeConstant (frameAdvanceSeconds,
            vqtLikeFastEnvelopeAttackSeconds);

    const auto fastMeanReleaseSmoothing =
        smoothingCoefficientForTimeConstant (frameAdvanceSeconds,
            vqtLikeFastEnvelopeReleaseSeconds);

    const auto fastPeakReleaseSmoothing =
        smoothingCoefficientForTimeConstant (frameAdvanceSeconds,
            vqtLikeFastPeakEnvelopeReleaseSeconds);

    const auto inverseNumSamples =
        1.0f / static_cast<float> (numSamples);

    const auto processLayer =
        [samples, numSamples, inverseNumSamples] (
            float b0,
            float b1,
            float b2,
            float a1,
            float a2,
            float calibrationPowerGain,
            float meanAttackSmoothingForLayer,
            float meanReleaseSmoothingForLayer,
            float peakReleaseSmoothingForLayer,
            float& z1State,
            float& z2State,
            float& powerState,
            float& peakPowerState,
            float& lastFramePowerState,
            float& lastFramePeakPowerState) noexcept {
            if (!std::isfinite (b0)
                || !std::isfinite (b1)
                || !std::isfinite (b2)
                || !std::isfinite (a1)
                || !std::isfinite (a2))
            {
                z1State = 0.0f;
                z2State = 0.0f;
                powerState = 0.0f;
                peakPowerState = 0.0f;
                lastFramePowerState = 0.0f;
                lastFramePeakPowerState = 0.0f;
                return;
            }

            auto frameMeanPower = 0.0f;
            auto framePeakPower = 0.0f;
            auto z1 = z1State;
            auto z2 = z2State;

            for (int sampleIndex = 0; sampleIndex < numSamples; ++sampleIndex)
            {
                const auto inputSample = samples[sampleIndex];
                const auto x = std::isfinite (inputSample) ? inputSample : 0.0f;

                const auto y = b0 * x + z1;

                z1 = b1 * x - a1 * y + z2;
                z2 = b2 * x - a2 * y;

                const auto samplePower = y * y;

                frameMeanPower += samplePower;
                framePeakPower = juce::jmax (framePeakPower, samplePower);
            }

            z1State = std::isfinite (z1) ? z1 : 0.0f;
            z2State = std::isfinite (z2) ? z2 : 0.0f;

            const auto safeCalibrationPowerGain =
                std::isfinite (calibrationPowerGain)
                    ? calibrationPowerGain
                    : 1.0f;

            frameMeanPower =
                std::isfinite (frameMeanPower)
                    ? frameMeanPower
                          * inverseNumSamples
                          * vqtLikeMeanPowerScale
                          * safeCalibrationPowerGain
                    : 0.0f;

            framePeakPower =
                std::isfinite (framePeakPower)
                    ? framePeakPower
                          * vqtLikePeakPowerScale
                          * safeCalibrationPowerGain
                    : 0.0f;

            frameMeanPower =
                juce::jlimit (0.0f,
                    vqtLikeMaxDisplayPower,
                    std::isfinite (frameMeanPower) ? frameMeanPower : 0.0f);

            framePeakPower =
                juce::jlimit (0.0f,
                    vqtLikeMaxDisplayPower,
                    std::isfinite (framePeakPower) ? framePeakPower : 0.0f);

            lastFramePowerState = frameMeanPower;
            lastFramePeakPowerState = framePeakPower;

            auto meanPower = std::isfinite (powerState) ? powerState : 0.0f;
            const auto meanSmoothing =
                frameMeanPower > meanPower
                    ? meanAttackSmoothingForLayer
                    : meanReleaseSmoothingForLayer;

            meanPower += meanSmoothing * (frameMeanPower - meanPower);
            powerState =
                juce::jlimit (0.0f,
                    vqtLikeMaxDisplayPower,
                    std::isfinite (meanPower) ? meanPower : 0.0f);

            auto peakPower =
                std::isfinite (peakPowerState) ? peakPowerState : 0.0f;

            if (framePeakPower > peakPower)
                peakPower = framePeakPower;
            else
                peakPower += peakReleaseSmoothingForLayer * (framePeakPower - peakPower);

            peakPowerState =
                juce::jlimit (0.0f,
                    vqtLikeMaxDisplayPower,
                    std::isfinite (peakPower) ? peakPower : 0.0f);
        };

    for (auto& band : vqtLikeFilterBands)
    {
        if (!band.isConfigured)
        {
            band.z1 = 0.0f;
            band.z2 = 0.0f;
            band.fastZ1 = 0.0f;
            band.fastZ2 = 0.0f;
            band.power = 0.0f;
            band.lastFramePower = 0.0f;
            band.peakPower = 0.0f;
            band.lastFramePeakPower = 0.0f;
            band.fastPower = 0.0f;
            band.fastPeakPower = 0.0f;
            band.lastFrameFastPower = 0.0f;
            band.lastFrameFastPeakPower = 0.0f;
            continue;
        }

        processLayer (band.b0,
            band.b1,
            band.b2,
            band.a1,
            band.a2,
            band.tonalCalibrationPowerGain,
            meanAttackSmoothing,
            meanReleaseSmoothing,
            peakReleaseSmoothing,
            band.z1,
            band.z2,
            band.power,
            band.peakPower,
            band.lastFramePower,
            band.lastFramePeakPower);

        if (band.hasFastLayer)
        {
            processLayer (band.fastB0,
                band.fastB1,
                band.fastB2,
                band.fastA1,
                band.fastA2,
                band.fastCalibrationPowerGain,
                fastMeanAttackSmoothing,
                fastMeanReleaseSmoothing,
                fastPeakReleaseSmoothing,
                band.fastZ1,
                band.fastZ2,
                band.fastPower,
                band.fastPeakPower,
                band.lastFrameFastPower,
                band.lastFrameFastPeakPower);
        }
        else
        {
            band.fastZ1 = 0.0f;
            band.fastZ2 = 0.0f;
            band.fastPower = 0.0f;
            band.fastPeakPower = 0.0f;
            band.lastFrameFastPower = 0.0f;
            band.lastFrameFastPeakPower = 0.0f;
        }
    }
}

float AnalyzerEngine::getVqtLikePeakBlendForFrequency (
    float frequencyHz,
    float lowBlend,
    float midBlend,
    float highBlend) const noexcept
{
    if (!std::isfinite (frequencyHz) || frequencyHz <= 0.0f)
        return 0.0f;

    const auto clampedLowBlend =
        juce::jlimit (0.0f, 1.0f, std::isfinite (lowBlend) ? lowBlend : 0.0f);

    const auto clampedMidBlend =
        juce::jlimit (0.0f, 1.0f, std::isfinite (midBlend) ? midBlend : clampedLowBlend);

    const auto clampedHighBlend =
        juce::jlimit (0.0f, 1.0f, std::isfinite (highBlend) ? highBlend : clampedMidBlend);

    const auto lowToMid =
        smoothLogFrequencyBlend (frequencyHz,
            vqtLikePeakBlendLowToMidStartHz,
            vqtLikePeakBlendLowToMidEndHz);

    auto blend =
        clampedLowBlend
        + lowToMid * (clampedMidBlend - clampedLowBlend);

    const auto midToHigh =
        smoothLogFrequencyBlend (frequencyHz,
            vqtLikePeakBlendMidToHighStartHz,
            vqtLikePeakBlendMidToHighEndHz);

    blend += midToHigh * (clampedHighBlend - blend);

    return juce::jlimit (0.0f, 1.0f, std::isfinite (blend) ? blend : 0.0f);
}

float AnalyzerEngine::getVqtLikeTransientDetailBlendForFrequency (
    float frequencyHz) const noexcept
{
    if (!std::isfinite (frequencyHz) || frequencyHz <= 0.0f)
        return 0.0f;

    const auto lowToMid =
        smoothLogFrequencyBlend (frequencyHz,
            vqtLikeTransientDetailLowToMidStartHz,
            vqtLikeTransientDetailLowToMidEndHz);

    auto blend =
        vqtLikeTransientDetailBlendLow
        + lowToMid * (vqtLikeTransientDetailBlendMid - vqtLikeTransientDetailBlendLow);

    const auto midToHigh =
        smoothLogFrequencyBlend (frequencyHz,
            vqtLikeTransientDetailMidToHighStartHz,
            vqtLikeTransientDetailMidToHighEndHz);

    blend += midToHigh * (vqtLikeTransientDetailBlendHigh - blend);

    return juce::jlimit (0.0f, 1.0f, std::isfinite (blend) ? blend : 0.0f);
}

float AnalyzerEngine::getVqtLikeTransientDetailMaxLiftDbForFrequency (
    float frequencyHz) const noexcept
{
    if (!std::isfinite (frequencyHz) || frequencyHz <= 0.0f)
        return 0.0f;

    const auto lowToMid =
        smoothLogFrequencyBlend (frequencyHz,
            vqtLikeTransientDetailLowToMidStartHz,
            vqtLikeTransientDetailLowToMidEndHz);

    auto maxLiftDb =
        vqtLikeTransientDetailMaxLiftDbLow
        + lowToMid
              * (vqtLikeTransientDetailMaxLiftDbMid
                  - vqtLikeTransientDetailMaxLiftDbLow);

    const auto midToHigh =
        smoothLogFrequencyBlend (frequencyHz,
            vqtLikeTransientDetailMidToHighStartHz,
            vqtLikeTransientDetailMidToHighEndHz);

    maxLiftDb +=
        midToHigh * (vqtLikeTransientDetailMaxLiftDbHigh - maxLiftDb);

    return juce::jlimit (0.0f, 10.0f, std::isfinite (maxLiftDb) ? maxLiftDb : 0.0f);
}

float AnalyzerEngine::getVqtLikeAggregationPeakShapeBlendForFrequency (
    float frequencyHz) const noexcept
{
    return getVqtLikePeakBlendForFrequency (frequencyHz,
        vqtLikeAggregationPeakShapeBlendLow,
        vqtLikeAggregationPeakShapeBlendMid,
        vqtLikeAggregationPeakShapeBlendHigh);
}

float AnalyzerEngine::limitPowerLiftDb (
    float basePower,
    float candidatePower,
    float maxLiftDb) const noexcept
{
    const auto safeBasePower =
        juce::jlimit (0.0f,
            vqtLikeMaxDisplayPower,
            std::isfinite (basePower) ? basePower : 0.0f);

    const auto safeCandidatePower =
        juce::jlimit (0.0f,
            vqtLikeMaxDisplayPower,
            std::isfinite (candidatePower) ? candidatePower : safeBasePower);

    if (safeCandidatePower <= safeBasePower)
        return safeBasePower;

    const auto safeMaxLiftDb =
        juce::jlimit (0.0f, 10.0f, std::isfinite (maxLiftDb) ? maxLiftDb : 0.0f);

    if (safeMaxLiftDb <= 0.0f)
        return safeBasePower;

    const auto baseMagnitude = std::sqrt (safeBasePower);
    const auto candidateMagnitude = std::sqrt (safeCandidatePower);

    const auto baseDb =
        juce::Decibels::gainToDecibels (baseMagnitude, -100.0f);

    const auto candidateDb =
        juce::Decibels::gainToDecibels (candidateMagnitude, -100.0f);

    const auto limitedDb =
        juce::jmin (candidateDb, baseDb + safeMaxLiftDb);

    const auto limitedMagnitude =
        juce::Decibels::decibelsToGain (limitedDb);

    const auto limitedPower = limitedMagnitude * limitedMagnitude;

    return juce::jlimit (safeBasePower,
        vqtLikeMaxDisplayPower,
        std::isfinite (limitedPower) ? limitedPower : safeBasePower);
}

AnalyzerEngine::VqtLikeDisplayBinStats
    AnalyzerEngine::getVqtLikeDisplayBinStats (
        size_t displayBinIndex) const noexcept
{
    VqtLikeDisplayBinStats result;

    if (displayBinIndex >= static_cast<size_t> (displayBinCount)
        || vqtLikeFilterBands.size() != static_cast<size_t> (vqtLikeAnalysisBandCount))
        return result;

    const auto displayDenominator =
        static_cast<float> (juce::jmax (1, displayBinCount - 1));

    const auto displayPosition =
        static_cast<float> (displayBinIndex) / displayDenominator;

    const auto displayBinWidth =
        1.0f / displayDenominator;

    auto weightedTonalMeanPower = 0.0f;
    auto weightedTonalPeakPower = 0.0f;
    auto tonalWeightedMaxPower = 0.0f;
    auto weightedTonalBandwidthHz = 0.0f;
    auto weightedTonalEffectiveQ = 0.0f;
    auto tonalWeightSum = 0.0f;
    auto weightedFastMeanPower = 0.0f;
    auto weightedFastPeakPower = 0.0f;
    auto fastWeightedMaxPower = 0.0f;
    auto weightedFastBandwidthHz = 0.0f;
    auto weightedFastEffectiveQ = 0.0f;
    auto fastWeightSum = 0.0f;
    auto analysisBandsUsed = 0;

    for (const auto& band : vqtLikeFilterBands)
    {
        if (!band.isConfigured
            || !std::isfinite (band.normalisedPosition)
            || !std::isfinite (band.centerFrequencyHz)
            || band.centerFrequencyHz <= 0.0f)
        {
            continue;
        }

        const auto distanceDisplayBins =
            std::abs ((band.normalisedPosition - displayPosition) / displayBinWidth);

        if (!std::isfinite (distanceDisplayBins)
            || distanceDisplayBins > vqtLikeAggregationRadiusDisplayBins)
        {
            continue;
        }

        const auto weight =
            1.0f
            - (distanceDisplayBins / vqtLikeAggregationRadiusDisplayBins);

        if (!std::isfinite (weight) || weight <= 0.0f)
            continue;

        const auto bandTonalMeanPower =
            juce::jlimit (0.0f,
                vqtLikeMaxDisplayPower,
                std::isfinite (band.power) ? band.power : 0.0f);

        const auto bandTonalPeakPower =
            juce::jlimit (0.0f,
                vqtLikeMaxDisplayPower,
                std::isfinite (band.peakPower) ? band.peakPower : bandTonalMeanPower);

        weightedTonalMeanPower += bandTonalMeanPower * weight;
        weightedTonalPeakPower += bandTonalPeakPower * weight;
        tonalWeightedMaxPower =
            juce::jmax (tonalWeightedMaxPower, bandTonalPeakPower * weight);
        weightedTonalBandwidthHz +=
            (std::isfinite (band.tonalEquivalentBandwidthHz)
                    ? band.tonalEquivalentBandwidthHz
                    : band.bandwidthHz)
            * weight;
        weightedTonalEffectiveQ +=
            (std::isfinite (band.effectiveQ) ? band.effectiveQ : 0.0f) * weight;
        tonalWeightSum += weight;

        if (band.hasFastLayer)
        {
            const auto bandFastMeanPower =
                juce::jlimit (0.0f,
                    vqtLikeMaxDisplayPower,
                    std::isfinite (band.fastPower) ? band.fastPower : 0.0f);

            const auto bandFastPeakPower =
                juce::jlimit (0.0f,
                    vqtLikeMaxDisplayPower,
                    std::isfinite (band.fastPeakPower)
                        ? band.fastPeakPower
                        : bandFastMeanPower);

            weightedFastMeanPower += bandFastMeanPower * weight;
            weightedFastPeakPower += bandFastPeakPower * weight;
            fastWeightedMaxPower =
                juce::jmax (fastWeightedMaxPower, bandFastPeakPower * weight);
            weightedFastBandwidthHz +=
                (std::isfinite (band.fastEquivalentBandwidthHz)
                        ? band.fastEquivalentBandwidthHz
                        : band.fastBandwidthHz)
                * weight;
            weightedFastEffectiveQ +=
                (std::isfinite (band.fastEffectiveQ) ? band.fastEffectiveQ : 0.0f)
                * weight;
            fastWeightSum += weight;
        }

        ++analysisBandsUsed;
    }

    if (tonalWeightSum <= 0.0f || analysisBandsUsed <= 0)
        return result;

    auto tonalMeanPower =
        juce::jlimit (0.0f,
            vqtLikeMaxDisplayPower,
            weightedTonalMeanPower / tonalWeightSum);

    auto tonalPeakPower =
        juce::jlimit (0.0f,
            vqtLikeMaxDisplayPower,
            weightedTonalPeakPower / tonalWeightSum);

    tonalPeakPower = juce::jmax (tonalPeakPower, tonalMeanPower);

    auto tonalBandwidthHz =
        juce::jmax (0.0f, weightedTonalBandwidthHz / tonalWeightSum);

    auto tonalEffectiveQ =
        juce::jmax (0.0f, weightedTonalEffectiveQ / tonalWeightSum);

    auto fastMeanPower = tonalMeanPower;
    auto fastPeakPower = tonalPeakPower;
    auto fastBandwidthHz = tonalBandwidthHz;
    auto fastEffectiveQ = tonalEffectiveQ;

    if (fastWeightSum > 0.0f)
    {
        fastMeanPower =
            juce::jlimit (0.0f,
                vqtLikeMaxDisplayPower,
                weightedFastMeanPower / fastWeightSum);

        fastPeakPower =
            juce::jlimit (0.0f,
                vqtLikeMaxDisplayPower,
                weightedFastPeakPower / fastWeightSum);

        fastPeakPower = juce::jmax (fastPeakPower, fastMeanPower);
        fastBandwidthHz =
            juce::jmax (0.0f, weightedFastBandwidthHz / fastWeightSum);
        fastEffectiveQ =
            juce::jmax (0.0f, weightedFastEffectiveQ / fastWeightSum);
    }

    auto centerFrequencyHz =
        logFrequencyAtNormalisedPosition (displayPosition,
            currentDisplayMinFrequencyHz,
            currentDisplayMaxFrequencyHz);

    if (displayBinCenterFrequenciesHz.size() == static_cast<size_t> (displayBinCount))
    {
        const auto displayCenterFrequencyHz =
            displayBinCenterFrequenciesHz[displayBinIndex];

        if (std::isfinite (displayCenterFrequencyHz) && displayCenterFrequencyHz > 0.0f)
            centerFrequencyHz = displayCenterFrequencyHz;
    }

    const auto peakShapeBlend =
        getVqtLikeAggregationPeakShapeBlendForFrequency (centerFrequencyHz);

    const auto tonalCalibrationTrim =
        vqtLikeTonalSineCalibrationTrim
        * getVqtLikeTonalCalibrationTrimForFrequency (centerFrequencyHz);

    const auto fastCalibrationTrim =
        vqtLikeFastSineCalibrationTrim
        * getVqtLikeFastCalibrationTrimForFrequency (centerFrequencyHz);

    const auto noiseDensityTrim =
        getVqtLikeNoiseDensityTrimForFrequency (centerFrequencyHz);

    tonalPeakPower =
        juce::jlimit (tonalMeanPower,
            vqtLikeMaxDisplayPower,
            tonalPeakPower
                + peakShapeBlend
                      * (juce::jmax (tonalPeakPower, tonalWeightedMaxPower)
                          - tonalPeakPower));

    fastPeakPower =
        juce::jlimit (fastMeanPower,
            vqtLikeMaxDisplayPower,
            fastPeakPower
                + peakShapeBlend
                      * (juce::jmax (fastPeakPower, fastWeightedMaxPower)
                          - fastPeakPower));

    if (tonalMeanPower < vqtLikeMinimumUsefulPower
        && tonalPeakPower < vqtLikeMinimumUsefulPower
        && fastMeanPower < vqtLikeMinimumUsefulPower
        && fastPeakPower < vqtLikeMinimumUsefulPower)
    {
        return result;
    }

    const auto livePeakBlend =
        getVqtLikePeakBlendForFrequency (centerFrequencyHz,
            vqtLikeLivePeakBlendLow,
            vqtLikeLivePeakBlendMid,
            vqtLikeLivePeakBlendHigh);

    const auto peakHoldPeakBlend =
        getVqtLikePeakBlendForFrequency (centerFrequencyHz,
            vqtLikePeakHoldPeakBlendLow,
            vqtLikePeakHoldPeakBlendMid,
            vqtLikePeakHoldPeakBlendHigh);

    const auto transientDetailBlend =
        getVqtLikeTransientDetailBlendForFrequency (centerFrequencyHz);

    const auto transientDetailMaxLiftDb =
        getVqtLikeTransientDetailMaxLiftDbForFrequency (centerFrequencyHz);

    const auto tonalLivePower =
        juce::jlimit (0.0f,
            vqtLikeMaxDisplayPower,
            tonalMeanPower + livePeakBlend * (tonalPeakPower - tonalMeanPower));

    const auto fastDetailPower = juce::jmax (fastPeakPower, fastMeanPower);

    const auto liveCandidatePower =
        juce::jmax (tonalLivePower,
            tonalLivePower
                + transientDetailBlend * (fastDetailPower - tonalLivePower));

    const auto livePower =
        juce::jlimit (0.0f,
            vqtLikeMaxDisplayPower,
            limitPowerLiftDb (tonalLivePower,
                liveCandidatePower,
                transientDetailMaxLiftDb));

    const auto peakHoldPowerBase =
        juce::jlimit (0.0f,
            vqtLikeMaxDisplayPower,
            tonalMeanPower + peakHoldPeakBlend * (tonalPeakPower - tonalMeanPower));

    const auto peakHoldTransientBlend =
        juce::jlimit (0.0f, 1.0f, transientDetailBlend * 1.25f);

    const auto peakHoldTransientMaxLiftDb =
        juce::jlimit (0.0f, 10.0f, transientDetailMaxLiftDb + 2.0f);

    const auto peakHoldCandidatePower =
        juce::jmax (peakHoldPowerBase,
            peakHoldPowerBase
                + peakHoldTransientBlend * (fastPeakPower - peakHoldPowerBase));

    const auto peakHoldPower =
        juce::jlimit (0.0f,
            vqtLikeMaxDisplayPower,
            limitPowerLiftDb (peakHoldPowerBase,
                peakHoldCandidatePower,
                peakHoldTransientMaxLiftDb));

    const auto vqtMetricDb = powerToAnalyzerDb (tonalMeanPower);
    const auto vqtLiveDb = powerToAnalyzerDb (livePower);

    result.metricStats = { tonalMeanPower, tonalMeanPower, 1 };
    result.liveVisualStats = { livePower, livePower, 1 };
    result.peakHoldVisualStats = { peakHoldPower, peakHoldPower, 1 };
    result.centerFrequencyHz = centerFrequencyHz;
    result.peakBlend = livePeakBlend;
    result.peakHoldBlend = peakHoldPeakBlend;
    result.analysisBandsUsed = analysisBandsUsed;
    result.analysisWeightSum = tonalWeightSum;
    result.tonalMeanPower = tonalMeanPower;
    result.tonalPeakPower = tonalPeakPower;
    result.fastMeanPower = fastMeanPower;
    result.fastPeakPower = fastPeakPower;
    result.transientDetailBlend = transientDetailBlend;
    result.transientDetailMaxLiftDb = transientDetailMaxLiftDb;
    result.tonalBandwidthHz = tonalBandwidthHz;
    result.fastBandwidthHz = fastBandwidthHz;
    result.tonalEffectiveQ = tonalEffectiveQ;
    result.fastEffectiveQ = fastEffectiveQ;
    result.peakShapeBlend = peakShapeBlend;
    result.tonalToFastMeanRatioDb =
        safePowerRatioDb (tonalMeanPower, fastMeanPower);
    result.tonalToFastPeakRatioDb =
        safePowerRatioDb (tonalPeakPower, fastPeakPower);
    result.liveLiftFromFastDb =
        safePowerRatioDb (livePower, tonalLivePower);
    result.peakHoldLiftFromFastDb =
        safePowerRatioDb (peakHoldPower, peakHoldPowerBase);
    result.tonalPeakToMeanDb =
        safePowerRatioDb (tonalPeakPower, tonalMeanPower);
    result.fastPeakToMeanDb =
        safePowerRatioDb (fastPeakPower, fastMeanPower);
    result.vqtMetricDb = vqtMetricDb;
    result.vqtLiveDb = vqtLiveDb;
    result.tonalCalibrationTrim = tonalCalibrationTrim;
    result.fastCalibrationTrim = fastCalibrationTrim;
    result.noiseDensityTrim = noiseDensityTrim;
    result.isConfigured = true;

    return result;
}

void AnalyzerEngine::generateVqtLikeValidationSignal (
    const VqtLikeValidationSignalSpec& spec,
    float sampleRate,
    std::vector<float>& outputBuffer) const
{
    outputBuffer.clear();

    if (!std::isfinite (sampleRate) || sampleRate <= 0.0f)
        return;

    const auto safeDurationSeconds =
        juce::jlimit (0.05f, 10.0f, std::isfinite (spec.durationSeconds) ? spec.durationSeconds : 1.0f);

    const auto numSamples =
        juce::jmax (1, static_cast<int> (std::ceil (safeDurationSeconds * sampleRate)));

    outputBuffer.assign (static_cast<size_t> (numSamples), 0.0f);

    const auto nyquist = sampleRate * 0.5f;
    const auto levelGain =
        juce::Decibels::decibelsToGain (
            juce::jlimit (-120.0f, 0.0f, std::isfinite (spec.levelDb) ? spec.levelDb : -18.0f));

    const auto clampFrequency = [nyquist] (float frequencyHz, float fallbackHz) noexcept {
        const auto safeFrequency =
            std::isfinite (frequencyHz) && frequencyHz > 0.0f ? frequencyHz : fallbackHz;

        return juce::jlimit (1.0f, nyquist * 0.95f, safeFrequency);
    };

    auto randomState = spec.randomSeed != 0u ? spec.randomSeed : 0x12345678u;
    const auto nextRandomBipolar = [&randomState]() noexcept {
        randomState = randomState * 1664525u + 1013904223u;
        const auto normalised =
            static_cast<float> ((randomState >> 8) & 0x00ffffffu)
            / static_cast<float> (0x00ffffffu);

        return normalised * 2.0f - 1.0f;
    };

    switch (spec.type)
    {
        case VqtLikeValidationSignalSpec::Type::sine:
        {
            const auto frequencyHz = clampFrequency (spec.frequencyHz, 1000.0f);
            const auto phaseAdvance =
                juce::MathConstants<float>::twoPi * frequencyHz / sampleRate;
            auto phase = 0.0f;

            for (auto& sample : outputBuffer)
            {
                sample = levelGain * std::sin (phase);
                phase += phaseAdvance;

                if (phase > juce::MathConstants<float>::twoPi)
                    phase -= juce::MathConstants<float>::twoPi;
            }

            break;
        }

        case VqtLikeValidationSignalSpec::Type::dualSine:
        {
            const auto firstFrequencyHz = clampFrequency (spec.frequencyHz, 80.0f);
            const auto secondFrequencyHz =
                clampFrequency (spec.secondFrequencyHz > 0.0f
                                    ? spec.secondFrequencyHz
                                    : spec.frequencyHz * 1.1f,
                    firstFrequencyHz * 1.1f);
            const auto firstPhaseAdvance =
                juce::MathConstants<float>::twoPi * firstFrequencyHz / sampleRate;
            const auto secondPhaseAdvance =
                juce::MathConstants<float>::twoPi * secondFrequencyHz / sampleRate;
            auto firstPhase = 0.0f;
            auto secondPhase = 0.0f;

            for (auto& sample : outputBuffer)
            {
                sample =
                    0.5f * levelGain
                    * (std::sin (firstPhase) + std::sin (secondPhase));

                firstPhase += firstPhaseAdvance;
                secondPhase += secondPhaseAdvance;

                if (firstPhase > juce::MathConstants<float>::twoPi)
                    firstPhase -= juce::MathConstants<float>::twoPi;

                if (secondPhase > juce::MathConstants<float>::twoPi)
                    secondPhase -= juce::MathConstants<float>::twoPi;
            }

            break;
        }

        case VqtLikeValidationSignalSpec::Type::whiteNoise:
        {
            for (auto& sample : outputBuffer)
                sample = levelGain * nextRandomBipolar();

            break;
        }

        case VqtLikeValidationSignalSpec::Type::pinkNoise:
        {
            auto b0 = 0.0f;
            auto b1 = 0.0f;
            auto b2 = 0.0f;

            for (auto& sample : outputBuffer)
            {
                const auto white = nextRandomBipolar();
                b0 = 0.99765f * b0 + white * 0.0990460f;
                b1 = 0.96300f * b1 + white * 0.2965164f;
                b2 = 0.57000f * b2 + white * 1.0526913f;

                const auto pink = (b0 + b1 + b2 + white * 0.1848f) * 0.20f;
                sample = levelGain * juce::jlimit (-1.0f, 1.0f, pink);
            }

            break;
        }

        case VqtLikeValidationSignalSpec::Type::logarithmicSweep:
        {
            const auto startHz = clampFrequency (spec.sweepStartHz, 20.0f);
            const auto endHz =
                juce::jmax (startHz + 1.0f, clampFrequency (spec.sweepEndHz, 20000.0f));
            const auto logRatio = std::log (endHz / startHz);
            auto phase = 0.0f;

            for (int sampleIndex = 0; sampleIndex < numSamples; ++sampleIndex)
            {
                const auto t =
                    static_cast<float> (sampleIndex)
                    / static_cast<float> (juce::jmax (1, numSamples - 1));
                const auto frequencyHz = startHz * std::exp (logRatio * t);
                phase += juce::MathConstants<float>::twoPi * frequencyHz / sampleRate;
                outputBuffer[static_cast<size_t> (sampleIndex)] = levelGain * std::sin (phase);

                if (phase > juce::MathConstants<float>::twoPi)
                    phase = std::fmod (phase, juce::MathConstants<float>::twoPi);
            }

            break;
        }
    }
}

AnalyzerEngine::VqtLikeValidationResult AnalyzerEngine::runVqtLikeValidationSignal (
    const VqtLikeValidationSignalSpec& spec,
    float sampleRate)
{
    VqtLikeValidationResult result;
    result.spec = spec;

    const auto validationSampleRate =
        std::isfinite (sampleRate) && sampleRate > 0.0f ? sampleRate : 48000.0f;

    const auto savedFilterBands = vqtLikeFilterBands;
    const auto savedFilterbankSampleRate = vqtLikeFilterbankSampleRate;
    const auto savedFilterbankMinFrequencyHz = vqtLikeFilterbankMinFrequencyHz;
    const auto savedFilterbankMaxFrequencyHz = vqtLikeFilterbankMaxFrequencyHz;
    const auto savedFilterbankNeedsReset = vqtLikeFilterbankNeedsReset;
    const auto savedCurrentSampleRate = currentSampleRate;
    const auto savedDisplayMinFrequencyHz = currentDisplayMinFrequencyHz;
    const auto savedDisplayMaxFrequencyHz = currentDisplayMaxFrequencyHz;
    const auto savedDisplayCenters = displayBinCenterFrequenciesHz;
    const auto savedFrameSummary = vqtLikeFrameSummary;

    currentSampleRate = validationSampleRate;
    currentDisplayMinFrequencyHz = AnalyzerFrequencyRange::minimumHz;
    currentDisplayMaxFrequencyHz =
        AnalyzerFrequencyRange::getMaximumHzForSampleRate (validationSampleRate);

    if (currentDisplayMaxFrequencyHz <= currentDisplayMinFrequencyHz)
        currentDisplayMaxFrequencyHz = currentDisplayMinFrequencyHz + 1.0f;

    if (displayBinCenterFrequenciesHz.size() != static_cast<size_t> (displayBinCount))
        displayBinCenterFrequenciesHz.resize (static_cast<size_t> (displayBinCount), 0.0f);

    const auto displayDenominator = static_cast<float> (juce::jmax (1, displayBinCount - 1));

    for (int i = 0; i < displayBinCount; ++i)
    {
        const auto normalisedPosition = static_cast<float> (i) / displayDenominator;
        displayBinCenterFrequenciesHz[static_cast<size_t> (i)] =
            logFrequencyAtNormalisedPosition (normalisedPosition,
                currentDisplayMinFrequencyHz,
                currentDisplayMaxFrequencyHz);
    }

    vqtLikeFilterbankSampleRate = 0.0f;
    vqtLikeFilterbankMinFrequencyHz = 0.0f;
    vqtLikeFilterbankMaxFrequencyHz = 0.0f;
    vqtLikeFilterbankNeedsReset = true;
    configureVqtLikeFilterbankIfNeeded();
    resetVqtLikeFilterbankState();

    std::vector<float> validationSignal;
    generateVqtLikeValidationSignal (spec, validationSampleRate, validationSignal);

    const auto validationHopSize =
        juce::jmax (1,
            juce::jmin ((1 << defaultFftOrder) / fftOverlapFactor,
                maximumFftHopSizeSamples));

    for (size_t offset = 0; offset < validationSignal.size();
        offset += static_cast<size_t> (validationHopSize))
    {
        const auto remainingSamples = validationSignal.size() - offset;
        const auto samplesThisBlock =
            static_cast<int> (juce::jmin (remainingSamples,
                static_cast<size_t> (validationHopSize)));
        const auto frameAdvanceSeconds =
            static_cast<float> (samplesThisBlock) / validationSampleRate;

        processVqtLikeFilterbankSamples (validationSignal.data() + offset,
            samplesThisBlock,
            frameAdvanceSeconds);
    }

    auto metricDbByDisplayBin =
        std::vector<float> (static_cast<size_t> (displayBinCount), -100.0f);

    auto peakBinIndex = -1;
    auto peakMetricDb = -100.0f;
    auto peakLiveDb = -100.0f;

    resetVqtLikeFrameSummary();

    for (int i = 0; i < displayBinCount; ++i)
    {
        const auto stats = getVqtLikeDisplayBinStats (static_cast<size_t> (i));
        accumulateVqtLikeFrameSummary (stats);

        if (!stats.isConfigured)
            continue;

        const auto metricDb = powerToAnalyzerDb (stats.metricStats.meanPower);
        const auto liveDb = displayBinPowerStatsToDb (stats.liveVisualStats);
        metricDbByDisplayBin[static_cast<size_t> (i)] = metricDb;

        if (metricDb > peakMetricDb)
        {
            peakMetricDb = metricDb;
            peakLiveDb = liveDb;
            peakBinIndex = i;
        }
    }

    finalizeVqtLikeFrameSummary();

    auto targetFrequencyHz = spec.frequencyHz;
    if (spec.type == VqtLikeValidationSignalSpec::Type::logarithmicSweep)
        targetFrequencyHz = std::sqrt (spec.sweepStartHz * spec.sweepEndHz);
    else if (spec.type == VqtLikeValidationSignalSpec::Type::whiteNoise
             || spec.type == VqtLikeValidationSignalSpec::Type::pinkNoise)
        targetFrequencyHz = 1000.0f;

    if (!std::isfinite (targetFrequencyHz) || targetFrequencyHz <= 0.0f)
        targetFrequencyHz = 1000.0f;

    result.targetFrequencyHz = targetFrequencyHz;
    result.expectedDb = juce::jlimit (-100.0f, 0.0f, spec.levelDb);

    if (spec.type == VqtLikeValidationSignalSpec::Type::dualSine)
        result.expectedDb = juce::jlimit (-100.0f, 0.0f, spec.levelDb - 6.0206f);

    if (peakBinIndex >= 0)
    {
        result.measuredPeakFrequencyHz =
            displayBinCenterFrequenciesHz[static_cast<size_t> (peakBinIndex)];
        result.measuredMetricDb = peakMetricDb;
        result.measuredLiveDb = peakLiveDb;
        result.metricErrorDb =
            juce::jlimit (-60.0f, 60.0f, result.measuredMetricDb - result.expectedDb);
        result.liveErrorDb =
            juce::jlimit (-60.0f, 60.0f, result.measuredLiveDb - result.expectedDb);

        const auto widthThresholdDb = peakMetricDb - 3.0f;
        auto leftBin = peakBinIndex;
        auto rightBin = peakBinIndex;

        while (leftBin > 0
               && metricDbByDisplayBin[static_cast<size_t> (leftBin - 1)] >= widthThresholdDb)
        {
            --leftBin;
        }

        while (rightBin + 1 < displayBinCount
               && metricDbByDisplayBin[static_cast<size_t> (rightBin + 1)] >= widthThresholdDb)
        {
            ++rightBin;
        }

        result.peakWidthBinsAboveMinus3Db =
            static_cast<float> (rightBin - leftBin + 1);

        const auto leftFrequencyHz = displayBinCenterFrequenciesHz[static_cast<size_t> (leftBin)];
        const auto rightFrequencyHz = displayBinCenterFrequenciesHz[static_cast<size_t> (rightBin)];
        result.peakWidthHzAboveMinus3Db =
            juce::jmax (0.0f, rightFrequencyHz - leftFrequencyHz);

        result.isValid = true;
    }

    result.averageMetricReferenceErrorDb =
        vqtLikeFrameSummary.averageMetricReferenceErrorDb;
    result.averageAbsMetricReferenceErrorDb =
        vqtLikeFrameSummary.averageAbsMetricReferenceErrorDb;
    result.maxAbsMetricReferenceErrorDb =
        vqtLikeFrameSummary.maxAbsMetricReferenceErrorDb;

    vqtLikeFilterBands = savedFilterBands;
    vqtLikeFilterbankSampleRate = savedFilterbankSampleRate;
    vqtLikeFilterbankMinFrequencyHz = savedFilterbankMinFrequencyHz;
    vqtLikeFilterbankMaxFrequencyHz = savedFilterbankMaxFrequencyHz;
    vqtLikeFilterbankNeedsReset = savedFilterbankNeedsReset;
    currentSampleRate = savedCurrentSampleRate;
    currentDisplayMinFrequencyHz = savedDisplayMinFrequencyHz;
    currentDisplayMaxFrequencyHz = savedDisplayMaxFrequencyHz;
    displayBinCenterFrequenciesHz = savedDisplayCenters;
    vqtLikeFrameSummary = savedFrameSummary;

    return result;
}

void AnalyzerEngine::runVqtLikeInternalCalibrationValidation()
{
    if (!enableVqtLikeInternalValidation)
        return;

    const auto validationSampleRate =
        currentSampleRate > 0.0 ? static_cast<float> (currentSampleRate) : 48000.0f;

    using ValidationType = VqtLikeValidationSignalSpec::Type;

    std::vector<VqtLikeValidationSignalSpec> specs;
    specs.reserve (9);

    auto makeSpec = [] (ValidationType type,
                        float frequencyHz,
                        float secondFrequencyHz,
                        float levelDb) {
        VqtLikeValidationSignalSpec spec;
        spec.type = type;
        spec.frequencyHz = frequencyHz;
        spec.secondFrequencyHz = secondFrequencyHz;
        spec.levelDb = levelDb;
        spec.durationSeconds = 1.0f;
        return spec;
    };

    specs.push_back (makeSpec (ValidationType::sine, 50.0f, 0.0f, -18.0f));
    specs.push_back (makeSpec (ValidationType::sine, 80.0f, 0.0f, -18.0f));
    specs.push_back (makeSpec (ValidationType::sine, 1000.0f, 0.0f, -18.0f));
    specs.push_back (makeSpec (ValidationType::sine, 10000.0f, 0.0f, -18.0f));
    specs.push_back (makeSpec (ValidationType::dualSine, 50.0f, 55.0f, -18.0f));
    specs.push_back (makeSpec (ValidationType::dualSine, 80.0f, 90.0f, -18.0f));
    specs.push_back (makeSpec (ValidationType::whiteNoise, 1000.0f, 0.0f, -24.0f));
    specs.push_back (makeSpec (ValidationType::pinkNoise, 1000.0f, 0.0f, -24.0f));

    auto sweepSpec = makeSpec (ValidationType::logarithmicSweep, 1000.0f, 0.0f, -24.0f);
    sweepSpec.sweepStartHz = 20.0f;
    sweepSpec.sweepEndHz = 20000.0f;
    specs.push_back (sweepSpec);

    lastVqtLikeValidationResults.clear();
    lastVqtLikeValidationResults.reserve (specs.size());

    for (const auto& spec : specs)
        lastVqtLikeValidationResults.push_back (runVqtLikeValidationSignal (spec, validationSampleRate));
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
    const auto requestedFrequencyDependentTunedResolution =
        requestedFrequencyDependentTunedResolutionEnabled.load (std::memory_order_relaxed);
    const auto requestedVqtLikeFilterbank =
        requestedVqtLikeFilterbankEnabled.load (std::memory_order_relaxed);

    if (requestedOrder == currentFftOrder
        && requestedFrequencyDependentResolution == currentFrequencyDependentResolutionEnabled
        && requestedFrequencyDependentTunedResolution == currentFrequencyDependentTunedResolutionEnabled
        && requestedVqtLikeFilterbank == currentVqtLikeFilterbankEnabled)
    {
        return;
    }

    currentFrequencyDependentResolutionEnabled = requestedFrequencyDependentResolution;
    currentFrequencyDependentTunedResolutionEnabled = requestedFrequencyDependentTunedResolution;
    currentVqtLikeFilterbankEnabled = requestedVqtLikeFilterbank;
    configureFft (requestedOrder);
    reset();
}

void AnalyzerEngine::start()
{
    if (!isThreadRunning())
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
    if (!hasFrame.load (std::memory_order_relaxed))
        return false;

    std::lock_guard<std::mutex> lock (latestSpectrumMutex);
    destination = latestSpectrumDb;

    return true;
}

bool AnalyzerEngine::copyLatestPeakHoldSpectrumDb (std::vector<float>& destination)
{
    if (!hasFrame.load (std::memory_order_relaxed))
        return false;

    std::lock_guard<std::mutex> lock (latestSpectrumMutex);
    destination = latestPeakHoldSpectrumDb;

    return true;
}

bool AnalyzerEngine::copyLatestRmsSpectrumDb (std::vector<float>& destination)
{
    if (!hasFrame.load (std::memory_order_relaxed))
        return false;

    std::lock_guard<std::mutex> lock (latestSpectrumMutex);
    destination = latestRmsSpectrumDb;

    return true;
}

bool AnalyzerEngine::copyLatestFrame (Frame& destination)
{
    if (!hasFrame.load (std::memory_order_relaxed))
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
    while (!threadShouldExit())
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

    if (!overlapBufferPrimed)
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

    if (currentFrequencyDependentResolutionEnabled && !currentVqtLikeFilterbankEnabled)
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

    // VQT-like prototype keeps NotePeaks on the existing FFT path for now.
    extractInstantaneousNotePeaksFromFftData (fftSizeForBlock);
    updateTrackedNotePeaks (frameAdvanceSeconds, currentPeakHoldDecayDbPerSecond);
    publishStableNotePeaks();

    updateDisplayBinFftRangesIfNeeded();

    if (currentVqtLikeFilterbankEnabled && displayAccumulationWarmStartRequested)
        resetVqtLikeFilterbankState();

    if (currentVqtLikeFilterbankEnabled)
    {
        configureVqtLikeFilterbankIfNeeded();
        processVqtLikeFilterbankSamples (newSamples,
            numNewSamples,
            frameAdvanceSeconds);
    }

    if (currentFrequencyDependentResolutionEnabled && !currentVqtLikeFilterbankEnabled)
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

    if (frequencyDependentTunedLowBandAlignmentAmounts.size()
        != static_cast<size_t> (displayBinCount))
    {
        return;
    }

    if (frequencyDependentTunedLowBandPreviousReferenceDb.size()
        != static_cast<size_t> (displayBinCount))
    {
        return;
    }

    if (frequencyDependentBinPolicySnapshots.size()
        != static_cast<size_t> (displayBinCount))
    {
        return;
    }

    if (currentVqtLikeFilterbankEnabled
        && vqtLikeFilterBands.size() != static_cast<size_t> (vqtLikeAnalysisBandCount))
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
        std::fill (frequencyDependentTunedLowBandAlignmentAmounts.begin(),
            frequencyDependentTunedLowBandAlignmentAmounts.end(),
            0.0f);
        std::fill (frequencyDependentTunedLowBandPreviousReferenceDb.begin(),
            frequencyDependentTunedLowBandPreviousReferenceDb.end(),
            -100.0f);
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

    const auto vqtLikeLiveAttackSmoothing =
        smoothingCoefficientForTimeConstant (frameAdvanceSeconds,
            vqtLikeLiveAttackTimeSeconds);

    const auto vqtLikeLiveReleaseSmoothing =
        smoothingCoefficientForTimeConstant (frameAdvanceSeconds,
            vqtLikeLiveReleaseTimeSeconds);

    const auto lowBassTailReleaseSmoothing =
        smoothingCoefficientForTimeConstant (
            frameAdvanceSeconds,
            getFrequencyDependentLowBassTailReleaseTimeSeconds());

    const auto veryHighReleaseSmoothing =
        smoothingCoefficientForTimeConstant (
            frameAdvanceSeconds,
            getFrequencyDependentVeryHighReleaseTimeSeconds());

    const auto transientAssistReleaseSmoothing =
        smoothingCoefficientForTimeConstant (
            frameAdvanceSeconds,
            getFrequencyDependentTransientAssistReleaseSeconds());

    const auto tunedLowBandAlignmentReleaseSmoothing =
        smoothingCoefficientForTimeConstant (
            frameAdvanceSeconds,
            frequencyDependentTunedLowBandAlignmentReleaseSeconds);

    resetFrequencyDependentPolicyFrameSummary();

    if (currentVqtLikeFilterbankEnabled)
        resetVqtLikeFrameSummary();

    auto energyFramePeakPower = 0.0f;
    const auto frequencyDependentSourceAvailability =
        getFrequencyDependentSourceAvailability();

    for (int i = 0; i < displayBinCount; ++i)
    {
        const auto index = static_cast<size_t> (i);

        if (currentVqtLikeFilterbankEnabled)
        {
            auto vqtBinStats =
                getVqtLikeDisplayBinStats (index);

            if (vqtBinStats.isConfigured
                && displayBinFftRanges.size() > index
                && fftData.size() >= static_cast<size_t> (fftSizeForBlock))
            {
                const auto& referenceRange = displayBinFftRanges[index];
                const auto referenceStats =
                    getFftBinPowerStatsForRange (fftData,
                        fftSizeForBlock,
                        referenceRange.firstBin,
                        referenceRange.lastBin,
                        referenceRange.leftBin,
                        referenceRange.rightBin);

                vqtBinStats.referenceMetricDb =
                    powerToAnalyzerDb (referenceStats.meanPower);
                vqtBinStats.referenceLiveDb =
                    displayBinPowerStatsToDb (referenceStats);
                vqtBinStats.vqtMetricDb =
                    powerToAnalyzerDb (vqtBinStats.metricStats.meanPower);
                vqtBinStats.vqtLiveDb =
                    displayBinPowerStatsToDb (vqtBinStats.liveVisualStats);
                vqtBinStats.metricReferenceErrorDb =
                    juce::jlimit (-60.0f,
                        60.0f,
                        vqtBinStats.vqtMetricDb - vqtBinStats.referenceMetricDb);
                vqtBinStats.liveReferenceErrorDb =
                    juce::jlimit (-60.0f,
                        60.0f,
                        vqtBinStats.vqtLiveDb - vqtBinStats.referenceLiveDb);
                vqtBinStats.hasReferenceComparison = true;
            }

            accumulateVqtLikeFrameSummary (vqtBinStats);

            const auto liveTargetDb =
                displayBinPowerStatsToDb (vqtBinStats.liveVisualStats);

            const auto peakHoldTargetDb =
                displayBinPowerStatsToDb (vqtBinStats.peakHoldVisualStats);

            const auto metricMeanPower =
                vqtBinStats.isConfigured
                    ? vqtBinStats.metricStats.meanPower
                    : 0.0f;

            energyFrameMeanPower[index] = metricMeanPower;
            energyFramePeakPower =
                juce::jmax (energyFramePeakPower, metricMeanPower);

            rawSpectrumDb[index] = liveTargetDb;

            if (shouldWarmStartDisplayAccumulation)
            {
                smoothedSpectrumDb[index] = liveTargetDb;
                peakHoldSpectrumDb[index] = peakHoldTargetDb;
                rmsPowerSpectrum[index] = metricMeanPower;
                continue;
            }

            const auto previousDb = smoothedSpectrumDb[index];
            const auto smoothing =
                liveTargetDb > previousDb
                    ? vqtLikeLiveAttackSmoothing
                    : vqtLikeLiveReleaseSmoothing;

            smoothedSpectrumDb[index] =
                previousDb + smoothing * (liveTargetDb - previousDb);

            if (peakHoldTargetDb > peakHoldSpectrumDb[index])
                peakHoldSpectrumDb[index] = peakHoldTargetDb;
            else
                peakHoldSpectrumDb[index] =
                    juce::jmax (-100.0f, peakHoldSpectrumDb[index] - decayPerFrame);

            rmsPowerSpectrum[index] =
                rmsPowerSpectrum[index]
                + rmsAlpha * (metricMeanPower - rmsPowerSpectrum[index]);

            continue;
        }

        const auto binStats = getFrequencyDependentBinStatsForDisplayBin (
            i,
            fftSizeForBlock,
            frequencyDependentSourceAvailability);

        const auto& transientReferenceBinPowerStats =
            binStats.hasTransientReferenceStats
                ? binStats.transientReferenceStats
                : binStats.mainStats;
        const auto& binPowerStats = binStats.compositeStats;

        auto liveVisualBinPowerStats = binPowerStats;
        const auto peakHoldVisualBinPowerStats = binPowerStats;
        auto liveReleaseBlendWeights = FrequencyDependentLiveReleaseBlendWeights {};
        auto tunedLowBandTransientAlignmentAmount = 0.0f;
        auto tunedLowBandOnsetConfidence = 0.0f;
        auto tunedLowBandAlignmentMaxLiftDb = 0.0f;

        if (currentFrequencyDependentResolutionEnabled)
        {
            auto& storedAssistAmount = frequencyDependentTransientAssistAmounts[index];

            const auto assistResult =
                applyFrequencyDependentLiveAssistForDisplayBin (
                    binPowerStats,
                    transientReferenceBinPowerStats,
                    binStats.centerFrequencyHz,
                    binStats.hasCenterFrequency,
                    binStats.blendWeights,
                    storedAssistAmount,
                    frameAdvanceSeconds,
                    transientAssistReleaseSmoothing);

            liveVisualBinPowerStats = assistResult.liveVisualStats;

            liveReleaseBlendWeights =
                getFrequencyDependentLiveReleaseBlendWeightsForDisplayBin (
                    binStats,
                    assistResult);

            if (currentFrequencyDependentTunedResolutionEnabled)
            {
                auto& storedAlignmentAmount =
                    frequencyDependentTunedLowBandAlignmentAmounts[index];

                auto& storedPreviousReferenceDb =
                    frequencyDependentTunedLowBandPreviousReferenceDb[index];

                const auto alignmentResult =
                    applyFrequencyDependentTunedLowBandTransientAlignmentForDisplayBin (
                        liveVisualBinPowerStats,
                        transientReferenceBinPowerStats,
                        binStats.centerFrequencyHz,
                        binStats.hasCenterFrequency,
                        binStats.blendWeights,
                        storedAlignmentAmount,
                        storedPreviousReferenceDb,
                        frameAdvanceSeconds,
                        tunedLowBandAlignmentReleaseSmoothing);

                liveVisualBinPowerStats = alignmentResult.liveVisualStats;
                tunedLowBandTransientAlignmentAmount =
                    alignmentResult.alignmentAmount;
                tunedLowBandOnsetConfidence =
                    alignmentResult.onsetConfidence;
                tunedLowBandAlignmentMaxLiftDb =
                    alignmentResult.maxLiftDb;
            }
        }
        else
        {
            frequencyDependentTunedLowBandAlignmentAmounts[index] = 0.0f;
            frequencyDependentTunedLowBandPreviousReferenceDb[index] = -100.0f;
        }

        if (!currentFrequencyDependentTunedResolutionEnabled)
        {
            frequencyDependentTunedLowBandAlignmentAmounts[index] = 0.0f;
            frequencyDependentTunedLowBandPreviousReferenceDb[index] = -100.0f;
            tunedLowBandTransientAlignmentAmount = 0.0f;
            tunedLowBandOnsetConfidence = 0.0f;
            tunedLowBandAlignmentMaxLiftDb = 0.0f;
        }

        auto policySnapshot =
            getFrequencyDependentBinPolicySnapshot (
                binStats,
                liveReleaseBlendWeights,
                tunedLowBandTransientAlignmentAmount,
                tunedLowBandOnsetConfidence,
                tunedLowBandAlignmentMaxLiftDb);

        frequencyDependentBinPolicySnapshots[index] = policySnapshot;
        accumulateFrequencyDependentPolicyFrameSummary (policySnapshot);

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

        const auto releaseSmoothing =
            blendFrequencyDependentLiveReleaseSmoothing (
                liveReleaseSmoothing,
                lowBassTailReleaseSmoothing,
                liveReleaseBlendWeights.lowBassTailReleaseBlend,
                veryHighReleaseSmoothing,
                liveReleaseBlendWeights.veryHighReleaseBlend);

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

    finalizeFrequencyDependentPolicyFrameSummary();

    if (currentVqtLikeFilterbankEnabled)
        finalizeVqtLikeFrameSummary();

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
        !hasFrame.load (std::memory_order_relaxed)
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

        if (!(currentDb >= previousDb
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

        instantaneousNotePeaks.push_back ({ frequencyHz,
            refinedDb,
            midiNote,
            pitchClass });
    }

    std::sort (instantaneousNotePeaks.begin(),
        instantaneousNotePeaks.end(),
        [] (const auto& first, const auto& second) {
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
        [] (const auto& first, const auto& second) {
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
                [&candidate] (const auto& trackedPeak) {
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
            [] (const auto& trackedPeak) {
                const auto unstablePeakIsGone =
                    !trackedPeak.hasBecomeStable
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
        if (!trackedPeak.hasBecomeStable
            || trackedPeak.heldDecibels <= notePeakMinAbsoluteDb + 0.001f
            || trackedPeak.frequencyHz < minNotePeakFrequencyHz
            || trackedPeak.frequencyHz > maxNotePeakFrequencyHz
            || trackedPeak.midiNote < 0
            || trackedPeak.pitchClass < 0)
        {
            continue;
        }

        currentNotePeaks.push_back ({ trackedPeak.frequencyHz,
            trackedPeak.heldDecibels,
            trackedPeak.midiNote,
            trackedPeak.pitchClass });
    }

    std::sort (currentNotePeaks.begin(),
        currentNotePeaks.end(),
        [] (const auto& first, const auto& second) {
            return first.decibels > second.decibels;
        });

    if (currentNotePeaks.size() > static_cast<size_t> (maxPublishedNotePeaks))
        currentNotePeaks.resize (static_cast<size_t> (maxPublishedNotePeaks));

    std::sort (currentNotePeaks.begin(),
        currentNotePeaks.end(),
        [] (const auto& first, const auto& second) {
            return first.frequencyHz < second.frequencyHz;
        });
}
