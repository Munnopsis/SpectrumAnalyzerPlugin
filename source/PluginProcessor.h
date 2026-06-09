#pragma once

#include <atomic>
#include <array>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "analyzer/AnalyzerFifo.h"
#include "analyzer/AnalyzerEngine.h"
#include <vector>
#include "analyzer/AnalyzerInputMode.h"
#include "analyzer/AnalyzerFftSize.h"
#include "analyzer/AnalyzerPeakHoldDecay.h"
#include "analyzer/AnalyzerRmsTime.h"
#include "analyzer/AnalyzerDbRange.h"
#include "analyzer/AnalyzerSlope.h"
#include "analyzer/AnalyzerDisplayResolution.h"
#include "analyzer/AnalyzerCurveSource.h"
#include "analyzer/AnalyzerReferenceManager.h"
#include "analyzer/AnalyzerValidationSignal.h"
#include "analyzer/AnalyzerVqtLiveCurveProfile.h"
#include "analyzer/FrequencyCorrelationMeter.h"
#include "analyzer/LoudnessMeter.h"

#include <cstdint>
#include <mutex>

#if (MSVC)
#include "ipps.h"
#endif

class PluginProcessor : public juce::AudioProcessor
{
public:
    PluginProcessor();
    ~PluginProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    float getInputLevelDb() const noexcept
    {
        return inputLevelDb.load (std::memory_order_relaxed);
    }

    bool copyLatestSpectrumDb (std::vector<float>& destination)
    {
        return analyzerEngine.copyLatestSpectrumDb (destination);
    }

    bool copyLatestPeakHoldSpectrumDb (std::vector<float>& destination)
    {
        return analyzerEngine.copyLatestPeakHoldSpectrumDb (destination);
    }

    bool copyLatestRmsSpectrumDb (std::vector<float>& destination)
    {
        return analyzerEngine.copyLatestRmsSpectrumDb (destination);
    }

    struct AnalyzerFrameBundle
    {
        AnalyzerEngine::Frame primary;
        AnalyzerEngine::Frame secondary;
        bool hasPrimary = false;
        bool hasSecondary = false;
        juce::String primaryLabel;
        juce::String secondaryLabel;
    };

    struct StereoMeterSnapshot
    {
        float correlation = 0.0f;
        float smoothedCorrelation = 0.0f;
        bool correlationValid = false;
        float leftLevelDb = -100.0f;
        float rightLevelDb = -100.0f;
        float midLevelDb = -100.0f;
        float sideLevelDb = -100.0f;
        float balanceDb = 0.0f;
        float widthPercent = 0.0f;
        float monoCompatibilityDb = 0.0f;
    };

    static inline const juce::String showLiveCurveParamId { "showLiveCurve" };
    static inline const juce::String showRmsCurveParamId { "showRmsCurve" };
    static inline const juce::String showPeakHoldCurveParamId { "showPeakHoldCurve" };
    static inline const juce::String showEnergyCurveParamId { "showEnergyCurve" };
    static inline const juce::String showPeakDipMarkersParamId { "showPeakDipMarkers" };
    static inline const juce::String showDifferenceCurveParamId { "showDifferenceCurve" };
    static inline const juce::String showStereoMeterParamId { "showStereoMeter" };
    static inline const juce::String showLoudnessMeterParamId { "showLoudnessMeter" };
    static inline const juce::String showFrequencyCorrelationParamId { "showFrequencyCorrelation" };
    static inline const juce::String resetLoudnessParamId { "resetLoudness" };
    static inline const juce::String inputModeParamId { "inputMode" };
    static inline const juce::String fftSizeParamId { "fftSize" };
    static inline const juce::String peakHoldDecayParamId { "peakHoldDecay" };
    static inline const juce::String rmsTimeParamId { "rmsTime" };
    static inline const juce::String dbRangeParamId { "dbRange" };
    static inline const juce::String slopeParamId { "slope" };
    static inline const juce::String displayResolutionParamId { "displayResolution" };
    static inline const juce::String vqtLiveCurveProfileParamId { "vqtLiveCurveProfile" };
    static inline const juce::String peakDipSourceParamId { "peakDipSource" };
    static inline const juce::String differenceCurveSourceParamId { "differenceCurveSource" };
    static inline const juce::String validationSignalParamId { "validationSignal" };

    juce::AudioProcessorValueTreeState& getValueTreeState() noexcept
    {
        return parameters;
    }

    bool shouldShowLiveCurve() const noexcept
    {
        return parameters.getRawParameterValue (showLiveCurveParamId)->load() > 0.5f;
    }

    bool shouldShowRmsCurve() const noexcept
    {
        return parameters.getRawParameterValue (showRmsCurveParamId)->load() > 0.5f;
    }

    bool shouldShowEnergyCurve() const noexcept
    {
        return parameters.getRawParameterValue (showEnergyCurveParamId)->load() > 0.5f;
    }

    bool shouldShowPeakHoldCurve() const noexcept
    {
        return parameters.getRawParameterValue (showPeakHoldCurveParamId)->load() > 0.5f;
    }

    bool shouldShowPeakDipMarkers() const noexcept
    {
        return parameters.getRawParameterValue (showPeakDipMarkersParamId)->load() > 0.5f;
    }

    bool shouldShowDifferenceCurve() const noexcept
    {
        return parameters.getRawParameterValue (showDifferenceCurveParamId)->load() > 0.5f;
    }

    bool shouldShowStereoMeter() const noexcept
    {
        return parameters.getRawParameterValue (showStereoMeterParamId)->load() > 0.5f;
    }

    bool shouldShowLoudnessMeter() const noexcept
    {
        return parameters.getRawParameterValue (showLoudnessMeterParamId)->load() > 0.5f;
    }

    bool shouldShowFrequencyCorrelation() const noexcept
    {
        return parameters.getRawParameterValue (showFrequencyCorrelationParamId)->load() > 0.5f;
    }

    AnalyzerValidationSignal getAnalyzerValidationSignal() const noexcept;

    bool isAnalyzerValidationSignalEnabled() const noexcept
    {
        return getAnalyzerValidationSignal() != AnalyzerValidationSignal::off;
    }

    void validateRestoredAnalyzerState();

    float getAnalyzerMinimumDecibels() const noexcept;
    float getAnalyzerSlopeDbPerOctave() const noexcept;
    AnalyzerDisplayResolution getAnalyzerDisplayResolution() const noexcept;
    AnalyzerCurveSource getPeakDipSource() const noexcept;
    AnalyzerCurveSource getDifferenceCurveSource() const noexcept;
    bool isFrequencyDependentAnalyzerResolution() const noexcept;
    bool isFrequencyDependentAnalyzerResolutionTuned() const noexcept;
    bool isVqtLikeAnalyzerFilterbank() const noexcept;

    bool copyLatestAnalyzerFrame (AnalyzerEngine::Frame& destination)
    {
        return analyzerEngine.copyLatestFrame (destination);
    }

    bool copyLatestSecondaryAnalyzerFrame (AnalyzerEngine::Frame& destination)
    {
        if (!secondaryAnalyzerEnabled.load (std::memory_order_relaxed))
            return false;

        return secondaryAnalyzerEngine.copyLatestFrame (destination);
    }

    bool copyLatestAnalyzerFrameBundle (AnalyzerFrameBundle& destination);
    bool isSecondaryAnalyzerActive() const noexcept;
    juce::String getPrimaryAnalyzerCurveLabel() const;
    juce::String getSecondaryAnalyzerCurveLabel() const;
    void syncSecondaryAnalyzerRuntimeForCurrentInputMode();
    StereoMeterSnapshot getStereoMeterSnapshot() const noexcept;
    void copyGoniometerPoints (std::vector<juce::Point<float>>& destination) const;
    LoudnessMeter::Snapshot getLoudnessSnapshot() const noexcept;
    FrequencyCorrelationMeter::Snapshot getFrequencyCorrelationSnapshot() const noexcept;

    void resetLoudnessMeter() noexcept
    {
        loudnessMeter.reset();
    }

    int addReferenceFromCurrentAnalyzerFrame();
    void clearReferenceCurves();
    bool removeActiveReferenceCurve();
    bool setActiveReferenceIndex (int index);
    int getNumReferenceCurves() const;
    int getActiveReferenceIndex() const;
    juce::String getReferenceCurveName (int index) const;
    std::vector<AnalyzerReferenceCurve> getReferenceCurvesSnapshot() const;
    uint64_t getReferenceStateRevision() const noexcept
    {
        return referenceStateRevision.load (std::memory_order_relaxed);
    }

    void requestClearPeakHold() noexcept
    {
        analyzerEngine.requestClearPeakHold();
        secondaryAnalyzerEngine.requestClearPeakHold();
    }

    void requestClearEnergy() noexcept
    {
        analyzerEngine.requestClearEnergy();
        secondaryAnalyzerEngine.requestClearEnergy();
    }

    void setAnalyzerDisplayFrequencyRange (float minimumHz, float maximumHz) noexcept
    {
        analyzerEngine.setDisplayFrequencyRange (minimumHz, maximumHz);
        secondaryAnalyzerEngine.setDisplayFrequencyRange (minimumHz, maximumHz);
    }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    juce::AudioProcessorValueTreeState parameters;

    int getAnalyzerFftOrder() const noexcept;
    float getPeakHoldDecayDbPerSecond() const noexcept;
    float getRmsTimeSeconds() const noexcept;
    AnalyzerVqtLiveCurveProfile getVqtLiveCurveProfile() const noexcept;
    void configureAnalyzerEngineForCurrentSettings (AnalyzerEngine& engine) noexcept;
    void updateStereoMeterData (const juce::AudioBuffer<float>& buffer,
                                int numInputChannels) noexcept;
    void fillAnalyzerValidationBuffer (juce::AudioBuffer<float>& destination,
                                       int numSamples,
                                       AnalyzerValidationSignal signal,
                                       double sampleRate) noexcept;
    static juce::String getAnalyzerCurveLabelForMode (AnalyzerInputMode mode);

    // cached parameters
    std::atomic<float> inputLevelDb { -100.0f };
    std::atomic<float>* inputModeParameter = nullptr;
    std::atomic<float>* fftSizeParameter = nullptr;
    std::atomic<float>* peakHoldDecayParameter = nullptr;
    std::atomic<float>* rmsTimeParameter = nullptr;
    std::atomic<float>* dbRangeParameter = nullptr;
    std::atomic<float>* slopeParameter = nullptr;
    std::atomic<float>* displayResolutionParameter = nullptr;
    std::atomic<float>* vqtLiveCurveProfileParameter = nullptr;
    std::atomic<float>* validationSignalParameter = nullptr;

    AnalyzerFifo analyzerFifo;
    AnalyzerEngine analyzerEngine;
    AnalyzerFifo secondaryAnalyzerFifo;
    AnalyzerEngine secondaryAnalyzerEngine;
    LoudnessMeter loudnessMeter;
    FrequencyCorrelationMeter frequencyCorrelationMeter;
    AnalyzerReferenceManager referenceManager;
    juce::AudioBuffer<float> validationBuffer;
    double validationPhase = 0.0;
    double validationPhase2 = 0.0;
    double validationPhase3 = 0.0;
    mutable std::mutex referenceMutex;
    std::atomic<uint64_t> referenceStateRevision { 1 };
    std::atomic<bool> secondaryAnalyzerEnabled { false };
    std::atomic<bool> secondaryAnalyzerThreadStarted { false };

    static constexpr int goniometerPointCount = 512;
    std::array<std::atomic<float>, goniometerPointCount> goniometerX {};
    std::array<std::atomic<float>, goniometerPointCount> goniometerY {};
    std::atomic<int> goniometerWriteIndex { 0 };

    std::atomic<float> stereoCorrelation { 0.0f };
    std::atomic<float> stereoSmoothedCorrelation { 0.0f };
    std::atomic<bool> stereoCorrelationValid { false };
    std::atomic<float> stereoLeftLevelDb { -100.0f };
    std::atomic<float> stereoRightLevelDb { -100.0f };
    std::atomic<float> stereoMidLevelDb { -100.0f };
    std::atomic<float> stereoSideLevelDb { -100.0f };
    std::atomic<float> stereoBalanceDb { 0.0f };
    std::atomic<float> stereoWidthPercent { 0.0f };
    std::atomic<float> stereoMonoCompatibilityDb { 0.0f };

    AnalyzerInputMode getAnalyzerInputMode() const noexcept;


    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
};
