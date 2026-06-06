#pragma once

#include "AnalyzerFifo.h"

#include <juce_core/juce_core.h>
#include <juce_dsp/juce_dsp.h>

#include <array>
#include <atomic>
#include <mutex>
#include <vector>

class AnalyzerEngine : private juce::Thread
{
public:
    AnalyzerEngine();
    ~AnalyzerEngine() override;

    void prepare (double sampleRate, AnalyzerFifo& fifoToReadFrom);
    void reset();

    void start();
    void stop();

    void requestClearPeakHold() noexcept;

    bool copyLatestSpectrumDb (std::vector<float>& destination);
    bool copyLatestPeakHoldSpectrumDb (std::vector<float>& destination);
    bool copyLatestRmsSpectrumDb (std::vector<float>& destination);

private:
    void run() override;
    void processOneFftBlock();

    static constexpr int fftOrder = 11;
    static constexpr int fftSize = 1 << fftOrder;
    static constexpr int displayBinCount = 256;

    static constexpr float peakHoldDecayDbPerSecond = 8.0f;

    AnalyzerFifo* sourceFifo = nullptr;

    double currentSampleRate = 44100.0;

    juce::dsp::FFT forwardFFT;
    juce::dsp::WindowingFunction<float> window;

    std::array<float, fftSize> timeDomainBlock {};
    std::array<float, fftSize * 2> fftData {};

    std::vector<float> rawSpectrumDb;
    std::vector<float> smoothedSpectrumDb;
    std::vector<float> peakHoldSpectrumDb;
    std::vector<float> rmsPowerSpectrum;

    std::vector<float> latestSpectrumDb;
    std::vector<float> latestPeakHoldSpectrumDb;
    std::vector<float> latestRmsSpectrumDb;

    static constexpr float attackSmoothing = 0.35f;
    static constexpr float releaseSmoothing = 0.08f;
    static constexpr float rmsTimeSeconds = 0.300f;

    std::mutex latestSpectrumMutex;
    std::atomic<bool> hasFrame { false };
    std::atomic<bool> clearPeakHoldRequested { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AnalyzerEngine)
};