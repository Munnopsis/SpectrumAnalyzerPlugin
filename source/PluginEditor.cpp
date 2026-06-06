#include "PluginEditor.h"

PluginEditor::PluginEditor (PluginProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    juce::ignoreUnused (processorRef);

    addAndMakeVisible (spectrumDisplay);
    addAndMakeVisible (inspectButton);
    addAndMakeVisible (liveButton);
    addAndMakeVisible (rmsButton);
    addAndMakeVisible (peakButton);

    liveButton.setClickingTogglesState (true);
    rmsButton.setClickingTogglesState (true);
    peakButton.setClickingTogglesState (true);

    liveButton.setToggleState (true, juce::dontSendNotification);
    rmsButton.setToggleState (true, juce::dontSendNotification);
    peakButton.setToggleState (true, juce::dontSendNotification);

    liveButton.onClick = [this]
    {
        showLiveCurve = liveButton.getToggleState();
        spectrumDisplay.setCurveVisibility (showLiveCurve, showRmsCurve, showPeakHoldCurve);
    };

    rmsButton.onClick = [this]
    {
        showRmsCurve = rmsButton.getToggleState();
        spectrumDisplay.setCurveVisibility (showLiveCurve, showRmsCurve, showPeakHoldCurve);
    };

    peakButton.onClick = [this]
    {
        showPeakHoldCurve = peakButton.getToggleState();
        spectrumDisplay.setCurveVisibility (showLiveCurve, showRmsCurve, showPeakHoldCurve);
    };

    spectrumDisplay.setCurveVisibility (showLiveCurve, showRmsCurve, showPeakHoldCurve);

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
    setSize (400, 300);

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

    inspectButton.setBounds (topBar.removeFromRight (140));

    topBar.removeFromRight (8);

    peakButton.setBounds (topBar.removeFromRight (64));
    topBar.removeFromRight (6);

    rmsButton.setBounds (topBar.removeFromRight (64));
    topBar.removeFromRight (6);

    liveButton.setBounds (topBar.removeFromRight (64));
}

void PluginEditor::timerCallback()
{
    spectrumDisplay.setInputLevelDb (processorRef.getInputLevelDb());

    if (processorRef.copyLatestSpectrumDb (spectrumBuffer))
        spectrumDisplay.setSpectrumDb (spectrumBuffer);

    if (processorRef.copyLatestPeakHoldSpectrumDb (peakHoldBuffer))
        spectrumDisplay.setPeakHoldSpectrumDb (peakHoldBuffer);

    if (processorRef.copyLatestRmsSpectrumDb (rmsBuffer))
        spectrumDisplay.setRmsSpectrumDb (rmsBuffer);
}
