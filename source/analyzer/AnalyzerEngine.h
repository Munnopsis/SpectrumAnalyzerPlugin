#pragma once

#include "AnalyzerFifo.h"
#include "AnalyzerFrequencyRange.h"

#include <juce_core/juce_core.h>
#include <juce_dsp/juce_dsp.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>
#include "AnalyzerFftSize.h"

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

    void prepare(double sampleRate, AnalyzerFifo& fifoToReadFrom);
    void reset();

    void start();
    void stop();

    void requestClearPeakHold() noexcept;
    void requestClearEnergy() noexcept;
    void setRmsTimeSeconds(float newRmsTimeSeconds) noexcept;

    void setRequestedFftOrder(int newFftOrder) noexcept;
    void setFrequencyDependentResolutionEnabled(
        bool shouldUseFrequencyDependentResolution) noexcept;
    void setFrequencyDependentTunedResolutionEnabled(bool shouldUseFrequencyDependentTunedResolution) noexcept;
    void setVqtLikeFilterbankEnabled(bool shouldUseVqtLikeFilterbank) noexcept;
    void setPeakHoldDecayDbPerSecond(float newDecayDbPerSecond) noexcept;
    void setDisplayFrequencyRange(float minimumHz, float maximumHz) noexcept;

    bool copyLatestSpectrumDb(std::vector<float>& destination);
    bool copyLatestPeakHoldSpectrumDb(std::vector<float>& destination);
    bool copyLatestRmsSpectrumDb(std::vector<float>& destination);
    bool copyLatestFrame(Frame& destination);

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
        bool usedFrequencyDependentSourceComposite = false;
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

    struct FrequencyDependentTunedLowBandAlignmentResult
    {
        DisplayBinPowerStats liveVisualStats;
        float alignmentAmount = 0.0f;
        float onsetConfidence = 0.0f;
        float maxLiftDb = 0.0f;
    };

    struct FrequencyDependentLiveReleaseBlendWeights
    {
        float lowBassTailReleaseBlend = 0.0f;
        float veryHighReleaseBlend = 0.0f;
    };

    enum class FrequencyDependentPolicyBand
    {
        none,
        bass,
        bassToMidBass,
        midBassToMain,
        main,
        mainToHigh,
        highToVeryHigh,
        veryHigh
    };

    struct FrequencyDependentBinPolicySnapshot
    {
        float centerFrequencyHz = 0.0f;

        FrequencyDependentBlendWeights blendWeights;
        FrequencyDependentLiveReleaseBlendWeights liveReleaseBlendWeights;

        bool hasCenterFrequency = false;
        bool hasBlendWeights = false;

        FrequencyDependentPolicyBand policyBand = FrequencyDependentPolicyBand::none;
        float tunedLowBandTransientAlignmentAmount = 0.0f;
        float tunedLowBandOnsetConfidence = 0.0f;
        float tunedLowBandAlignmentMaxLiftDb = 0.0f;

        bool usesLowBassFastRelease = false;
        bool usesVeryHighFastRelease = false;
        bool usesTunedLowBandTransientAlignment = false;
        bool hasTunedLowBandOnsetConfidence = false;
        bool isTransitionBand = false;

        bool usedBassComposite = false;
        bool usedMidBassComposite = false;
        bool usedMainComposite = false;
        bool usedHighComposite = false;
        bool usedVeryHighComposite = false;
        bool usedFrequencyDependentSourceComposite = false;
    };

    struct FrequencyDependentPolicyFrameSummary
    {
        int totalBins = 0;

        int noneBins = 0;
        int bassBins = 0;
        int bassToMidBassBins = 0;
        int midBassToMainBins = 0;
        int mainBins = 0;
        int mainToHighBins = 0;
        int highToVeryHighBins = 0;
        int veryHighBins = 0;

        int binsUsingFrequencyDependentSources = 0;
        int binsUsingLowBassFastRelease = 0;
        int binsUsingVeryHighFastRelease = 0;
        int binsUsingTunedLowBandTransientAlignment = 0;
        int binsWithTunedLowBandOnsetConfidence = 0;
        int transitionBins = 0;

        int classifiedBins = 0;
        bool hasConsistentBinCounts = true;

        float noneRatio = 0.0f;
        float bassRatio = 0.0f;
        float bassToMidBassRatio = 0.0f;
        float midBassToMainRatio = 0.0f;
        float mainRatio = 0.0f;
        float mainToHighRatio = 0.0f;
        float highToVeryHighRatio = 0.0f;
        float veryHighRatio = 0.0f;

        float frequencyDependentSourceRatio = 0.0f;
        float lowBassFastReleaseRatio = 0.0f;
        float veryHighFastReleaseRatio = 0.0f;
        float tunedLowBandTransientAlignmentRatio = 0.0f;
        float tunedLowBandOnsetConfidenceRatio = 0.0f;
        float transitionRatio = 0.0f;
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

    struct VqtLikeFilterBand
    {
        float centerFrequencyHz = 0.0f;
        float bandwidthHz = 0.0f;
        float effectiveQ = 0.0f;
        float normalisedPosition = 0.0f;
        float leftDisplayBin = 0.0f;
        float rightDisplayBin = 0.0f;
        float b0 = 0.0f;
        float b1 = 0.0f;
        float b2 = 0.0f;
        float a1 = 0.0f;
        float a2 = 0.0f;
        float z1 = 0.0f;
        float z2 = 0.0f;
        float power = 0.0f;
        float lastFramePower = 0.0f;
        float peakPower = 0.0f;
        float lastFramePeakPower = 0.0f;
        float calibrationGain = 1.0f;
        float noiseFloorPower = 0.0f;
        float tonalCenterGain = 1.0f;
        float fastCenterGain = 1.0f;
        float tonalEquivalentBandwidthHz = 0.0f;
        float fastEquivalentBandwidthHz = 0.0f;
        float tonalCalibrationPowerGain = 1.0f;
        float fastCalibrationPowerGain = 1.0f;
        float fastBandwidthHz = 0.0f;
        float fastEffectiveQ = 0.0f;
        float fastB0 = 0.0f;
        float fastB1 = 0.0f;
        float fastB2 = 0.0f;
        float fastA1 = 0.0f;
        float fastA2 = 0.0f;
        float fastZ1 = 0.0f;
        float fastZ2 = 0.0f;
        float fastPower = 0.0f;
        float fastPeakPower = 0.0f;
        float lastFrameFastPower = 0.0f;
        float lastFrameFastPeakPower = 0.0f;
        float fastCalibrationGain = 1.0f;
        bool isConfigured = false;
        bool hasFastLayer = false;
    };

    struct VqtLikeDisplayBinStats
    {
        DisplayBinPowerStats metricStats;
        DisplayBinPowerStats liveVisualStats;
        DisplayBinPowerStats peakHoldVisualStats;

        float centerFrequencyHz = 0.0f;
        float peakBlend = 0.0f;
        float peakHoldBlend = 0.0f;
        int analysisBandsUsed = 0;
        float analysisWeightSum = 0.0f;
        float tonalMeanPower = 0.0f;
        float tonalPeakPower = 0.0f;
        float fastMeanPower = 0.0f;
        float fastPeakPower = 0.0f;
        float transientDetailBlend = 0.0f;
        float transientDetailMaxLiftDb = 0.0f;
        float tonalBandwidthHz = 0.0f;
        float fastBandwidthHz = 0.0f;
        float tonalEffectiveQ = 0.0f;
        float fastEffectiveQ = 0.0f;
        float peakShapeBlend = 0.0f;
        float tonalToFastMeanRatioDb = 0.0f;
        float tonalToFastPeakRatioDb = 0.0f;
        float liveLiftFromFastDb = 0.0f;
        float peakHoldLiftFromFastDb = 0.0f;
        float tonalPeakToMeanDb = 0.0f;
        float fastPeakToMeanDb = 0.0f;
        float referenceMetricDb = -100.0f;
        float referenceLiveDb = -100.0f;
        float vqtMetricDb = -100.0f;
        float vqtLiveDb = -100.0f;
        float metricReferenceErrorDb = 0.0f;
        float liveReferenceErrorDb = 0.0f;
        float tonalCalibrationTrim = 1.0f;
        float fastCalibrationTrim = 1.0f;
        float noiseDensityTrim = 1.0f;

        bool isConfigured = false;
        bool hasReferenceComparison = false;
    };

    struct VqtLikeFrameSummary
    {
        int configuredAnalysisBands = 0;
        int displayBinsConfigured = 0;

        float configuredAnalysisBandRatio = 0.0f;
        float displayBinConfiguredRatio = 0.0f;

        float averageAnalysisBandsUsed = 0.0f;
        float averageTonalBandwidthHz = 0.0f;
        float averageFastBandwidthHz = 0.0f;
        float averageTonalEffectiveQ = 0.0f;
        float averageFastEffectiveQ = 0.0f;
        float averageTonalToFastMeanRatioDb = 0.0f;
        float averageTonalToFastPeakRatioDb = 0.0f;
        float averageLiveLiftFromFastDb = 0.0f;
        float averagePeakHoldLiftFromFastDb = 0.0f;
        float averageTonalPeakToMeanDb = 0.0f;
        float averageFastPeakToMeanDb = 0.0f;
        float maxLiveLiftFromFastDb = 0.0f;
        float maxPeakHoldLiftFromFastDb = 0.0f;
        int binsWithFastLift = 0;
        float fastLiftBinRatio = 0.0f;
        float averageMetricReferenceErrorDb = 0.0f;
        float averageLiveReferenceErrorDb = 0.0f;
        float averageAbsMetricReferenceErrorDb = 0.0f;
        float averageAbsLiveReferenceErrorDb = 0.0f;
        float maxAbsMetricReferenceErrorDb = 0.0f;
        float maxAbsLiveReferenceErrorDb = 0.0f;
        int binsWithReferenceComparison = 0;
        float referenceComparisonBinRatio = 0.0f;

        float lowBandAverageMetricDb = -100.0f;
        float midBandAverageMetricDb = -100.0f;
        float highBandAverageMetricDb = -100.0f;

        float lowBandAverageLiveDb = -100.0f;
        float midBandAverageLiveDb = -100.0f;
        float highBandAverageLiveDb = -100.0f;

        float lowToMidMetricTiltDb = 0.0f;
        float highToMidMetricTiltDb = 0.0f;

        float lowToMidLiveTiltDb = 0.0f;
        float highToMidLiveTiltDb = 0.0f;
    };

    struct VqtLikeValidationSignalSpec
    {
        enum class Type
        {
            sine,
            dualSine,
            whiteNoise,
            pinkNoise,
            logarithmicSweep
        };

        Type type = Type::sine;

        float frequencyHz = 1000.0f;
        float secondFrequencyHz = 0.0f;
        float levelDb = -18.0f;
        float durationSeconds = 1.0f;

        float sweepStartHz = 20.0f;
        float sweepEndHz = 20000.0f;

        std::uint32_t randomSeed = 0x12345678u;
    };

    struct VqtLikeValidationResult
    {
        VqtLikeValidationSignalSpec spec;
        juce::String signalName;

        float targetFrequencyHz = 0.0f;
        float measuredPeakFrequencyHz = 0.0f;
        float measuredMetricDb = -100.0f;
        float measuredLiveDb = -100.0f;
        float expectedDb = -100.0f;

        float metricErrorDb = 0.0f;
        float liveErrorDb = 0.0f;

        float peakWidthBinsAboveMinus3Db = 0.0f;
        float peakWidthHzAboveMinus3Db = 0.0f;

        float averageMetricReferenceErrorDb = 0.0f;
        float averageAbsMetricReferenceErrorDb = 0.0f;
        float maxAbsMetricReferenceErrorDb = 0.0f;

        float lowBandAverageMetricDb = -100.0f;
        float midBandAverageMetricDb = -100.0f;
        float highBandAverageMetricDb = -100.0f;

        float lowBandAverageLiveDb = -100.0f;
        float midBandAverageLiveDb = -100.0f;
        float highBandAverageLiveDb = -100.0f;

        float lowToMidMetricTiltDb = 0.0f;
        float highToMidMetricTiltDb = 0.0f;

        float lowToMidLiveTiltDb = 0.0f;
        float highToMidLiveTiltDb = 0.0f;

        bool isValid = false;
    };
    
    void run() override;
    void processOneFftBlock();
    void updateFftSizeIfNeeded();
    std::array<FrequencyDependentSourceDescriptor, 4>
    getFrequencyDependentSourceDescriptors() noexcept;
    bool canUseFrequencyDependentSource(const FrequencyDependentFftSource& source,
                                        int fftSize) const noexcept;
    FrequencyDependentSourceAvailability
    getFrequencyDependentSourceAvailability() const noexcept;

    DisplayBinPowerStats getFrequencyDependentSourceStatsForDisplayBin(
        const FrequencyDependentFftSource& source,
        int fftSize,
        size_t displayBinIndex) const noexcept;

    void configureFft(int newFftOrder);
    void resetOverlapBuffer();
    void configureFrequencyDependentFftSource(FrequencyDependentFftSource& source,
                                              int fftOrder,
                                              int fftSize);
    void resetFrequencyDependentFftSource(FrequencyDependentFftSource& source);
    void appendSamplesToFrequencyDependentFftSource(FrequencyDependentFftSource& source,
                                                    const float* samples,
                                                    int numSamples,
                                                    int fftSize);
    void processFrequencyDependentFftSourceIfReady(FrequencyDependentFftSource& source,
                                                   int fftSize);
    void updateFrequencyDependentSourceBinFftRangesIfNeeded(
        FrequencyDependentFftSource& source,
        int fftSize);
    float getFrequencyDependentMidBassBlendForFrequency(float frequencyHz) const noexcept;
    float getFrequencyDependentMainBlendForFrequency(float frequencyHz) const noexcept;
    float getFrequencyDependentHighBlendForFrequency(float frequencyHz) const noexcept;
    float getFrequencyDependentVeryHighBlendForFrequency(
        float frequencyHz) const noexcept;
    FrequencyDependentBlendWeights getFrequencyDependentBlendWeightsForFrequency(
        float frequencyHz) const noexcept;
    float getFrequencyDependentTransientAttackBlend() const noexcept;
    float getFrequencyDependentTransientTailSuppressBlend() const noexcept;
    float getFrequencyDependentTransientAssistReleaseSeconds() const noexcept;
    float getFrequencyDependentLowBassTailReleaseTimeSeconds() const noexcept;
    float getFrequencyDependentVeryHighReleaseTimeSeconds() const noexcept;
    FrequencyDependentLowCompositeResult getFrequencyDependentLowCompositeForDisplayBin(
        size_t displayBinIndex,
        float centerFrequencyHz,
        const DisplayBinPowerStats& mainStats,
        const FrequencyDependentSourceAvailability& sourceAvailability,
        const FrequencyDependentBlendWeights& blendWeights) const;
    FrequencyDependentHighCompositeResult applyFrequencyDependentHighBlendForDisplayBin(
        size_t displayBinIndex,
        float centerFrequencyHz,
        const DisplayBinPowerStats& baseStats,
        const FrequencyDependentSourceAvailability& sourceAvailability,
        const FrequencyDependentBlendWeights& blendWeights) const;
    FrequencyDependentBinStats getFrequencyDependentBinStatsForDisplayBin(
        int displayBinIndex,
        int fftSizeForBlock,
        const FrequencyDependentSourceAvailability& sourceAvailability) const;
    DisplayBinPowerStats blendDisplayBinPowerStats(const DisplayBinPowerStats& bassStats,
                                                   const DisplayBinPowerStats& mainStats,
                                                   float mainBlend) const noexcept;
    DisplayBinPowerStats applyFrequencyDependentTransientAssist(
        const DisplayBinPowerStats& frequencyDependentStats,
        const DisplayBinPowerStats& mainStats,
        float centerFrequencyHz,
        float assistAmount) const noexcept;
    FrequencyDependentLiveAssistResult applyFrequencyDependentLiveAssistForDisplayBin(
        const DisplayBinPowerStats& compositeStats,
        const DisplayBinPowerStats& transientReferenceStats,
        float centerFrequencyHz,
        bool hasCenterFrequency,
        const FrequencyDependentBlendWeights& blendWeights,
        float& storedAssistAmount,
        float frameAdvanceSeconds,
        float transientAssistReleaseSmoothing) const noexcept;
    FrequencyDependentTunedLowBandAlignmentResult
    applyFrequencyDependentTunedLowBandTransientAlignmentForDisplayBin(
        const DisplayBinPowerStats& liveVisualStats,
        const DisplayBinPowerStats& transientReferenceStats,
        float centerFrequencyHz,
        bool hasCenterFrequency,
        const FrequencyDependentBlendWeights& blendWeights,
        float& storedAlignmentAmount,
        float& storedPreviousReferenceDb,
        float frameAdvanceSeconds,
        float alignmentReleaseSmoothing) const noexcept;
    FrequencyDependentLiveReleaseBlendWeights
    getFrequencyDependentLiveReleaseBlendWeightsForDisplayBin(
        const FrequencyDependentBinStats& binStats,
        const FrequencyDependentLiveAssistResult& assistResult) const noexcept;
    FrequencyDependentBinPolicySnapshot getFrequencyDependentBinPolicySnapshot(
        const FrequencyDependentBinStats& binStats,
        const FrequencyDependentLiveReleaseBlendWeights& liveReleaseBlendWeights,
        float tunedLowBandTransientAlignmentAmount,
        float tunedLowBandOnsetConfidence,
        float tunedLowBandAlignmentMaxLiftDb) const noexcept;
    FrequencyDependentPolicyBand getFrequencyDependentPolicyBandForSnapshot(
        const FrequencyDependentBinPolicySnapshot& snapshot) const noexcept;
    void resetFrequencyDependentPolicyFrameSummary() noexcept;
    void accumulateFrequencyDependentPolicyFrameSummary(
        const FrequencyDependentBinPolicySnapshot& snapshot) noexcept;
    void finalizeFrequencyDependentPolicyFrameSummary() noexcept;
    void resetVqtLikeFilterbankState() noexcept;
    void resetVqtLikeFrameSummary() noexcept;
    void accumulateVqtLikeFrameSummary(
        const VqtLikeDisplayBinStats& stats) noexcept;
    void finalizeVqtLikeFrameSummary() noexcept;
    void configureVqtLikeFilterbankIfNeeded();
    void configureVqtLikeFilterBand(VqtLikeFilterBand& band,
                                    float centerFrequencyHz,
                                    float sampleRate) noexcept;
    float powerToAnalyzerDb(float power) const noexcept;
    float safePowerRatioDb(float numeratorPower,
                           float denominatorPower) const noexcept;
    float getBiquadMagnitudeAtFrequency(float b0,
                                        float b1,
                                        float b2,
                                        float a1,
                                        float a2,
                                        float frequencyHz,
                                        float sampleRate) const noexcept;
    float getVqtLikeLayerCalibrationPowerGain(float centerGain,
                                              float trim) const noexcept;
    float getVqtLikeTonalCalibrationTrimForFrequency(
        float frequencyHz) const noexcept;
    float getVqtLikeFastCalibrationTrimForFrequency(
        float frequencyHz) const noexcept;
    float getVqtLikeNoiseDensityTrimForFrequency(
        float frequencyHz) const noexcept;
    bool configureVqtLikeBandpassLayer(
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
        float& outCalibrationPowerGain) const noexcept;
    void processVqtLikeFilterbankSamples(const float* samples,
                                         int numSamples,
                                         float frameAdvanceSeconds) noexcept;
    float getVqtLikePeakBlendForFrequency(float frequencyHz,
                                          float lowBlend,
                                          float midBlend,
                                          float highBlend) const noexcept;
    float getVqtLikeTransientDetailBlendForFrequency(
        float frequencyHz) const noexcept;
    float getVqtLikeTransientDetailMaxLiftDbForFrequency(
        float frequencyHz) const noexcept;
    float getVqtLikeAggregationPeakShapeBlendForFrequency(
        float frequencyHz) const noexcept;
    float limitPowerLiftDb(float basePower,
                           float candidatePower,
                           float maxLiftDb) const noexcept;
    VqtLikeDisplayBinStats getVqtLikeDisplayBinStats(
        size_t displayBinIndex) const noexcept;

    juce::String getVqtLikeValidationSignalName(
        const VqtLikeValidationSignalSpec& spec) const;

    void generateVqtLikeValidationSignal(
        const VqtLikeValidationSignalSpec& spec,
        float sampleRate,
        std::vector<float>& outputBuffer) const;
    VqtLikeValidationResult runVqtLikeValidationSignal(
        const VqtLikeValidationSignalSpec& spec,
        float sampleRate);
    void runVqtLikeInternalCalibrationValidation();
    void requestDisplayAccumulationWarmStartForRangeChange() noexcept;
    void updateDisplayBinFftRangesIfNeeded();
    void publishLatestFrame();
    void handleClearPeakHoldRequest();
    void handleClearEnergyRequest();
    int getFftHopSize() const noexcept;
    int frequencyToMidiNote(float frequencyHz) const noexcept;
    int midiNoteToPitchClass(int midiNote) const noexcept;
    void extractInstantaneousNotePeaksFromFftData(int fftSizeForBlock);
    void updateTrackedNotePeaks(float frameDurationSeconds,
                                float peakHoldDecayDbPerSecondForFrame);
    static float smoothingCoefficientForTimeConstant(float frameDurationSeconds,
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
    static constexpr float frequencyDependentTunedTransientAttackBlend = 0.58f;
    static constexpr float frequencyDependentTunedTransientTailSuppressBlend = 0.80f;
    static constexpr float frequencyDependentTunedTransientAssistReleaseSeconds = 0.075f;
    static constexpr float frequencyDependentTunedLowBassTailReleaseTimeSeconds = 0.145f;
    static constexpr float frequencyDependentTunedVeryHighReleaseTimeSeconds = 0.210f;
    static constexpr float frequencyDependentTunedLowBandAlignmentMaxBlend = 0.82f;
    static constexpr float frequencyDependentTunedLowBandAlignmentReleaseSeconds = 0.055f;
    static constexpr float frequencyDependentTunedLowBandOnsetMinRiseDb = 1.2f;
    static constexpr float frequencyDependentTunedLowBandOnsetFullRiseDb = 5.5f;
    static constexpr float frequencyDependentTunedLowBandOnsetMinGapDb = 1.5f;
    static constexpr float frequencyDependentTunedLowBandOnsetFullGapDb = 7.0f;
    static constexpr float frequencyDependentTunedLowBandAlignmentMaxLiftDb = 4.5f;
    static constexpr float frequencyDependentTunedLowBandAlignmentMinEnergyDb = -82.0f;
    static constexpr float frequencyDependentTunedLowBandAlignmentFullEnergyDb = -58.0f;
    static constexpr int vqtLikeAnalysisBandsPerDisplayBin = 3;
    static constexpr int vqtLikeAnalysisBandCount =
        displayBinCount * vqtLikeAnalysisBandsPerDisplayBin;
    static constexpr float vqtLikeResolutionBaseQ = 42.0f;
    static constexpr float vqtLikeResolutionMinEffectiveQ = 2.5f;
    static constexpr float vqtLikeResolutionMaxEffectiveQ = 96.0f;
    static constexpr float vqtLikeResolutionLowBandGammaHz = 4.5f;
    static constexpr float vqtLikeResolutionGammaFadeStartHz = 70.0f;
    static constexpr float vqtLikeResolutionGammaFadeEndHz = 850.0f;
    static constexpr float vqtLikeResolutionMinBandwidthHz = 3.5f;
    static constexpr float vqtLikeResolutionMaxBandwidthFractionOfCenter = 0.75f;
    static constexpr float vqtLikeAggregationRadiusDisplayBins = 1.25f;
    static constexpr float vqtLikeCalibrationMinGain = 0.125f;
    static constexpr float vqtLikeCalibrationMaxGain = 8.0f;
    static constexpr float vqtLikeCalibrationReferenceBandwidthHz = 1.0f;
    static constexpr float vqtLikeTonalSineCalibrationTrim = 1.0f;
    static constexpr float vqtLikeFastSineCalibrationTrim = 1.0f;
    static constexpr float vqtLikeAggregationPeakShapeBlendLow = 0.18f;
    static constexpr float vqtLikeAggregationPeakShapeBlendMid = 0.32f;
    static constexpr float vqtLikeAggregationPeakShapeBlendHigh = 0.46f;
    static constexpr float vqtLikeFastBaseQ = 10.0f;
    static constexpr float vqtLikeFastMinEffectiveQ = 0.85f;
    static constexpr float vqtLikeFastMaxEffectiveQ = 28.0f;
    static constexpr float vqtLikeFastLowBandGammaHz = 12.0f;
    static constexpr float vqtLikeFastGammaFadeStartHz = 120.0f;
    static constexpr float vqtLikeFastGammaFadeEndHz = 1600.0f;
    static constexpr float vqtLikeFastMinBandwidthHz = 12.0f;
    static constexpr float vqtLikeFastMaxBandwidthFractionOfCenter = 1.15f;
    static constexpr float vqtLikeEnvelopeAttackSeconds = 0.018f;
    static constexpr float vqtLikeEnvelopeReleaseSeconds = 0.160f;
    static constexpr float vqtLikePeakEnvelopeReleaseSeconds = 0.075f;
    static constexpr float vqtLikeFastEnvelopeAttackSeconds = 0.006f;
    static constexpr float vqtLikeFastEnvelopeReleaseSeconds = 0.060f;
    static constexpr float vqtLikeFastPeakEnvelopeReleaseSeconds = 0.040f;
    static constexpr float vqtLikeLiveAttackTimeSeconds = 0.025f;
    static constexpr float vqtLikeLiveReleaseTimeSeconds = 0.220f;
    static constexpr float vqtLikeMeanPowerScale = 2.0f;
    static constexpr float vqtLikePeakPowerScale = 1.0f;
    static constexpr float vqtLikeMaxDisplayPower = 4.0f;
    static constexpr float vqtLikeLivePeakBlendLow = 0.20f;
    static constexpr float vqtLikeLivePeakBlendMid = 0.42f;
    static constexpr float vqtLikeLivePeakBlendHigh = 0.68f;
    static constexpr float vqtLikePeakHoldPeakBlendLow = 0.35f;
    static constexpr float vqtLikePeakHoldPeakBlendMid = 0.58f;
    static constexpr float vqtLikePeakHoldPeakBlendHigh = 0.82f;
    static constexpr float vqtLikePeakBlendLowToMidStartHz = 90.0f;
    static constexpr float vqtLikePeakBlendLowToMidEndHz = 420.0f;
    static constexpr float vqtLikePeakBlendMidToHighStartHz = 2500.0f;
    static constexpr float vqtLikePeakBlendMidToHighEndHz = 9000.0f;
    static constexpr float vqtLikeTransientDetailBlendLow = 0.22f;
    static constexpr float vqtLikeTransientDetailBlendMid = 0.38f;
    static constexpr float vqtLikeTransientDetailBlendHigh = 0.62f;
    static constexpr float vqtLikeTransientDetailLowToMidStartHz = 90.0f;
    static constexpr float vqtLikeTransientDetailLowToMidEndHz = 450.0f;
    static constexpr float vqtLikeTransientDetailMidToHighStartHz = 2200.0f;
    static constexpr float vqtLikeTransientDetailMidToHighEndHz = 9000.0f;
    static constexpr float vqtLikeTransientDetailMaxLiftDbLow = 3.0f;
    static constexpr float vqtLikeTransientDetailMaxLiftDbMid = 5.0f;
    static constexpr float vqtLikeTransientDetailMaxLiftDbHigh = 8.0f;
    static constexpr float vqtLikeMinimumUsefulPower = 1.0e-12f;
    static constexpr bool enableVqtLikeInternalValidation = false;
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
    int currentFftSize = analyzerFftSizeFromOrder(defaultFftOrder);
    bool currentFrequencyDependentResolutionEnabled = false;
    bool currentFrequencyDependentTunedResolutionEnabled = false;
    bool currentVqtLikeFilterbankEnabled = false;

    std::atomic<int> requestedFftOrder{defaultFftOrder};
    std::atomic<bool> requestedFrequencyDependentResolutionEnabled{false};
    std::atomic<bool> requestedFrequencyDependentTunedResolutionEnabled { false };
    std::atomic<bool> requestedVqtLikeFilterbankEnabled { false };
    std::atomic<float> requestedDisplayMinFrequencyHz{AnalyzerFrequencyRange::minimumHz};
    std::atomic<float> requestedDisplayMaxFrequencyHz{AnalyzerFrequencyRange::maximumHz};

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
    // Oversampled internal VQT-like analysis bands, aggregated into display bins.
    std::vector<VqtLikeFilterBand> vqtLikeFilterBands;
    float vqtLikeFilterbankSampleRate = 0.0f;
    float vqtLikeFilterbankMinFrequencyHz = 0.0f;
    float vqtLikeFilterbankMaxFrequencyHz = 0.0f;
    bool vqtLikeFilterbankNeedsReset = true;

    bool overlapBufferPrimed = false;
    float secondsSinceLastFramePublish = 0.0f;
    bool displayAccumulationWarmStartRequested = false;
    float energyAccumulatedActiveSeconds = 0.0f;
    std::vector<float> frequencyDependentTransientAssistAmounts;
    std::vector<float> frequencyDependentTunedLowBandAlignmentAmounts;
    std::vector<float> frequencyDependentTunedLowBandPreviousReferenceDb;
    std::vector<FrequencyDependentBinPolicySnapshot> frequencyDependentBinPolicySnapshots;
    FrequencyDependentPolicyFrameSummary frequencyDependentPolicyFrameSummary;
    VqtLikeFrameSummary vqtLikeFrameSummary;
    std::vector<VqtLikeValidationResult> lastVqtLikeValidationResults;

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
    std::atomic<bool> hasFrame{false};
    std::atomic<bool> clearPeakHoldRequested{false};
    std::atomic<bool> clearEnergyRequested{false};
    std::atomic<float> peakHoldDecayDbPerSecond{defaultPeakHoldDecayDbPerSecond};
    std::atomic<float> rmsTimeSeconds{defaultRmsTimeSeconds};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AnalyzerEngine)
};
