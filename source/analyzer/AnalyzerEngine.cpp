#include "AnalyzerEngine.h"

#include <algorithm>
#include <cmath>

AnalyzerEngine::AnalyzerEngine()
    : juce::Thread ("FullSpectrum Analyzer Engine"),
      forwardFFT (fftOrder),
      window (fftSize, juce::dsp::WindowingFunction<float>::hann, false)
{
    rawSpectrumDb.resize (displayBinCount, -100.0f);
    smoothedSpectrumDb.resize (displayBinCount, -100.0f);
    peakHoldSpectrumDb.resize (displayBinCount, -100.0f);
    rmsPowerSpectrum.resize (displayBinCount, 0.0f);

    latestSpectrumDb.resize (displayBinCount, -100.0f);
    latestPeakHoldSpectrumDb.resize (displayBinCount, -100.0f);
    latestRmsSpectrumDb.resize (displayBinCount, -100.0f);
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

        if (sourceFifo->getNumAvailableForReading() >= fftSize)
        {
            processOneFftBlock();
            continue;
        }

        wait (5);
    }
}

void AnalyzerEngine::processOneFftBlock()
{
    if (sourceFifo == nullptr)
        return;

    const auto numRead = sourceFifo->pop (timeDomainBlock.data(), fftSize);

    if (numRead != fftSize)
        return;

    std::fill (fftData.begin(), fftData.end(), 0.0f);
    std::copy (timeDomainBlock.begin(), timeDomainBlock.end(), fftData.begin());

    window.multiplyWithWindowingTable (fftData.data(), fftSize);

    forwardFFT.performFrequencyOnlyForwardTransform (fftData.data());

    const auto minFrequency = 20.0f;
    const auto maxFrequency = juce::jmax (
        minFrequency + 1.0f,
        juce::jmin (20000.0f, static_cast<float> (currentSampleRate * 0.5))
    );

    const auto decayPerFrame =
        peakHoldDecayDbPerSecond * static_cast<float> (fftSize)
        / static_cast<float> (currentSampleRate);

    const auto frameDurationSeconds =
        static_cast<float> (fftSize) / static_cast<float> (currentSampleRate);

    const auto rmsAlpha =
        1.0f - std::exp (-frameDurationSeconds / rmsTimeSeconds);

    for (int i = 0; i < displayBinCount; ++i)
    {
        const auto normalisedX =
            static_cast<float> (i) / static_cast<float> (displayBinCount - 1);

        const auto frequency =
            minFrequency * std::pow (maxFrequency / minFrequency, normalisedX);

        const auto fftBin =
            juce::jlimit (1,
                          (fftSize / 2) - 1,
                          juce::roundToInt (frequency * static_cast<float> (fftSize)
                                            / static_cast<float> (currentSampleRate)));

        const auto magnitude =
            (fftData[static_cast<size_t> (fftBin)] / static_cast<float> (fftSize)) * 2.0f;

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
            peakHoldSpectrumDb[index] = juce::jmax (-100.0f, peakHoldSpectrumDb[index] - decayPerFrame);

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