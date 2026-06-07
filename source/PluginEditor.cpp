#include "PluginEditor.h"
#include "analyzer/AnalyzerInputMode.h"
#include "analyzer/AnalyzerFftSize.h"
#include "analyzer/AnalyzerFrequencyRange.h"
#include "analyzer/AnalyzerPeakHoldDecay.h"
#include "analyzer/AnalyzerRmsTime.h"
#include "analyzer/AnalyzerDbRange.h"
#include "analyzer/AnalyzerSlope.h"

PluginEditor::PluginEditor (PluginProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    juce::ignoreUnused (processorRef);

    addAndMakeVisible (spectrumDisplay);
    spectrumDisplay.setName ("SpectrumDisplay");
    processorRef.setAnalyzerDisplayFrequencyRange (AnalyzerFrequencyRange::minimumHz,
                                                   AnalyzerFrequencyRange::maximumHz);

    addAndMakeVisible (inspectButton);
    addAndMakeVisible (liveButton);
    addAndMakeVisible (rmsButton);
    addAndMakeVisible (peakButton);
    addAndMakeVisible (clearPeakButton);
    addAndMakeVisible (freezeButton);
    addAndMakeVisible (tooltipButton);
    addAndMakeVisible (inputModeBox);
    addAndMakeVisible (fftSizeBox);
    addAndMakeVisible (peakHoldDecayBox);
    addAndMakeVisible (rmsTimeBox);
    addAndMakeVisible (dbRangeBox);
    addAndMakeVisible (slopeBox);

    inspectButton.setName ("InspectButton");
    liveButton.setName ("LiveButton");
    rmsButton.setName ("RmsButton");
    peakButton.setName ("PeakButton");
    clearPeakButton.setName ("ClearPeakButton");
    freezeButton.setName ("FreezeButton");
    tooltipButton.setName ("TooltipButton");
    inputModeBox.setName ("InputModeBox");
    fftSizeBox.setName ("FftSizeBox");
    peakHoldDecayBox.setName ("PeakHoldDecayBox");
    rmsTimeBox.setName ("RmsTimeBox");
    dbRangeBox.setName ("DbRangeBox");
    slopeBox.setName ("SlopeBox");

    inspectButton.setWantsKeyboardFocus (true);
    liveButton.setWantsKeyboardFocus (true);
    rmsButton.setWantsKeyboardFocus (true);
    peakButton.setWantsKeyboardFocus (true);
    clearPeakButton.setWantsKeyboardFocus (true);
    freezeButton.setWantsKeyboardFocus (true);
    tooltipButton.setWantsKeyboardFocus (true);
    inputModeBox.setWantsKeyboardFocus (true);
    fftSizeBox.setWantsKeyboardFocus (true);
    peakHoldDecayBox.setWantsKeyboardFocus (true);
    rmsTimeBox.setWantsKeyboardFocus (true);
    dbRangeBox.setWantsKeyboardFocus (true);
    slopeBox.setWantsKeyboardFocus (true);

    inspectButton.setTooltip ("Open the Melatonin UI inspector");
    liveButton.setTooltip ("Show or hide the live spectrum curve");
    rmsButton.setTooltip ("Show or hide the RMS spectrum curve");
    peakButton.setTooltip ("Show or hide the peak hold curve");
    clearPeakButton.setTooltip ("Clear the peak hold curve");
    freezeButton.setTooltip ("Freeze the current live spectrum as a reference curve");
    tooltipButton.setTooltip ("Show or hide tooltips");

    clearPeakButton.onClick = [this]
    {
        processorRef.requestClearPeakHold();
    };

    freezeButton.onClick = [this]
    {
        if (spectrumDisplay.hasFrozenReferenceSpectrum())
            spectrumDisplay.clearFrozenReferenceSpectrum();
        else
            spectrumDisplay.freezeCurrentSpectrumAsReference();

        updateFreezeButtonState();
    };

    liveButton.setClickingTogglesState (true);
    rmsButton.setClickingTogglesState (true);
    peakButton.setClickingTogglesState (true);
    freezeButton.setClickingTogglesState (false);
    tooltipButton.setClickingTogglesState (true);
    tooltipButton.setToggleState (true, juce::dontSendNotification);

    tooltipButton.onClick = [this]
    {
        setTooltipsEnabled (tooltipButton.getToggleState());
    };

    auto& state = processorRef.getValueTreeState();

    inputModeBox.addItemList (getAnalyzerInputModeChoices(), 1);
    inputModeBox.setJustificationType (juce::Justification::centred);
    inputModeBox.setTextWhenNothingSelected ("Input");
    inputModeBox.setTooltip ("Select which channel signal is analysed");

    inputModeAttachment = std::make_unique<ComboBoxAttachment> (
        state,
        PluginProcessor::inputModeParamId,
        inputModeBox);

    fftSizeBox.addItemList (getAnalyzerFftSizeChoices(), 1);
    fftSizeBox.setJustificationType (juce::Justification::centred);
    fftSizeBox.setTextWhenNothingSelected ("FFT");
    fftSizeBox.setTooltip ("Select FFT size: smaller is faster, larger gives better bass resolution");

    fftSizeAttachment = std::make_unique<ComboBoxAttachment> (
        state,
        PluginProcessor::fftSizeParamId,
        fftSizeBox);

    peakHoldDecayBox.addItemList (getAnalyzerPeakHoldDecayChoices(), 1);
    peakHoldDecayBox.setJustificationType (juce::Justification::centred);
    peakHoldDecayBox.setTextWhenNothingSelected ("Peak");
    peakHoldDecayBox.setTooltip ("Select how quickly the peak hold curve falls");

    peakHoldDecayAttachment = std::make_unique<ComboBoxAttachment> (
        state,
        PluginProcessor::peakHoldDecayParamId,
        peakHoldDecayBox);

    rmsTimeBox.addItemList (getAnalyzerRmsTimeChoices(), 1);
    rmsTimeBox.setJustificationType (juce::Justification::centred);
    rmsTimeBox.setTextWhenNothingSelected ("RMS");
    rmsTimeBox.setTooltip ("Select RMS averaging time");

    rmsTimeAttachment = std::make_unique<ComboBoxAttachment> (
        state,
        PluginProcessor::rmsTimeParamId,
        rmsTimeBox);

    dbRangeBox.addItemList (getAnalyzerDbRangeChoices(), 1);
    dbRangeBox.setJustificationType (juce::Justification::centred);
    dbRangeBox.setTextWhenNothingSelected ("Range");
    dbRangeBox.setTooltip ("Select analyzer dB display range");

    dbRangeAttachment = std::make_unique<ComboBoxAttachment> (
        state,
        PluginProcessor::dbRangeParamId,
        dbRangeBox);

    slopeBox.addItemList (getAnalyzerSlopeChoices(), 1);
    slopeBox.setJustificationType (juce::Justification::centred);
    slopeBox.setTextWhenNothingSelected ("Slope");
    slopeBox.setTooltip ("Select display-only spectrum slope compensation");

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

    updateFreezeButtonState();

    inspectButton.onClick = [&]
    {
        if (! inspector)
        {
            inspector = std::make_unique<melatonin::Inspector> (*this);
            inspector->onClose = [this]() { inspector.reset(); };
        }

        inspector->setVisible (true);
    };

    setSize (1180, 650);

    startTimerHz (30);
}

PluginEditor::~PluginEditor()
{
}

void PluginEditor::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
}

void PluginEditor::resized()
{
    auto bounds = getLocalBounds();

    spectrumDisplay.setBounds (bounds);

    auto controlsArea = bounds.reduced (12).removeFromTop (68);

    auto firstRow = controlsArea.removeFromTop (30);
    controlsArea.removeFromTop (6);
    auto secondRow = controlsArea.removeFromTop (30);

    constexpr auto titleReserveWidth = 164;

    firstRow.removeFromLeft (titleReserveWidth);
    secondRow.removeFromLeft (titleReserveWidth);

    auto addGap = [] (juce::Rectangle<int>& area, int pixels)
    {
        area.removeFromLeft (pixels);
    };

    inputModeBox.setBounds (firstRow.removeFromLeft (132));
    addGap (firstRow, 8);

    fftSizeBox.setBounds (firstRow.removeFromLeft (86));
    addGap (firstRow, 8);

    peakHoldDecayBox.setBounds (firstRow.removeFromLeft (116));
    addGap (firstRow, 8);

    rmsTimeBox.setBounds (firstRow.removeFromLeft (106));
    addGap (firstRow, 8);

    dbRangeBox.setBounds (firstRow.removeFromLeft (80));
    addGap (firstRow, 8);

    slopeBox.setBounds (firstRow.removeFromLeft (100));

    liveButton.setBounds (secondRow.removeFromLeft (64));
    addGap (secondRow, 6);

    rmsButton.setBounds (secondRow.removeFromLeft (64));
    addGap (secondRow, 6);

    peakButton.setBounds (secondRow.removeFromLeft (64));
    addGap (secondRow, 8);

    clearPeakButton.setBounds (secondRow.removeFromLeft (92));
    addGap (secondRow, 8);

    freezeButton.setBounds (secondRow.removeFromLeft (104));
    addGap (secondRow, 8);

    inspectButton.setBounds (secondRow.removeFromLeft (128));
    addGap (secondRow, 8);

    tooltipButton.setBounds (secondRow.removeFromLeft (54));
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
        std::vector<SpectrumDisplay::DisplayNotePeak> displayNotePeaks;
        displayNotePeaks.reserve (analyzerFrame.notePeaks.size());

        for (const auto& notePeak : analyzerFrame.notePeaks)
        {
            displayNotePeaks.push_back ({
                notePeak.frequencyHz,
                notePeak.decibels,
                notePeak.midiNote,
                notePeak.pitchClass
            });
        }

        spectrumDisplay.setAnalyzerFrameData (analyzerFrame.dataMinFrequencyHz,
                                              analyzerFrame.dataMaxFrequencyHz,
                                              analyzerFrame.liveDb,
                                              analyzerFrame.peakHoldDb,
                                              analyzerFrame.rmsDb,
                                              analyzerFrame.energyDb,
                                              displayNotePeaks);
    }
}

void PluginEditor::updateFreezeButtonState()
{
    const auto hasFrozenReference =
        spectrumDisplay.hasFrozenReferenceSpectrum();

    freezeButton.setButtonText (hasFrozenReference ? "Clear Freeze" : "Freeze");
    freezeButton.setToggleState (hasFrozenReference, juce::dontSendNotification);
}

void PluginEditor::setTooltipsEnabled (bool shouldBeEnabled)
{
    tooltipWindow.setTooltipsEnabled (shouldBeEnabled);
}
