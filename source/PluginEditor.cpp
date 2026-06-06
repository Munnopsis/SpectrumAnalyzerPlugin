#include "PluginEditor.h"
#include "analyzer/AnalyzerInputMode.h"
#include "analyzer/AnalyzerFftSize.h"
#include "analyzer/AnalyzerPeakHoldDecay.h"
#include "analyzer/AnalyzerRmsTime.h"
#include "analyzer/AnalyzerDbRange.h"
#include "analyzer/AnalyzerSlope.h"

PluginEditor::PluginEditor (PluginProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    juce::ignoreUnused (processorRef);

    addAndMakeVisible (spectrumDisplay);

    addAndMakeVisible (inspectButton);
    addAndMakeVisible (liveButton);
    addAndMakeVisible (rmsButton);
    addAndMakeVisible (peakButton);
    addAndMakeVisible (clearPeakButton);
    addAndMakeVisible (inputModeBox);
    addAndMakeVisible (fftSizeBox);
    addAndMakeVisible (peakHoldDecayBox);
    addAndMakeVisible (rmsTimeBox);
    addAndMakeVisible (dbRangeBox);
    addAndMakeVisible (slopeBox);

    clearPeakButton.onClick = [this]
    {
        processorRef.requestClearPeakHold();
    };

    liveButton.setClickingTogglesState (true);
    rmsButton.setClickingTogglesState (true);
    peakButton.setClickingTogglesState (true);

    auto& state = processorRef.getValueTreeState();

    inputModeBox.addItemList (getAnalyzerInputModeChoices(), 1);
    inputModeBox.setJustificationType (juce::Justification::centred);
    inputModeBox.setTooltip ("Analyzer input mode");

    inputModeAttachment = std::make_unique<ComboBoxAttachment> (
        state,
        PluginProcessor::inputModeParamId,
        inputModeBox);

    fftSizeBox.addItemList (getAnalyzerFftSizeChoices(), 1);
    fftSizeBox.setJustificationType (juce::Justification::centred);
    fftSizeBox.setTooltip ("FFT size");

    fftSizeAttachment = std::make_unique<ComboBoxAttachment> (
        state,
        PluginProcessor::fftSizeParamId,
        fftSizeBox);

    peakHoldDecayBox.addItemList (getAnalyzerPeakHoldDecayChoices(), 1);
    peakHoldDecayBox.setJustificationType (juce::Justification::centred);
    peakHoldDecayBox.setTooltip ("Peak hold decay");

    peakHoldDecayAttachment = std::make_unique<ComboBoxAttachment> (
        state,
        PluginProcessor::peakHoldDecayParamId,
        peakHoldDecayBox);

    rmsTimeBox.addItemList (getAnalyzerRmsTimeChoices(), 1);
    rmsTimeBox.setJustificationType (juce::Justification::centred);
    rmsTimeBox.setTooltip ("RMS averaging time");

    rmsTimeAttachment = std::make_unique<ComboBoxAttachment> (
        state,
        PluginProcessor::rmsTimeParamId,
        rmsTimeBox);

    dbRangeBox.addItemList (getAnalyzerDbRangeChoices(), 1);
    dbRangeBox.setJustificationType (juce::Justification::centred);
    dbRangeBox.setTooltip ("Analyzer dB display range");

    dbRangeAttachment = std::make_unique<ComboBoxAttachment> (
        state,
        PluginProcessor::dbRangeParamId,
        dbRangeBox);

    slopeBox.addItemList (getAnalyzerSlopeChoices(), 1);
    slopeBox.setJustificationType (juce::Justification::centred);
    slopeBox.setTooltip ("Analyzer slope compensation");

    slopeAttachment = std::make_unique<ComboBoxAttachment> (
        state,
        PluginProcessor::slopeParamId,
        slopeBox);

    liveButtonAttachment = std::make_unique<ButtonAttachment> (
        state,
        PluginProcessor::showLiveCurveParamId,
        liveButton);

    rmsButtonAttachment = std::make_unique<ButtonAttachment> (
        state,
        PluginProcessor::showRmsCurveParamId,
        rmsButton);

    peakButtonAttachment = std::make_unique<ButtonAttachment> (
        state,
        PluginProcessor::showPeakHoldCurveParamId,
        peakButton);

    spectrumDisplay.setCurveVisibility (
        processorRef.shouldShowLiveCurve(),
        processorRef.shouldShowRmsCurve(),
        processorRef.shouldShowPeakHoldCurve());

    inspectButton.onClick = [&]
    {
        if (! inspector)
        {
            inspector = std::make_unique<melatonin::Inspector> (*this);
            inspector->onClose = [this]() { inspector.reset(); };
        }

        inspector->setVisible (true);
    };

    setSize (1500, 650);

    startTimerHz (30);
}

PluginEditor::~PluginEditor()
{
}

void PluginEditor::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    auto area = getLocalBounds();
    g.setColour (juce::Colours::white);
    g.setFont (16.0f);
    auto helloWorld = juce::String ("Hello from ") + PRODUCT_NAME_WITHOUT_VERSION + " v" VERSION + " running in " + CMAKE_BUILD_TYPE;
    g.drawText (helloWorld, area.removeFromTop (150), juce::Justification::centred, false);
}

void PluginEditor::resized()
{
    auto bounds = getLocalBounds();

    spectrumDisplay.setBounds (bounds);

    auto topBar = bounds.reduced (12).removeFromTop (32);

    auto rightControls = topBar.removeFromRight (1200);

    inputModeBox.setBounds (rightControls.removeFromLeft (150));
    rightControls.removeFromLeft (8);

    fftSizeBox.setBounds (rightControls.removeFromLeft (100));
    rightControls.removeFromLeft (8);

    peakHoldDecayBox.setBounds (rightControls.removeFromLeft (130));
    rightControls.removeFromLeft (8);

    rmsTimeBox.setBounds (rightControls.removeFromLeft (120));
    rightControls.removeFromLeft (8);

    dbRangeBox.setBounds (rightControls.removeFromLeft (90));
    rightControls.removeFromLeft (8);

    slopeBox.setBounds (rightControls.removeFromLeft (110));
    rightControls.removeFromLeft (8);

    liveButton.setBounds (rightControls.removeFromLeft (64));
    rightControls.removeFromLeft (6);

    rmsButton.setBounds (rightControls.removeFromLeft (64));
    rightControls.removeFromLeft (6);

    peakButton.setBounds (rightControls.removeFromLeft (64));
    rightControls.removeFromLeft (8);

    clearPeakButton.setBounds (rightControls.removeFromLeft (92));
    rightControls.removeFromLeft (8);

    inspectButton.setBounds (rightControls.removeFromLeft (140));
}

void PluginEditor::timerCallback()
{
    spectrumDisplay.setInputLevelDb (processorRef.getInputLevelDb());
    spectrumDisplay.setMinimumDecibels (processorRef.getAnalyzerMinimumDecibels());
    spectrumDisplay.setSlopeDbPerOctave (processorRef.getAnalyzerSlopeDbPerOctave());

    spectrumDisplay.setCurveVisibility (
        processorRef.shouldShowLiveCurve(),
        processorRef.shouldShowRmsCurve(),
        processorRef.shouldShowPeakHoldCurve());

    if (processorRef.copyLatestAnalyzerFrame (analyzerFrame))
    {
        spectrumDisplay.setSpectrumDb (analyzerFrame.liveDb);
        spectrumDisplay.setPeakHoldSpectrumDb (analyzerFrame.peakHoldDb);
        spectrumDisplay.setRmsSpectrumDb (analyzerFrame.rmsDb);
    }
}
