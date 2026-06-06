#include "AnalyzerEngine.h"
#include "AnalyzerFrequencyRange.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace
{
    constexpr float hannCoherentGain = 0.5f;

    struct DisplayBinPowerStats
    {
        float meanPower = 0.0f;
        float peakPower = 0.0f;
        int numBinsUsed = 0;
    };

    float logFrequencyAtNormalisedPosition (float normalisedPosition,
                                            float minFrequency,
                                            float maxFrequency) noexcept
    {
        const auto clampedPosition = juce::jlimit (0.0f, 1.0f, normalisedPosition);

        return minFrequency * std::pow (maxFrequency / minFrequency, clampedPosition);
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
        int lastBin) noexcept
    {
        DisplayBinPowerStats result;

        if (fftSize <= 0)
            return result;

        const auto maxAvailableBin = (fftSize / 2) - 1;

        if (maxAvailableBin < 1)
            return result;

        const auto clampedFirstBin = juce::jlimit (1, maxAvailableBin, firstBin);
        const auto clampedLastBin = juce::jlimit (1, maxAvailableBin, lastBin);

        if (clampedLastBin < clampedFirstBin)
            return result;

        auto powerSum = 0.0f;
        auto peakPower = 0.0f;
        auto numBinsUsed = 0;

        for (auto bin = clampedFirstBin; bin <= clampedLastBin; ++bin)
        {
            const auto amplitude =
                getCalibratedFftBinAmplitude (frequencyOnlyFftData, bin, fftSize);

            const auto power = amplitude * amplitude;

            powerSum += power;
            peakPower = juce::jmax (peakPower, power);
            ++numBinsUsed;
        }

        if (numBinsUsed <= 0)
            return result;

        result.meanPower = powerSum / static_cast<float> (numBinsUsed);
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
}

AnalyzerEngine::AnalyzerEngine()
    : juce::Thread ("FullSpectrum Analyzer Engine")
{
    rawSpectrumDb.resize (displayBinCount, -100.0f);
    smoothedSpectrumDb.resize (displayBinCount, -100.0f);
    peakHoldSpectrumDb.resize (displayBinCount, -100.0f);
    rmsPowerSpectrum.resize (displayBinCount, 0.0f);
    instantaneousNotePeaks.reserve (maxInstantaneousNotePeaks);
    trackedNotePeaks.reserve (maxInstantaneousNotePeaks);
    currentNotePeaks.reserve (maxPublishedNotePeaks);
    latestNotePeaks.reserve (maxPublishedNotePeaks);

    latestSpectrumDb.resize (displayBinCount, -100.0f);
    latestPeakHoldSpectrumDb.resize (displayBinCount, -100.0f);
    latestRmsSpectrumDb.resize (displayBinCount, -100.0f);

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

    configureFft (requestedFftOrder.load (std::memory_order_relaxed));
    reset();
}

void AnalyzerEngine::reset()
{
    resetOverlapBuffer();

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
    instantaneousNotePeaks.clear();
    trackedNotePeaks.clear();
    currentNotePeaks.clear();

    {
        std::lock_guard<std::mutex> lock (latestSpectrumMutex);
        std::fill (latestSpectrumDb.begin(), latestSpectrumDb.end(), -100.0f);
        std::fill (latestPeakHoldSpectrumDb.begin(), latestPeakHoldSpectrumDb.end(), -100.0f);
        std::fill (latestRmsSpectrumDb.begin(), latestRmsSpectrumDb.end(), -100.0f);
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
}

void AnalyzerEngine::resetOverlapBuffer()
{
    std::fill (timeDomainBlock.begin(), timeDomainBlock.end(), 0.0f);
    std::fill (hopBuffer.begin(), hopBuffer.end(), 0.0f);

    overlapBufferPrimed = false;
}

void AnalyzerEngine::resetDisplayAccumulationStateForRangeChange()
{
    std::fill (rawSpectrumDb.begin(), rawSpectrumDb.end(), -100.0f);
    std::fill (smoothedSpectrumDb.begin(), smoothedSpectrumDb.end(), -100.0f);
    std::fill (peakHoldSpectrumDb.begin(), peakHoldSpectrumDb.end(), -100.0f);
    std::fill (rmsPowerSpectrum.begin(), rmsPowerSpectrum.end(), 0.0f);

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
        resetDisplayAccumulationStateForRangeChange();

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

    for (int i = 0; i < displayBinCount; ++i)
    {
        const auto leftNormalised =
            (static_cast<float> (i) - 0.5f) / displayDenominator;

        const auto rightNormalised =
            (static_cast<float> (i) + 0.5f) / displayDenominator;

        const auto leftFrequency =
            logFrequencyAtNormalisedPosition (leftNormalised, minFrequency, maxFrequency);

        const auto rightFrequency =
            logFrequencyAtNormalisedPosition (rightNormalised, minFrequency, maxFrequency);

        const auto firstBin =
            juce::jlimit (1,
                          maxAvailableBin,
                          static_cast<int> (std::floor (leftFrequency
                                                        * static_cast<float> (fftSizeForRanges)
                                                        / sampleRateForRanges)));

        const auto lastBin =
            juce::jlimit (1,
                          maxAvailableBin,
                          static_cast<int> (std::ceil (rightFrequency
                                                       * static_cast<float> (fftSizeForRanges)
                                                       / sampleRateForRanges)));

        auto& range = displayBinFftRanges[static_cast<size_t> (i)];
        range.firstBin = firstBin;
        range.lastBin = juce::jmax (firstBin, lastBin);
    }

    displayBinRangeSampleRate = sampleRateForRanges;
    displayBinRangeFftSize = fftSizeForRanges;
    displayBinRangeMinFrequencyHz = currentDisplayMinFrequencyHz;
    displayBinRangeMaxFrequencyHz = currentDisplayMaxFrequencyHz;
}

int AnalyzerEngine::getFftHopSize() const noexcept
{
    return juce::jmax (1, currentFftSize / fftOverlapFactor);
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

            latestRmsSpectrumDb[i] =
                juce::jlimit (-100.0f,
                              0.0f,
                              juce::Decibels::gainToDecibels (rmsMagnitude, -100.0f));
        }
    }

    hasFrame.store (true, std::memory_order_relaxed);
}

void AnalyzerEngine::updateFftSizeIfNeeded()
{
    const auto requestedOrder = requestedFftOrder.load (std::memory_order_relaxed);

    if (requestedOrder == currentFftOrder)
        return;

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
            wait (20);
            continue;
        }

        handleClearPeakHoldRequest();
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

    if (! overlapBufferPrimed)
    {
        const auto numRead = sourceFifo->pop (timeDomainBlock.data(), fftSizeForBlock);

        if (numRead != fftSizeForBlock)
            return;

        overlapBufferPrimed = true;
    }
    else
    {
        const auto numRead = sourceFifo->pop (hopBuffer.data(), hopSize);

        if (numRead != hopSize)
            return;

        std::copy (timeDomainBlock.begin() + hopSize,
                   timeDomainBlock.begin() + fftSizeForBlock,
                   timeDomainBlock.begin());

        std::copy (hopBuffer.begin(),
                   hopBuffer.begin() + hopSize,
                   timeDomainBlock.begin() + (fftSizeForBlock - hopSize));
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

    if (displayBinFftRanges.size() != static_cast<size_t> (displayBinCount))
        return;

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

    for (int i = 0; i < displayBinCount; ++i)
    {
        const auto& displayBinRange =
            displayBinFftRanges[static_cast<size_t> (i)];

        const auto binPowerStats =
            getFftBinPowerStatsForRange (fftData,
                                         fftSizeForBlock,
                                         displayBinRange.firstBin,
                                         displayBinRange.lastBin);

        const auto displayPower =
            getPeakPreservingDisplayPower (binPowerStats);

        const auto displayMagnitude = std::sqrt (displayPower);

        const auto db =
            juce::Decibels::gainToDecibels (displayMagnitude, -100.0f);

        const auto targetDb = juce::jlimit (-100.0f, 0.0f, db);
        const auto index = static_cast<size_t> (i);

        rawSpectrumDb[index] = targetDb;

        const auto previousDb = smoothedSpectrumDb[index];

        const auto smoothing =
            targetDb > previousDb ? liveAttackSmoothing : liveReleaseSmoothing;

        smoothedSpectrumDb[index] =
            previousDb + smoothing * (targetDb - previousDb);

        if (targetDb > peakHoldSpectrumDb[index])
            peakHoldSpectrumDb[index] = targetDb;
        else
            peakHoldSpectrumDb[index] =
                juce::jmax (-100.0f, peakHoldSpectrumDb[index] - decayPerFrame);

        rmsPowerSpectrum[index] =
            rmsPowerSpectrum[index]
            + rmsAlpha * (binPowerStats.meanPower - rmsPowerSpectrum[index]);
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
