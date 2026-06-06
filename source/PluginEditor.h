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
    PluginProcessor& processorRef;
    std::unique_ptr<melatonin::Inspector> inspector;

    juce::TextButton inspectButton { "Inspect the UI" };
    juce::TextButton liveButton { "Live" };
    juce::TextButton rmsButton { "RMS" };
    juce::TextButton peakButton { "Peak" };

    void timerCallback() override;

    SpectrumDisplay spectrumDisplay;
    std::vector<float> spectrumBuffer;
    std::vector<float> peakHoldBuffer;
    std::vector<float> rmsBuffer;

    bool showLiveCurve = true;
    bool showRmsCurve = true;
    bool showPeakHoldCurve = true;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
