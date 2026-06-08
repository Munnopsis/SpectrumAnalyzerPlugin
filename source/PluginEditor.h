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
    void setTooltipsEnabled (bool shouldBeEnabled);
    void updateFreezeButtonState();

    PluginProcessor& processorRef;

    std::unique_ptr<melatonin::Inspector> inspector;
    static constexpr int tooltipDelayMs = 700;

    class ToggleableTooltipWindow : public juce::TooltipWindow
    {
    public:
        ToggleableTooltipWindow (juce::Component* parentComponent,
                                 int millisecondsBeforeTipAppears)
            : juce::TooltipWindow (parentComponent, millisecondsBeforeTipAppears)
        {
        }

        void setTooltipsEnabled (bool shouldBeEnabled)
        {
            if (tooltipsEnabled == shouldBeEnabled)
                return;

            tooltipsEnabled = shouldBeEnabled;
            hideTip();
        }

        bool areTooltipsEnabled() const noexcept
        {
            return tooltipsEnabled;
        }

        juce::String getTipFor (juce::Component& component) override
        {
            if (! tooltipsEnabled)
                return {};

            return juce::TooltipWindow::getTipFor (component);
        }

    private:
        bool tooltipsEnabled = true;
    };

    ToggleableTooltipWindow tooltipWindow { this, tooltipDelayMs };
    juce::TextButton inspectButton { "Inspect the UI" };

    // Buttons / Boxes / ui elements
    juce::TextButton liveButton { "Live" };
    juce::TextButton rmsButton { "RMS" };
    juce::TextButton energyButton { "Energy" };
    juce::TextButton clearEnergyButton { "Clear Energy" };
    juce::TextButton peakButton { "Peak" };
    juce::TextButton clearPeakButton { "Clear Peak" };
    juce::TextButton peakDipButton { "Peaks/Dips" };
    juce::TextButton freezeButton { "Add Ref" };
    juce::TextButton clearReferencesButton { "Clear Refs" };
    juce::TextButton differenceButton { "Diff" };
    juce::TextButton stereoMeterButton { "Stereo" };
    juce::TextButton tooltipButton { "Tips" };
    juce::ComboBox inputModeBox;
    juce::ComboBox fftSizeBox;
    juce::ComboBox peakHoldDecayBox;
    juce::ComboBox rmsTimeBox;
    juce::ComboBox dbRangeBox;
    juce::ComboBox slopeBox;
    juce::ComboBox displayResolutionBox;
    juce::ComboBox vqtLiveCurveBox;

    // Attachments
    std::unique_ptr<ButtonAttachment> liveButtonAttachment;
    std::unique_ptr<ButtonAttachment> rmsButtonAttachment;
    std::unique_ptr<ButtonAttachment> energyButtonAttachment;
    std::unique_ptr<ButtonAttachment> peakButtonAttachment;
    std::unique_ptr<ButtonAttachment> peakDipButtonAttachment;
    std::unique_ptr<ButtonAttachment> differenceButtonAttachment;
    std::unique_ptr<ButtonAttachment> stereoMeterButtonAttachment;
    std::unique_ptr<ComboBoxAttachment> inputModeAttachment;
    std::unique_ptr<ComboBoxAttachment> fftSizeAttachment;
    std::unique_ptr<ComboBoxAttachment> peakHoldDecayAttachment;
    std::unique_ptr<ComboBoxAttachment> rmsTimeAttachment;
    std::unique_ptr<ComboBoxAttachment> dbRangeAttachment;
    std::unique_ptr<ComboBoxAttachment> slopeAttachment;
    std::unique_ptr<ComboBoxAttachment> displayResolutionAttachment;
    std::unique_ptr<ComboBoxAttachment> vqtLiveCurveAttachment;

    SpectrumDisplay spectrumDisplay;
    PluginProcessor::AnalyzerFrameBundle analyzerFrameBundle;
    SpectrumDisplay::StereoMeterDisplayData stereoMeterDisplayData;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
