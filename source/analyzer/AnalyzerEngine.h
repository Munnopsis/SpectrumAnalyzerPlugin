#pragma once

#include "AnalyzerFifo.h"
#include "AnalyzerFrequencyRange.h"

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

    struct DisplayBinPowerStats
    {
        float meanPower = 0.0f;
        float peakPower = 0.0f;
        int numBinsUsed = 0;
    };

    struct NotePeak
    {
        float frequencyHz = 0.0f;
        float decibels = -100.0f;
        int midiNote = -1;
        int pitchClass = -1;
    };

    struct Frame
    {
        float dataMinFrequencyHz = AnalyzerFrequencyRange::minimumHz;
        float dataMaxFrequencyHz = AnalyzerFrequencyRange::maximumHz;
        std::vector<float> liveDb;
        std::vector<float> peakHoldDb;
        std::vector<float> rmsDb;
        std::vector<float> energyDb;
        std::vector<NotePeak> notePeaks;
    };

    void prepare (double sampleRate, AnalyzerFifo& fifoToReadFrom);
    void reset();

    void start();
    void stop();

    void requestClearPeakHold() noexcept;
    void requestClearEnergy() noexcept;
    void setRmsTimeSeconds (float newRmsTimeSeconds) noexcept;

    void setRequestedFftOrder (int newFftOrder) noexcept;
    void setFrequencyDependentResolutionEnabled (
        bool shouldUseFrequencyDependentResolution) noexcept;
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
        float leftBin = 1.0f;
        float rightBin = 1.0f;
        int firstBin = 1;
        int lastBin = 1;
    };

    struct FrequencyDependentFftSource
    {
        std::unique_ptr<juce::dsp::FFT> fft;
        std::unique_ptr<juce::dsp::WindowingFunction<float>> window;
        std::vector<float> timeDomainBlock;
        std::vector<float> fftData;
        std::vector<DisplayBinFftRange> displayBinFftRanges;
        int fftOrder = 0;
        int fftSize = 0;
        float nominalWindowSeconds = 0.0f;
        float rangeSampleRate = 0.0f;
        float rangeMinFrequencyHz = 0.0f;
        float rangeMaxFrequencyHz = 0.0f;
        int samplesCollected = 0;
        bool hasValidFftData = false;
    };

    struct FrequencyDependentBlendWeights
    {
        float midBassBlend = 0.0f;
        float mainBlend = 0.0f;
        float highBlend = 0.0f;
        float veryHighBlend = 0.0f;
        float lowBassTailReleaseBlend = 0.0f;
    };

    struct FrequencyDependentBinStats
    {
        DisplayBinPowerStats mainStats;
        DisplayBinPowerStats compositeStats;
        DisplayBinPowerStats transientReferenceStats;
        FrequencyDependentBlendWeights blendWeights;
        float centerFrequencyHz = 0.0f;
        bool hasCenterFrequency = false;
        bool hasTransientReferenceStats = false;
        bool hasBlendWeights = false;
        bool usedBassComposite = false;
        bool usedMidBassComposite = false;
        bool usedMainComposite = false;
        bool usedHighComposite = false;
        bool usedVeryHighComposite = false;
    };

    struct FrequencyDependentLowCompositeResult
    {
        DisplayBinPowerStats lowCompositeStats;
        DisplayBinPowerStats transientReferenceStats;
        bool hasLowCompositeStats = false;
        bool hasTransientReferenceStats = false;
        bool usedBassComposite = false;
        bool usedMidBassComposite = false;
    };

    struct FrequencyDependentHighCompositeResult
    {
        DisplayBinPowerStats compositeStats;
        bool usedHighComposite = false;
        bool usedVeryHighComposite = false;
    };

    struct FrequencyDependentLiveAssistResult
    {
        DisplayBinPowerStats liveVisualStats;
        float assistAmount = 0.0f;
        float lowBassTailReleaseBlend = 0.0f;
    };

    enum class FrequencyDependentSourceRole
    {
        bass,
        midBass,
        high,
        veryHigh
    };

    struct FrequencyDependentSourceDescriptor
    {
        FrequencyDependentSourceRole role = FrequencyDependentSourceRole::bass;
        FrequencyDependentFftSource* source = nullptr;
        int fftOrder = 0;
        int fftSize = 0;
    };

    struct FrequencyDependentSourceAvailability
    {
        bool canUseBass = false;
        bool canUseMidBass = false;
        bool canUseHigh = false;
        bool canUseVeryHigh = false;
    };

    void run() override;
    void processOneFftBlock();
    void updateFftSizeIfNeeded();
    std::array<FrequencyDependentSourceDescriptor, 4>
        getFrequencyDependentSourceDescriptors() noexcept;
    bool canUseFrequencyDependentSource (const FrequencyDependentFftSource& source,
        int fftSize) const noexcept;
    FrequencyDependentSourceAvailability
        getFrequencyDependentSourceAvailability() const noexcept;

    DisplayBinPowerStats getFrequencyDependentSourceStatsForDisplayBin (
        const FrequencyDependentFftSource& source,
        int fftSize,
        size_t displayBinIndex) const noexcept;

    void configureFft (int newFftOrder);
    void resetOverlapBuffer();
    void configureFrequencyDependentFftSource (FrequencyDependentFftSource& source,
                                               int fftOrder,
                                               int fftSize);
    void resetFrequencyDependentFftSource (FrequencyDependentFftSource& source);
    void appendSamplesToFrequencyDependentFftSource (FrequencyDependentFftSource& source,
                                                     const float* samples,
                                                     int numSamples,
                                                     int fftSize);
    void processFrequencyDependentFftSourceIfReady (FrequencyDependentFftSource& source,
                                                    int fftSize);
    void updateFrequencyDependentSourceBinFftRangesIfNeeded (
        FrequencyDependentFftSource& source,
        int fftSize);
    float getFrequencyDependentMidBassBlendForFrequency (float frequencyHz) const noexcept;
    float getFrequencyDependentMainBlendForFrequency (float frequencyHz) const noexcept;
    float getFrequencyDependentHighBlendForFrequency (float frequencyHz) const noexcept;
    float getFrequencyDependentVeryHighBlendForFrequency (
        float frequencyHz) const noexcept;
    FrequencyDependentBlendWeights getFrequencyDependentBlendWeightsForFrequency (
        float frequencyHz) const noexcept;
    FrequencyDependentLowCompositeResult getFrequencyDependentLowCompositeForDisplayBin (
        size_t displayBinIndex,
        float centerFrequencyHz,
        const DisplayBinPowerStats& mainStats,
        const FrequencyDependentSourceAvailability& sourceAvailability,
        const FrequencyDependentBlendWeights& blendWeights) const;
    FrequencyDependentHighCompositeResult applyFrequencyDependentHighBlendForDisplayBin (
        size_t displayBinIndex,
        float centerFrequencyHz,
        const DisplayBinPowerStats& baseStats,
        const FrequencyDependentSourceAvailability& sourceAvailability,
        const FrequencyDependentBlendWeights& blendWeights) const;
    FrequencyDependentBinStats getFrequencyDependentBinStatsForDisplayBin (
        int displayBinIndex,
        int fftSizeForBlock,
        const FrequencyDependentSourceAvailability& sourceAvailability) const;
    DisplayBinPowerStats blendDisplayBinPowerStats (const DisplayBinPowerStats& bassStats,
                                                    const DisplayBinPowerStats& mainStats,
                                                    float mainBlend) const noexcept;
    DisplayBinPowerStats applyFrequencyDependentTransientAssist (
        const DisplayBinPowerStats& frequencyDependentStats,
        const DisplayBinPowerStats& mainStats,
        float centerFrequencyHz,
        float assistAmount) const noexcept;
    FrequencyDependentLiveAssistResult applyFrequencyDependentLiveAssistForDisplayBin (
        const DisplayBinPowerStats& compositeStats,
        const DisplayBinPowerStats& transientReferenceStats,
        float centerFrequencyHz,
        bool hasCenterFrequency,
        const FrequencyDependentBlendWeights& blendWeights,
        float& storedAssistAmount,
        float frameAdvanceSeconds,
        float transientAssistReleaseSmoothing) const noexcept;
    void requestDisplayAccumulationWarmStartForRangeChange() noexcept;
    void updateDisplayBinFftRangesIfNeeded();
    void publishLatestFrame();
    void handleClearPeakHoldRequest();
    void handleClearEnergyRequest();
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
    static constexpr int maxFftOrder = 15;

    static constexpr int displayBinCount = 256;
    static constexpr int fftOverlapFactor = 4; // 4 = 75% overlap, hop size = fftSize / 4
    static constexpr int maximumFftHopSizeSamples = 2048;
    static constexpr int frequencyDependentMainFftOrder = 13;
    static constexpr int frequencyDependentMainFftSize = 1 << frequencyDependentMainFftOrder;
    static constexpr int frequencyDependentBassFftOrder = 15;
    static constexpr int frequencyDependentBassFftSize = 1 << frequencyDependentBassFftOrder;
    static constexpr int frequencyDependentMidBassFftOrder = 14;
    static constexpr int frequencyDependentMidBassFftSize = 1 << frequencyDependentMidBassFftOrder;
    static constexpr int frequencyDependentHighFftOrder = 12;
    static constexpr int frequencyDependentHighFftSize = 1 << frequencyDependentHighFftOrder;
    static constexpr int frequencyDependentVeryHighFftOrder = 11;
    static constexpr int frequencyDependentVeryHighFftSize = 1 << frequencyDependentVeryHighFftOrder;
    static constexpr float defaultPeakHoldDecayDbPerSecond = 8.0f;
    static constexpr float frequencyDependentDeepBassOnlyMaxHz = 60.0f;
    static constexpr float frequencyDependentMidBassOnlyMinHz = 140.0f;
    static constexpr float frequencyDependentBassOnlyMaxHz = 160.0f;
    static constexpr float frequencyDependentMainOnlyMinHz = 320.0f;
    static constexpr float frequencyDependentMainOnlyMaxHz = 3000.0f;
    static constexpr float frequencyDependentHighOnlyMinHz = 6000.0f;
    static constexpr float frequencyDependentVeryHighOnlyMinHz = 12000.0f;
    static constexpr float frequencyDependentTransientAssistMaxHz = 320.0f;
    static constexpr float frequencyDependentTransientAssistMinRiseDb = 4.0f;
    static constexpr float frequencyDependentTransientAttackBlend = 0.55f;
    static constexpr float frequencyDependentTransientTailSuppressBlend = 0.85f;
    static constexpr float frequencyDependentTransientTailSuppressMinExcessDb = 2.0f;
    static constexpr float frequencyDependentTransientAssistReleaseSeconds = 0.120f;

    static constexpr float liveAttackTimeSeconds = 0.100f;
    static constexpr float liveReleaseTimeSeconds = 0.500f;
    static constexpr float frequencyDependentLowBassTailReleaseTimeSeconds = 0.180f;
    static constexpr float frequencyDependentVeryHighReleaseTimeSeconds = 0.220f;
    static constexpr float latestFramePublishRateHz = 60.0f;
    static constexpr float defaultRmsTimeSeconds = 0.300f;
    static constexpr float energyAveragingWindowSeconds = 20.0f;
    static constexpr float energyActivityThresholdDb = -90.0f;

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
    bool currentFrequencyDependentResolutionEnabled = false;

    std::atomic<int> requestedFftOrder { defaultFftOrder };
    std::atomic<bool> requestedFrequencyDependentResolutionEnabled { false };
    std::atomic<float> requestedDisplayMinFrequencyHz { AnalyzerFrequencyRange::minimumHz };
    std::atomic<float> requestedDisplayMaxFrequencyHz { AnalyzerFrequencyRange::maximumHz };

    std::unique_ptr<juce::dsp::FFT> forwardFFT;
    std::unique_ptr<juce::dsp::WindowingFunction<float>> window;

    std::vector<float> timeDomainBlock;
    std::vector<float> hopBuffer;
    std::vector<float> fftData;

    std::vector<DisplayBinFftRange> displayBinFftRanges;
    std::vector<float> displayBinCenterFrequenciesHz;
    float displayBinRangeSampleRate = 0.0f;
    float displayBinRangeMinFrequencyHz = 0.0f;
    float displayBinRangeMaxFrequencyHz = 0.0f;
    int displayBinRangeFftSize = 0;
    FrequencyDependentFftSource frequencyDependentBassPath;
    FrequencyDependentFftSource frequencyDependentMidBassPath;
    FrequencyDependentFftSource frequencyDependentHighPath;
    FrequencyDependentFftSource frequencyDependentVeryHighPath;

    bool overlapBufferPrimed = false;
    float secondsSinceLastFramePublish = 0.0f;
    bool displayAccumulationWarmStartRequested = false;
    float energyAccumulatedActiveSeconds = 0.0f;
    std::vector<float> frequencyDependentTransientAssistAmounts;

    std::vector<float> rawSpectrumDb;
    std::vector<float> smoothedSpectrumDb;
    std::vector<float> peakHoldSpectrumDb;
    std::vector<float> rmsPowerSpectrum;
    std::vector<float> energyPowerSpectrum;
    std::vector<float> energyFrameMeanPower;
    std::vector<float> notePeakBinDecibels;

    std::vector<float> latestSpectrumDb;
    std::vector<float> latestPeakHoldSpectrumDb;
    std::vector<float> latestRmsSpectrumDb;
    std::vector<float> latestEnergySpectrumDb;
    float latestFrameMinFrequencyHz = AnalyzerFrequencyRange::minimumHz;
    float latestFrameMaxFrequencyHz = AnalyzerFrequencyRange::maximumHz;
    std::vector<NotePeak> instantaneousNotePeaks;
    std::vector<TrackedNotePeak> trackedNotePeaks;
    std::vector<NotePeak> currentNotePeaks;
    std::vector<NotePeak> latestNotePeaks;

    std::mutex latestSpectrumMutex;
    std::atomic<bool> hasFrame { false };
    std::atomic<bool> clearPeakHoldRequested { false };
    std::atomic<bool> clearEnergyRequested { false };
    std::atomic<float> peakHoldDecayDbPerSecond { defaultPeakHoldDecayDbPerSecond };
    std::atomic<float> rmsTimeSeconds { defaultRmsTimeSeconds };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AnalyzerEngine)
};
