#include "PluginEditor.h"
#include "analyzer/AnalyzerInputMode.h"
#include "analyzer/AnalyzerFftSize.h"

PluginEditor::PluginEditor (PluginProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    juce::ignoreUnused (processorRef);

    // ui elements
    addAndMakeVisible (spectrumDisplay);
    addAndMakeVisible (inspectButton);
    addAndMakeVisible (liveButton);
    addAndMakeVisible (rmsButton);
    addAndMakeVisible (peakButton);
    addAndMakeVisible (clearPeakButton);
    addAndMakeVisible (inputModeBox);
    addAndMakeVisible (fftSizeBox);

    // button actions
    clearPeakButton.onClick = [this]
    {
        processorRef.requestClearPeakHold();
    };

    liveButton.setClickingTogglesState (true);
    rmsButton.setClickingTogglesState (true);
    peakButton.setClickingTogglesState (true);

    auto& state = processorRef.getValueTreeState();

    // combo box options
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

    // link button with action
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

    // this chunk of code instantiates and opens the melatonin inspector
    inspectButton.onClick = [&] {
        if (!inspector)
        {
            inspector = std::make_unique<melatonin::Inspector> (*this);
            inspector->onClose = [this]() { inspector.reset(); };
        }

        inspector->setVisible (true);
    };

    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.
    setSize (1100, 650);

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

    auto rightControls = topBar.removeFromRight (740);

    inputModeBox.setBounds (rightControls.removeFromLeft (150));
    rightControls.removeFromLeft (8);

    fftSizeBox.setBounds (rightControls.removeFromLeft (110));
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

    spectrumDisplay.setCurveVisibility (
        processorRef.shouldShowLiveCurve(),
        processorRef.shouldShowRmsCurve(),
        processorRef.shouldShowPeakHoldCurve());

    if (processorRef.copyLatestSpectrumDb (spectrumBuffer))
        spectrumDisplay.setSpectrumDb (spectrumBuffer);

    if (processorRef.copyLatestPeakHoldSpectrumDb (peakHoldBuffer))
        spectrumDisplay.setPeakHoldSpectrumDb (peakHoldBuffer);

    if (processorRef.copyLatestRmsSpectrumDb (rmsBuffer))
        spectrumDisplay.setRmsSpectrumDb (rmsBuffer);
}