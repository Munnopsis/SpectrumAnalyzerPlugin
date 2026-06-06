#include "AnalyzerEngine.h"

#include <algorithm>
#include <cmath>

AnalyzerEngine::AnalyzerEngine()
    : juce::Thread ("FullSpectrum Analyzer Engine")
{
    rawSpectrumDb.resize (displayBinCount, -100.0f);
    smoothedSpectrumDb.resize (displayBinCount, -100.0f);
    peakHoldSpectrumDb.resize (displayBinCount, -100.0f);
    rmsPowerSpectrum.resize (displayBinCount, 0.0f);

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
    std::fill (timeDomainBlock.begin(), timeDomainBlock.end(), 0.0f);
    std::fill (fftData.begin(), fftData.end(), 0.0f);
    std::fill (rawSpectrumDb.begin(), rawSpectrumDb.end(), -100.0f);
    std::fill (smoothedSpectrumDb.begin(), smoothedSpectrumDb.end(), -100.0f);
    std::fill (peakHoldSpectrumDb.begin(), peakHoldSpectrumDb.end(), -100.0f);
    std::fill (rmsPowerSpectrum.begin(), rmsPowerSpectrum.end(), 0.0f);

    {
        std::lock_guard<std::mutex> lock (latestSpectrumMutex);
        std::fill (latestSpectrumDb.begin(), latestSpectrumDb.end(), -100.0f);
        std::fill (latestPeakHoldSpectrumDb.begin(), latestPeakHoldSpectrumDb.end(), -100.0f);
        std::fill (latestRmsSpectrumDb.begin(), latestRmsSpectrumDb.end(), -100.0f);
    }

    hasFrame.store (false, std::memory_order_relaxed);
}

void AnalyzerEngine::requestClearPeakHold() noexcept
{
    clearPeakHoldRequested.store (true, std::memory_order_relaxed);
}

void AnalyzerEngine::setPeakHoldDecayDbPerSecond (float newDecayDbPerSecond) noexcept
{
    peakHoldDecayDbPerSecond.store (
        juce::jlimit (0.0f, 60.0f, newDecayDbPerSecond),
        std::memory_order_relaxed);
}

void AnalyzerEngine::setRequestedFftOrder (int newFftOrder) noexcept
{
    requestedFftOrder.store (
        juce::jlimit (minFftOrder, maxFftOrder, newFftOrder),
        std::memory_order_relaxed);
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
    fftData.assign (static_cast<size_t> (currentFftSize * 2), 0.0f);
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

void AnalyzerEngine::run()
{
    while (! threadShouldExit())
    {
        if (sourceFifo == nullptr)
        {
            wait (20);
            continue;
        }

        updateFftSizeIfNeeded();

        if (sourceFifo->getNumAvailableForReading() >= currentFftSize)
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

    if (fftSizeForBlock <= 0)
        return;

    const auto numRead = sourceFifo->pop (timeDomainBlock.data(), fftSizeForBlock);

    if (numRead != fftSizeForBlock)
        return;

    std::fill (fftData.begin(), fftData.end(), 0.0f);
    std::copy (timeDomainBlock.begin(), timeDomainBlock.end(), fftData.begin());

    window->multiplyWithWindowingTable (
        fftData.data(),
        static_cast<size_t> (fftSizeForBlock));

    forwardFFT->performFrequencyOnlyForwardTransform (fftData.data());

    const auto minFrequency = 20.0f;
    const auto maxFrequency = juce::jmax (
        minFrequency + 1.0f,
        juce::jmin (20000.0f, static_cast<float> (currentSampleRate * 0.5))
    );

    const auto decayPerFrame =
        peakHoldDecayDbPerSecond * static_cast<float> (fftSizeForBlock)
        / static_cast<float> (currentSampleRate);

    const auto frameDurationSeconds =
        static_cast<float> (fftSizeForBlock) / static_cast<float> (currentSampleRate);

    const auto rmsAlpha =
        1.0f - std::exp (-frameDurationSeconds / rmsTimeSeconds);

    if (clearPeakHoldRequested.exchange (false, std::memory_order_relaxed))
    {
        std::fill (peakHoldSpectrumDb.begin(), peakHoldSpectrumDb.end(), -100.0f);

        std::lock_guard<std::mutex> lock (latestSpectrumMutex);
        std::fill (latestPeakHoldSpectrumDb.begin(), latestPeakHoldSpectrumDb.end(), -100.0f);
    }

    for (int i = 0; i < displayBinCount; ++i)
    {
        const auto normalisedX =
            static_cast<float> (i) / static_cast<float> (displayBinCount - 1);

        const auto frequency =
            minFrequency * std::pow (maxFrequency / minFrequency, normalisedX);

        const auto fftBin =
            juce::jlimit (1,
                          (fftSizeForBlock / 2) - 1,
                          juce::roundToInt (frequency * static_cast<float> (fftSizeForBlock)
                                            / static_cast<float> (currentSampleRate)));

        const auto magnitude =
            (fftData[static_cast<size_t> (fftBin)] / static_cast<float> (fftSizeForBlock)) * 2.0f;

        const auto db =
            juce::Decibels::gainToDecibels (magnitude, -100.0f);

        const auto targetDb = juce::jlimit (-100.0f, 0.0f, db);
        const auto index = static_cast<size_t> (i);

        rawSpectrumDb[index] = targetDb;

        const auto previousDb = smoothedSpectrumDb[index];
        const auto smoothing = targetDb > previousDb ? attackSmoothing : releaseSmoothing;

        smoothedSpectrumDb[index] = previousDb + smoothing * (targetDb - previousDb);

        if (targetDb > peakHoldSpectrumDb[index])
            peakHoldSpectrumDb[index] = targetDb;
        else
            peakHoldSpectrumDb[index] = juce::jmax (-100.0f,
                                                    peakHoldSpectrumDb[index] - decayPerFrame);

        const auto power = magnitude * magnitude;

        rmsPowerSpectrum[index] =
            rmsPowerSpectrum[index] + rmsAlpha * (power - rmsPowerSpectrum[index]);
    }

    {
        std::lock_guard<std::mutex> lock (latestSpectrumMutex);

        latestSpectrumDb = smoothedSpectrumDb;
        latestPeakHoldSpectrumDb = peakHoldSpectrumDb;

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