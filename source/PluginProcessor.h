#pragma once

#include <atomic>
#include <juce_audio_processors/juce_audio_processors.h>
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
#include "analyzer/AnalyzerVqtLiveCurveProfile.h"

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

    static inline const juce::String showLiveCurveParamId { "showLiveCurve" };
    static inline const juce::String showRmsCurveParamId { "showRmsCurve" };
    static inline const juce::String showPeakHoldCurveParamId { "showPeakHoldCurve" };
    static inline const juce::String showEnergyCurveParamId { "showEnergyCurve" };
    static inline const juce::String inputModeParamId { "inputMode" };
    static inline const juce::String fftSizeParamId { "fftSize" };
    static inline const juce::String peakHoldDecayParamId { "peakHoldDecay" };
    static inline const juce::String rmsTimeParamId { "rmsTime" };
    static inline const juce::String dbRangeParamId { "dbRange" };
    static inline const juce::String slopeParamId { "slope" };
    static inline const juce::String displayResolutionParamId { "displayResolution" };
    static inline const juce::String vqtLiveCurveProfileParamId { "vqtLiveCurveProfile" };

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

    float getAnalyzerMinimumDecibels() const noexcept;
    float getAnalyzerSlopeDbPerOctave() const noexcept;
    AnalyzerDisplayResolution getAnalyzerDisplayResolution() const noexcept;
    bool isFrequencyDependentAnalyzerResolution() const noexcept;
    bool isFrequencyDependentAnalyzerResolutionTuned() const noexcept;
    bool isVqtLikeAnalyzerFilterbank() const noexcept;

    bool copyLatestAnalyzerFrame (AnalyzerEngine::Frame& destination)
    {
        return analyzerEngine.copyLatestFrame (destination);
    }

    void requestClearPeakHold() noexcept
    {
        analyzerEngine.requestClearPeakHold();
    }

    void requestClearEnergy() noexcept
    {
        analyzerEngine.requestClearEnergy();
    }

    void setAnalyzerDisplayFrequencyRange (float minimumHz, float maximumHz) noexcept
    {
        analyzerEngine.setDisplayFrequencyRange (minimumHz, maximumHz);
    }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    juce::AudioProcessorValueTreeState parameters;

    int getAnalyzerFftOrder() const noexcept;
    float getPeakHoldDecayDbPerSecond() const noexcept;
    float getRmsTimeSeconds() const noexcept;
    AnalyzerVqtLiveCurveProfile getVqtLiveCurveProfile() const noexcept;

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

    AnalyzerFifo analyzerFifo;
    AnalyzerEngine analyzerEngine;
    AnalyzerInputMode getAnalyzerInputMode() const noexcept;


    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
};
