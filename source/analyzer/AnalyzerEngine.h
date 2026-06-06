#pragma once

#include "AnalyzerFifo.h"

#include <juce_core/juce_core.h>
#include <juce_dsp/juce_dsp.h>

#include <array>
#include <atomic>
#include <mutex>
#include <vector>
#include "AnalyzerFftSize.h"
#include <memory>

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

    void setRequestedFftOrder (int newFftOrder) noexcept;
    void setPeakHoldDecayDbPerSecond (float newDecayDbPerSecond) noexcept;

    bool copyLatestSpectrumDb (std::vector<float>& destination);
    bool copyLatestPeakHoldSpectrumDb (std::vector<float>& destination);
    bool copyLatestRmsSpectrumDb (std::vector<float>& destination);

private:
    void run() override;
    void processOneFftBlock();

    void updateFftSizeIfNeeded();
    void configureFft (int newFftOrder);

    static constexpr int minFftOrder = 10;
    static constexpr int defaultFftOrder = 11;
    static constexpr int maxFftOrder = 13;

    static constexpr int displayBinCount = 256;
    static constexpr float defaultPeakHoldDecayDbPerSecond = 8.0f;

    AnalyzerFifo* sourceFifo = nullptr;

    double currentSampleRate = 44100.0;

    int currentFftOrder = defaultFftOrder;
    int currentFftSize = analyzerFftSizeFromOrder (defaultFftOrder);

    std::atomic<int> requestedFftOrder { defaultFftOrder };

    std::unique_ptr<juce::dsp::FFT> forwardFFT;
    std::unique_ptr<juce::dsp::WindowingFunction<float>> window;

    std::vector<float> timeDomainBlock;
    std::vector<float> fftData;

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
    std::atomic<float> peakHoldDecayDbPerSecond { defaultPeakHoldDecayDbPerSecond };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AnalyzerEngine)
};