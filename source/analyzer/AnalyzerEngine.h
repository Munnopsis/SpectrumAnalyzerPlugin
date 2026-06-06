#pragma once

#include "AnalyzerFifo.h"
#include "AnalyzerFrequencyRange.h"

#include <juce_core/juce_core.h>
#include <juce_dsp/juce_dsp.h>

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

    struct NotePeak
    {
        float frequencyHz = 0.0f;
        float decibels = -100.0f;
        int midiNote = -1;
        int pitchClass = -1;
    };

    struct Frame
    {
        std::vector<float> liveDb;
        std::vector<float> peakHoldDb;
        std::vector<float> rmsDb;
        std::vector<NotePeak> notePeaks;
    };

    void prepare (double sampleRate, AnalyzerFifo& fifoToReadFrom);
    void reset();

    void start();
    void stop();

    void requestClearPeakHold() noexcept;
    void setRmsTimeSeconds (float newRmsTimeSeconds) noexcept;

    void setRequestedFftOrder (int newFftOrder) noexcept;
    void setPeakHoldDecayDbPerSecond (float newDecayDbPerSecond) noexcept;
    void setDisplayFrequencyRange (float minimumHz, float maximumHz) noexcept;

    bool copyLatestSpectrumDb (std::vector<float>& destination);
    bool copyLatestPeakHoldSpectrumDb (std::vector<float>& destination);
    bool copyLatestRmsSpectrumDb (std::vector<float>& destination);
    bool copyLatestFrame (Frame& destination);

private:
    struct TrackedNotePeak
    {
        float frequencyHz = 0.0f;
        float decibels = -100.0f;
        float heldDecibels = -100.0f;

        int midiNote = -1;
        int pitchClass = -1;

        int hitCount = 0;
        int framesSinceSeen = 0;
        float secondsSinceSeen = 0.0f;
        float confidence = 0.0f;
        bool hasBecomeStable = false;
    };

    struct DisplayBinFftRange
    {
        int firstBin = 1;
        int lastBin = 1;
    };

    void run() override;
    void processOneFftBlock();
    void updateFftSizeIfNeeded();
    void configureFft (int newFftOrder);
    void resetOverlapBuffer();
    void updateDisplayBinFftRangesIfNeeded();
    void publishLatestFrame();
    void handleClearPeakHoldRequest();
    int getFftHopSize() const noexcept;
    int frequencyToMidiNote (float frequencyHz) const noexcept;
    int midiNoteToPitchClass (int midiNote) const noexcept;
    void extractInstantaneousNotePeaksFromFftData (int fftSizeForBlock);
    void updateTrackedNotePeaks (float frameDurationSeconds,
                             float peakHoldDecayDbPerSecondForFrame);
    static float smoothingCoefficientForTimeConstant (float frameDurationSeconds,
                                                      float timeConstantSeconds) noexcept;
    void publishStableNotePeaks();

    static constexpr int minFftOrder = 10;
    static constexpr int defaultFftOrder = 11;
    static constexpr int maxFftOrder = 13;

    static constexpr int displayBinCount = 256;
    static constexpr int fftOverlapFactor = 4; // 4 = 75% overlap, hop size = fftSize / 4
    static constexpr float defaultPeakHoldDecayDbPerSecond = 8.0f;

    static constexpr float liveAttackTimeSeconds = 0.100f;
    static constexpr float liveReleaseTimeSeconds = 0.500f;
    static constexpr float latestFramePublishRateHz = 60.0f;
    static constexpr float defaultRmsTimeSeconds = 0.300f;

    static constexpr int maxInstantaneousNotePeaks = 60;
    static constexpr int maxPublishedNotePeaks = 16;

    static constexpr float minNotePeakFrequencyHz = 40.0f;
    static constexpr float maxNotePeakFrequencyHz = 5000.0f;

    static constexpr float notePeakRelativeThresholdDb = 36.0f;
    static constexpr float notePeakMinAbsoluteDb = -90.0f;
    static constexpr float notePeakMinProminenceDb = 2.5f;

    static constexpr int notePeakMinimumHitCount = 1;

    static constexpr float notePeakPublishAttackSeconds = 0.100f;
    static constexpr float notePeakReleaseSeconds = 0.650f;
    static constexpr float notePeakFrequencySmoothingSeconds = 0.080f;
    static constexpr float notePeakDbAttackSeconds = 0.080f;
    static constexpr float notePeakDbReleaseSeconds = 0.300f;

    static constexpr float notePeakPublishConfidence = 0.60f;
    static constexpr float notePeakRemoveConfidence = 0.02f;

    AnalyzerFifo* sourceFifo = nullptr;

    double currentSampleRate = 44100.0;
    float currentDisplayMinFrequencyHz = AnalyzerFrequencyRange::minimumHz;
    float currentDisplayMaxFrequencyHz = AnalyzerFrequencyRange::maximumHz;

    int currentFftOrder = defaultFftOrder;
    int currentFftSize = analyzerFftSizeFromOrder (defaultFftOrder);

    std::atomic<int> requestedFftOrder { defaultFftOrder };
    std::atomic<float> requestedDisplayMinFrequencyHz { AnalyzerFrequencyRange::minimumHz };
    std::atomic<float> requestedDisplayMaxFrequencyHz { AnalyzerFrequencyRange::maximumHz };

    std::unique_ptr<juce::dsp::FFT> forwardFFT;
    std::unique_ptr<juce::dsp::WindowingFunction<float>> window;

    std::vector<float> timeDomainBlock;
    std::vector<float> hopBuffer;
    std::vector<float> fftData;

    std::vector<DisplayBinFftRange> displayBinFftRanges;
    float displayBinRangeSampleRate = 0.0f;
    float displayBinRangeMinFrequencyHz = 0.0f;
    float displayBinRangeMaxFrequencyHz = 0.0f;
    int displayBinRangeFftSize = 0;

    bool overlapBufferPrimed = false;
    float secondsSinceLastFramePublish = 0.0f;

    std::vector<float> rawSpectrumDb;
    std::vector<float> smoothedSpectrumDb;
    std::vector<float> peakHoldSpectrumDb;
    std::vector<float> rmsPowerSpectrum;
    std::vector<float> notePeakBinDecibels;

    std::vector<float> latestSpectrumDb;
    std::vector<float> latestPeakHoldSpectrumDb;
    std::vector<float> latestRmsSpectrumDb;
    std::vector<NotePeak> instantaneousNotePeaks;
    std::vector<TrackedNotePeak> trackedNotePeaks;
    std::vector<NotePeak> currentNotePeaks;
    std::vector<NotePeak> latestNotePeaks;

    std::mutex latestSpectrumMutex;
    std::atomic<bool> hasFrame { false };
    std::atomic<bool> clearPeakHoldRequested { false };
    std::atomic<float> peakHoldDecayDbPerSecond { defaultPeakHoldDecayDbPerSecond };
    std::atomic<float> rmsTimeSeconds { defaultRmsTimeSeconds };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AnalyzerEngine)
};
