#pragma once

#include "PluginProcessor.h"
#include "BinaryData.h"
#include "melatonin_inspector/melatonin_inspector.h"
#include "ui/SpectrumDisplay.h"
#include <vector>


//==============================================================================
class PluginEditor : public juce::AudioProcessorEditor,
                     private juce::Timer
{
public:
    explicit PluginEditor (PluginProcessor&);
    ~PluginEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    void timerCallback() override;

    PluginProcessor& processorRef;

    std::unique_ptr<melatonin::Inspector> inspector;
    juce::TextButton inspectButton { "Inspect the UI" };

    // Buttons / Boxes / ui elements
    juce::TextButton liveButton { "Live" };
    juce::TextButton rmsButton { "RMS" };
    juce::TextButton peakButton { "Peak" };
    juce::TextButton clearPeakButton { "Clear Peak" };
    juce::ComboBox inputModeBox;
    juce::ComboBox fftSizeBox;
    juce::ComboBox peakHoldDecayBox;
    juce::ComboBox rmsTimeBox;
    juce::ComboBox dbRangeBox;
    juce::ComboBox slopeBox;

    // Attachments
    std::unique_ptr<ButtonAttachment> liveButtonAttachment;
    std::unique_ptr<ButtonAttachment> rmsButtonAttachment;
    std::unique_ptr<ButtonAttachment> peakButtonAttachment;
    std::unique_ptr<ComboBoxAttachment> inputModeAttachment;
    std::unique_ptr<ComboBoxAttachment> fftSizeAttachment;
    std::unique_ptr<ComboBoxAttachment> peakHoldDecayAttachment;
    std::unique_ptr<ComboBoxAttachment> rmsTimeAttachment;
    std::unique_ptr<ComboBoxAttachment> dbRangeAttachment;
    std::unique_ptr<ComboBoxAttachment> slopeAttachment;

    SpectrumDisplay spectrumDisplay;
    AnalyzerEngine::Frame analyzerFrame;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
