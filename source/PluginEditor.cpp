#include "PluginEditor.h"
#include "analyzer/AnalyzerInputMode.h"
#include "analyzer/AnalyzerFftSize.h"
#include "analyzer/AnalyzerFrequencyRange.h"
#include "analyzer/AnalyzerPeakHoldDecay.h"
#include "analyzer/AnalyzerRmsTime.h"
#include "analyzer/AnalyzerDbRange.h"
#include "analyzer/AnalyzerSlope.h"
#include "analyzer/AnalyzerDisplayResolution.h"
#include "analyzer/AnalyzerVqtLiveCurveProfile.h"

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
    addAndMakeVisible (energyButton);
    addAndMakeVisible (clearEnergyButton);
    addAndMakeVisible (peakButton);
    addAndMakeVisible (clearPeakButton);
    addAndMakeVisible (peakDipButton);
    addAndMakeVisible (freezeButton);
    addAndMakeVisible (clearReferencesButton);
    addAndMakeVisible (differenceButton);
    addAndMakeVisible (tooltipButton);
    addAndMakeVisible (inputModeBox);
    addAndMakeVisible (fftSizeBox);
    addAndMakeVisible (peakHoldDecayBox);
    addAndMakeVisible (rmsTimeBox);
    addAndMakeVisible (dbRangeBox);
    addAndMakeVisible (slopeBox);
    addAndMakeVisible (displayResolutionBox);
    addAndMakeVisible (vqtLiveCurveBox);

    inspectButton.setName ("InspectButton");
    liveButton.setName ("LiveButton");
    rmsButton.setName ("RmsButton");
    energyButton.setName ("EnergyButton");
    clearEnergyButton.setName ("ClearEnergyButton");
    peakButton.setName ("PeakButton");
    clearPeakButton.setName ("ClearPeakButton");
    peakDipButton.setName ("PeakDipButton");
    freezeButton.setName ("FreezeButton");
    clearReferencesButton.setName ("ClearReferencesButton");
    differenceButton.setName ("DifferenceButton");
    tooltipButton.setName ("TooltipButton");
    inputModeBox.setName ("InputModeBox");
    fftSizeBox.setName ("FftSizeBox");
    peakHoldDecayBox.setName ("PeakHoldDecayBox");
    rmsTimeBox.setName ("RmsTimeBox");
    dbRangeBox.setName ("DbRangeBox");
    slopeBox.setName ("SlopeBox");
    displayResolutionBox.setName ("DisplayResolutionBox");
    vqtLiveCurveBox.setName ("VqtLiveCurveBox");

    inspectButton.setWantsKeyboardFocus (true);
    liveButton.setWantsKeyboardFocus (true);
    rmsButton.setWantsKeyboardFocus (true);
    energyButton.setWantsKeyboardFocus (true);
    clearEnergyButton.setWantsKeyboardFocus (true);
    peakButton.setWantsKeyboardFocus (true);
    clearPeakButton.setWantsKeyboardFocus (true);
    peakDipButton.setWantsKeyboardFocus (true);
    freezeButton.setWantsKeyboardFocus (true);
    clearReferencesButton.setWantsKeyboardFocus (true);
    differenceButton.setWantsKeyboardFocus (true);
    tooltipButton.setWantsKeyboardFocus (true);
    inputModeBox.setWantsKeyboardFocus (true);
    fftSizeBox.setWantsKeyboardFocus (true);
    peakHoldDecayBox.setWantsKeyboardFocus (true);
    rmsTimeBox.setWantsKeyboardFocus (true);
    dbRangeBox.setWantsKeyboardFocus (true);
    slopeBox.setWantsKeyboardFocus (true);
    displayResolutionBox.setWantsKeyboardFocus (true);
    vqtLiveCurveBox.setWantsKeyboardFocus (true);

    inspectButton.setTooltip ("Open the Melatonin UI inspector");
    liveButton.setTooltip ("Show or hide the live spectrum curve");
    rmsButton.setTooltip ("Show or hide the RMS spectrum curve");
    energyButton.setTooltip ("Show or hide the Energy spectrum curve");
    clearEnergyButton.setTooltip ("Clear the long-term Energy curve");
    peakButton.setTooltip ("Show or hide the peak hold curve");
    clearPeakButton.setTooltip ("Clear the peak hold curve");
    peakDipButton.setTooltip ("Show automatic peak and dip markers on the visible analyzer curve");
    freezeButton.setTooltip ("Store the current analyzer curves as a reference snapshot");
    clearReferencesButton.setTooltip ("Clear all stored reference curves");
    differenceButton.setTooltip ("Show difference between current analyzer curve and the active reference");
    tooltipButton.setTooltip ("Show or hide tooltips");

    clearPeakButton.onClick = [this]
    {
        processorRef.requestClearPeakHold();
    };

    clearEnergyButton.onClick = [this]
    {
        processorRef.requestClearEnergy();
    };

    freezeButton.onClick = [this]
    {
        spectrumDisplay.addCurrentSpectrumAsReference();

        updateFreezeButtonState();
    };

    clearReferencesButton.onClick = [this]
    {
        spectrumDisplay.clearAllReferenceCurves();

        updateFreezeButtonState();
    };

    liveButton.setClickingTogglesState (true);
    rmsButton.setClickingTogglesState (true);
    energyButton.setClickingTogglesState (true);
    peakButton.setClickingTogglesState (true);
    peakDipButton.setClickingTogglesState (true);
    freezeButton.setClickingTogglesState (false);
    clearReferencesButton.setClickingTogglesState (false);
    differenceButton.setClickingTogglesState (true);
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
    fftSizeBox.setTextWhenNothingSelected ("Resolution");
    fftSizeBox.setTooltip ("Select fixed FFT size or frequency-dependent analyzer resolution");

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
    slopeBox.setTextWhenNothingSelected ("Weighting");
    slopeBox.setTooltip (
        "Display weighting for raw, music-tilt or white-flat spectrum views");

    slopeAttachment = std::make_unique<ComboBoxAttachment> (
        state,
        PluginProcessor::slopeParamId,
        slopeBox);

    displayResolutionBox.addItemList (getAnalyzerDisplayResolutionChoices(), 1);
    displayResolutionBox.setJustificationType (juce::Justification::centred);
    displayResolutionBox.setTextWhenNothingSelected ("Resolution");
    displayResolutionBox.setTooltip (
        "Controls visual analyzer resolution from high-detail to octave-smoothed display");

    displayResolutionAttachment = std::make_unique<ComboBoxAttachment> (
        state,
        PluginProcessor::displayResolutionParamId,
        displayResolutionBox);

    vqtLiveCurveBox.addItemList (getAnalyzerVqtLiveCurveProfileChoices(), 1);
    vqtLiveCurveBox.setJustificationType (juce::Justification::centred);
    vqtLiveCurveBox.setTextWhenNothingSelected ("VQT Live");
    vqtLiveCurveBox.setTooltip (
        "Select VQT-like live curve response: smoother, balanced, or more detailed");

    vqtLiveCurveAttachment = std::make_unique<ComboBoxAttachment> (
        state,
        PluginProcessor::vqtLiveCurveProfileParamId,
        vqtLiveCurveBox);

    liveButtonAttachment = std::make_unique<ButtonAttachment> (
        state,
        PluginProcessor::showLiveCurveParamId,
        liveButton);

    rmsButtonAttachment = std::make_unique<ButtonAttachment> (
        state,
        PluginProcessor::showRmsCurveParamId,
        rmsButton);

    energyButtonAttachment = std::make_unique<ButtonAttachment> (
        state,
        PluginProcessor::showEnergyCurveParamId,
        energyButton);

    peakButtonAttachment = std::make_unique<ButtonAttachment> (
        state,
        PluginProcessor::showPeakHoldCurveParamId,
        peakButton);

    peakDipButtonAttachment = std::make_unique<ButtonAttachment> (
        state,
        PluginProcessor::showPeakDipMarkersParamId,
        peakDipButton);

    differenceButtonAttachment = std::make_unique<ButtonAttachment> (
        state,
        PluginProcessor::showDifferenceCurveParamId,
        differenceButton);

    spectrumDisplay.setCurveVisibility (
        processorRef.shouldShowLiveCurve(),
        processorRef.shouldShowRmsCurve(),
        processorRef.shouldShowEnergyCurve(),
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

    setSize (1280, 680);

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

    slopeBox.setBounds (firstRow.removeFromLeft (132));
    addGap (firstRow, 8);

    displayResolutionBox.setBounds (firstRow.removeFromLeft (128));
    addGap (firstRow, 8);

    vqtLiveCurveBox.setBounds (firstRow.removeFromLeft (128));

    liveButton.setBounds (secondRow.removeFromLeft (64));
    addGap (secondRow, 6);

    rmsButton.setBounds (secondRow.removeFromLeft (64));
    addGap (secondRow, 6);

    energyButton.setBounds (secondRow.removeFromLeft (76));
    addGap (secondRow, 6);

    clearEnergyButton.setBounds (secondRow.removeFromLeft (112));
    addGap (secondRow, 8);

    peakButton.setBounds (secondRow.removeFromLeft (64));
    addGap (secondRow, 8);

    clearPeakButton.setBounds (secondRow.removeFromLeft (92));
    addGap (secondRow, 8);

    peakDipButton.setBounds (secondRow.removeFromLeft (92));
    addGap (secondRow, 8);

    freezeButton.setBounds (secondRow.removeFromLeft (78));
    addGap (secondRow, 8);

    clearReferencesButton.setBounds (secondRow.removeFromLeft (86));
    addGap (secondRow, 8);

    differenceButton.setBounds (secondRow.removeFromLeft (54));
    addGap (secondRow, 8);

    tooltipButton.setBounds (secondRow.removeFromLeft (54));
    addGap (secondRow, 8);

    inspectButton.setBounds (secondRow.removeFromLeft (128));
}

void PluginEditor::timerCallback()
{
    spectrumDisplay.setInputLevelDb (processorRef.getInputLevelDb());
    spectrumDisplay.setMinimumDecibels (processorRef.getAnalyzerMinimumDecibels());
    spectrumDisplay.setSlopeDbPerOctave (processorRef.getAnalyzerSlopeDbPerOctave());
    spectrumDisplay.setDisplayResolution (
        processorRef.getAnalyzerDisplayResolution());

    spectrumDisplay.setCurveVisibility (
        processorRef.shouldShowLiveCurve(),
        processorRef.shouldShowRmsCurve(),
        processorRef.shouldShowEnergyCurve(),
        processorRef.shouldShowPeakHoldCurve());

    spectrumDisplay.setPeakDipMarkersVisible (
        processorRef.shouldShowPeakDipMarkers());

    spectrumDisplay.setPeakDipCurveSource (
        processorRef.getPeakDipSource());

    spectrumDisplay.setDifferenceCurveVisible (
        processorRef.shouldShowDifferenceCurve());

    spectrumDisplay.setDifferenceCurveSource (
        processorRef.getDifferenceCurveSource());

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
    const auto hasReference =
        spectrumDisplay.hasFrozenReferenceSpectrum();

    freezeButton.setButtonText ("Add Ref");
    freezeButton.setToggleState (false, juce::dontSendNotification);
    clearReferencesButton.setEnabled (hasReference);
}

void PluginEditor::setTooltipsEnabled (bool shouldBeEnabled)
{
    tooltipWindow.setTooltipsEnabled (shouldBeEnabled);
}
