#pragma once

#include <atomic>
#include <juce_audio_processors/juce_audio_processors.h>
#include "analyzer/AnalyzerFifo.h"
#include "analyzer/AnalyzerEngine.h"
#include <vector>

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

private:
    std::atomic<float> inputLevelDb { -100.0f };

    AnalyzerFifo analyzerFifo;
    AnalyzerEngine analyzerEngine;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
};
