#include "PluginEditor.h"
#include "analyzer/AnalyzerInputMode.h"
#include "analyzer/AnalyzerFftSize.h"
#include "analyzer/AnalyzerFrequencyRange.h"
#include "analyzer/AnalyzerPeakHoldDecay.h"
#include "analyzer/AnalyzerRmsTime.h"
#include "analyzer/AnalyzerDbRange.h"
#include "analyzer/AnalyzerSlope.h"
#include "analyzer/AnalyzerDisplayResolution.h"
#include "analyzer/AnalyzerValidationSignal.h"
#include "analyzer/AnalyzerVqtLiveCurveProfile.h"

PluginEditor::PluginEditor (PluginProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    juce::ignoreUnused (processorRef);

    addAndMakeVisible (spectrumDisplay);
    spectrumDisplay.setName ("SpectrumDisplay");
    spectrumDisplay.onAudioFilesDropped =
    [this] (const juce::StringArray& files)
    {
        auto addedReference = false;

        for (const auto& path : files)
        {
            const auto file =
                juce::File (path);

            const auto extension =
                file.getFileExtension().toLowerCase();

            if (extension != ".wav"
                && extension != ".wave"
                && extension != ".aif"
                && extension != ".aiff")
            {
                continue;
            }

            const auto newReferenceIndex =
                processorRef.addReferenceFromAudioFile (file);

            if (newReferenceIndex >= 0)
            {
                processorRef.setActiveReferenceIndex (newReferenceIndex);
                addedReference = true;
                break;
            }
        }

        if (addedReference)
        {
            updateReferenceControls();
            return;
        }

        juce::AlertWindow::showMessageBoxAsync (
            juce::MessageBoxIconType::WarningIcon,
            "Audio Reference",
            "Could not create a reference from the dropped file. "
            "Please use a WAV, AIFF or AIF file.");
    };
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
    addAndMakeVisible (referenceMenuButton);
    addAndMakeVisible (differenceButton);
    addAndMakeVisible (stereoMeterButton);
    addAndMakeVisible (loudnessMeterButton);
    addAndMakeVisible (frequencyCorrelationButton);
    addAndMakeVisible (resetLoudnessButton);
    addAndMakeVisible (tooltipButton);
    addAndMakeVisible (inputModeBox);
    addAndMakeVisible (fftSizeBox);
    addAndMakeVisible (peakHoldDecayBox);
    addAndMakeVisible (rmsTimeBox);
    addAndMakeVisible (dbRangeBox);
    addAndMakeVisible (slopeBox);
    addAndMakeVisible (displayResolutionBox);
    addAndMakeVisible (vqtLiveCurveBox);
    addAndMakeVisible (referenceBox);
    addAndMakeVisible (validationSignalBox);

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
    referenceMenuButton.setName ("ReferenceMenuButton");
    differenceButton.setName ("DifferenceButton");
    stereoMeterButton.setName ("StereoMeterButton");
    loudnessMeterButton.setName ("LoudnessMeterButton");
    frequencyCorrelationButton.setName ("FrequencyCorrelationButton");
    resetLoudnessButton.setName ("ResetLoudnessButton");
    tooltipButton.setName ("TooltipButton");
    inputModeBox.setName ("InputModeBox");
    fftSizeBox.setName ("FftSizeBox");
    peakHoldDecayBox.setName ("PeakHoldDecayBox");
    rmsTimeBox.setName ("RmsTimeBox");
    dbRangeBox.setName ("DbRangeBox");
    slopeBox.setName ("SlopeBox");
    displayResolutionBox.setName ("DisplayResolutionBox");
    vqtLiveCurveBox.setName ("VqtLiveCurveBox");
    referenceBox.setName ("ReferenceBox");
    validationSignalBox.setName ("ValidationSignalBox");

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
    referenceMenuButton.setWantsKeyboardFocus (true);
    differenceButton.setWantsKeyboardFocus (true);
    stereoMeterButton.setWantsKeyboardFocus (true);
    loudnessMeterButton.setWantsKeyboardFocus (true);
    frequencyCorrelationButton.setWantsKeyboardFocus (true);
    resetLoudnessButton.setWantsKeyboardFocus (true);
    tooltipButton.setWantsKeyboardFocus (true);
    inputModeBox.setWantsKeyboardFocus (true);
    fftSizeBox.setWantsKeyboardFocus (true);
    peakHoldDecayBox.setWantsKeyboardFocus (true);
    rmsTimeBox.setWantsKeyboardFocus (true);
    dbRangeBox.setWantsKeyboardFocus (true);
    slopeBox.setWantsKeyboardFocus (true);
    displayResolutionBox.setWantsKeyboardFocus (true);
    vqtLiveCurveBox.setWantsKeyboardFocus (true);
    referenceBox.setWantsKeyboardFocus (true);
    validationSignalBox.setWantsKeyboardFocus (true);

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
    referenceMenuButton.setTooltip ("Rename, remove or show/hide the selected reference curve");
    differenceButton.setTooltip ("Show difference between current analyzer curve and the active reference");
    stereoMeterButton.setTooltip ("Show stereo correlation, balance, width and phase scope");
    loudnessMeterButton.setTooltip ("Show momentary, short-term and integrated loudness metering");
    frequencyCorrelationButton.setTooltip ("Show 31-band stereo frequency correlation");
    resetLoudnessButton.setTooltip ("Reset integrated loudness and peak hold metering");
    tooltipButton.setTooltip ("Show or hide tooltips");
    validationSignalBox.setTooltip (
        "Internal analyzer-only validation signal. Does not affect audio output.");

    clearPeakButton.onClick = [this]
    {
        processorRef.requestClearPeakHold();
    };

    clearEnergyButton.onClick = [this]
    {
        processorRef.requestClearEnergy();
    };

    resetLoudnessButton.onClick = [this]
    {
        processorRef.resetLoudnessMeter();
    };

    freezeButton.onClick = [this]
    {
        processorRef.addReferenceFromCurrentAnalyzerFrame();
        updateReferenceControls();
    };

    referenceMenuButton.onClick = [this]
    {
        showReferenceMenu();
    };

    clearReferencesButton.onClick = [this]
    {
        processorRef.clearReferenceCurves();
        updateReferenceControls();
    };

    liveButton.setClickingTogglesState (true);
    rmsButton.setClickingTogglesState (true);
    energyButton.setClickingTogglesState (true);
    peakButton.setClickingTogglesState (true);
    peakDipButton.setClickingTogglesState (true);
    freezeButton.setClickingTogglesState (false);
    clearReferencesButton.setClickingTogglesState (false);
    referenceMenuButton.setClickingTogglesState (false);
    differenceButton.setClickingTogglesState (true);differenceButton.setClickingTogglesState (true);
    stereoMeterButton.setClickingTogglesState (true);
    loudnessMeterButton.setClickingTogglesState (true);
    frequencyCorrelationButton.setClickingTogglesState (true);
    resetLoudnessButton.setClickingTogglesState (false);
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

    validationSignalBox.addItemList (getAnalyzerValidationSignalChoices(), 1);
    validationSignalBox.setJustificationType (juce::Justification::centred);
    validationSignalBox.setTextWhenNothingSelected ("Test");

    validationSignalAttachment = std::make_unique<ComboBoxAttachment> (
        state,
        PluginProcessor::validationSignalParamId,
        validationSignalBox);

    referenceBox.setJustificationType (juce::Justification::centred);
    referenceBox.setTextWhenNothingSelected ("Reference");
    referenceBox.setTooltip ("Select the active reference curve for Difference view");

    referenceBox.onChange = [this]
    {
        if (updatingReferenceBox)
            return;

        const auto selectedIndex = referenceBox.getSelectedId() - 1;

        if (selectedIndex >= 0)
        {
            processorRef.setActiveReferenceIndex (selectedIndex);
            updateReferenceControls();
        }
    };

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

    stereoMeterButtonAttachment = std::make_unique<ButtonAttachment> (
        state,
        PluginProcessor::showStereoMeterParamId,
        stereoMeterButton);

    loudnessMeterButtonAttachment = std::make_unique<ButtonAttachment> (
        state,
        PluginProcessor::showLoudnessMeterParamId,
        loudnessMeterButton);

    frequencyCorrelationButtonAttachment = std::make_unique<ButtonAttachment> (
        state,
        PluginProcessor::showFrequencyCorrelationParamId,
        frequencyCorrelationButton);

    spectrumDisplay.setCurveVisibility (
        processorRef.shouldShowLiveCurve(),
        processorRef.shouldShowRmsCurve(),
        processorRef.shouldShowEnergyCurve(),
        processorRef.shouldShowPeakHoldCurve());

    spectrumDisplay.setStereoMeterVisible (
        processorRef.shouldShowStereoMeter());

    spectrumDisplay.setLoudnessMeterVisible (
        processorRef.shouldShowLoudnessMeter());

    spectrumDisplay.setFrequencyCorrelationVisible (
        processorRef.shouldShowFrequencyCorrelation());

    updateReferenceControls();

    inspectButton.onClick = [&]
    {
        if (! inspector)
        {
            inspector = std::make_unique<melatonin::Inspector> (*this);
            inspector->onClose = [this]() { inspector.reset(); };
        }

        inspector->setVisible (true);
    };

    setSize (1520, 680);

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
    addGap (firstRow, 8);

    referenceBox.setBounds (firstRow.removeFromLeft (172));
    addGap (firstRow, 8);

    referenceMenuButton.setBounds (firstRow.removeFromLeft (56));
    addGap (firstRow, 8);

    validationSignalBox.setBounds (firstRow.removeFromLeft (116));

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

    stereoMeterButton.setBounds (secondRow.removeFromLeft (72));
    addGap (secondRow, 8);

    loudnessMeterButton.setBounds (secondRow.removeFromLeft (58));
    addGap (secondRow, 8);

    frequencyCorrelationButton.setBounds (secondRow.removeFromLeft (58));
    addGap (secondRow, 8);

    resetLoudnessButton.setBounds (secondRow.removeFromLeft (74));
    addGap (secondRow, 8);

    tooltipButton.setBounds (secondRow.removeFromLeft (54));
    addGap (secondRow, 8);

    inspectButton.setBounds (secondRow.removeFromLeft (128));
}

void PluginEditor::timerCallback()
{
    processorRef.syncSecondaryAnalyzerRuntimeForCurrentInputMode();

    if (lastReferenceStateRevision != processorRef.getReferenceStateRevision())
        updateReferenceControls();

    spectrumDisplay.setInputLevelDb (processorRef.getInputLevelDb());
    spectrumDisplay.setMinimumDecibels (processorRef.getAnalyzerMinimumDecibels());
    spectrumDisplay.setSlopeDbPerOctave (processorRef.getAnalyzerSlopeDbPerOctave());
    spectrumDisplay.setDisplayResolution (
        processorRef.getAnalyzerDisplayResolution());

    const auto validationSignal =
        processorRef.getAnalyzerValidationSignal();

    spectrumDisplay.setValidationSignalLabels (
        getAnalyzerValidationSignalLabel (validationSignal),
        getAnalyzerValidationExpectedLabel (validationSignal));

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

    const auto stereoMeterVisible = processorRef.shouldShowStereoMeter();
    spectrumDisplay.setStereoMeterVisible (stereoMeterVisible);

    if (stereoMeterVisible)
    {
        const auto snapshot = processorRef.getStereoMeterSnapshot();

        stereoMeterDisplayData.correlation = snapshot.correlation;
        stereoMeterDisplayData.smoothedCorrelation = snapshot.smoothedCorrelation;
        stereoMeterDisplayData.correlationValid = snapshot.correlationValid;
        stereoMeterDisplayData.leftLevelDb = snapshot.leftLevelDb;
        stereoMeterDisplayData.rightLevelDb = snapshot.rightLevelDb;
        stereoMeterDisplayData.midLevelDb = snapshot.midLevelDb;
        stereoMeterDisplayData.sideLevelDb = snapshot.sideLevelDb;
        stereoMeterDisplayData.balanceDb = snapshot.balanceDb;
        stereoMeterDisplayData.widthPercent = snapshot.widthPercent;
        stereoMeterDisplayData.monoCompatibilityDb =
            snapshot.monoCompatibilityDb;

        processorRef.copyGoniometerPoints (
            stereoMeterDisplayData.goniometerPoints);

        spectrumDisplay.setStereoMeterData (stereoMeterDisplayData);
    }

    const auto loudnessMeterVisible =
        processorRef.shouldShowLoudnessMeter();

    spectrumDisplay.setLoudnessMeterVisible (loudnessMeterVisible);

    if (loudnessMeterVisible)
    {
        const auto snapshot = processorRef.getLoudnessSnapshot();

        loudnessMeterDisplayData.momentaryLufs = snapshot.momentaryLufs;
        loudnessMeterDisplayData.shortTermLufs = snapshot.shortTermLufs;
        loudnessMeterDisplayData.integratedLufs = snapshot.integratedLufs;
        loudnessMeterDisplayData.loudnessRangeLu = snapshot.loudnessRangeLu;
        loudnessMeterDisplayData.samplePeakDb = snapshot.samplePeakDb;
        loudnessMeterDisplayData.truePeakDb = snapshot.truePeakDb;
        loudnessMeterDisplayData.rmsDb = snapshot.rmsDb;
        loudnessMeterDisplayData.crestDb = snapshot.crestDb;
        loudnessMeterDisplayData.peakHoldDb = snapshot.peakHoldDb;
        loudnessMeterDisplayData.hasIntegratedMeasurement =
            snapshot.hasIntegratedMeasurement;
        loudnessMeterDisplayData.hasLoudnessRange =
            snapshot.hasLoudnessRange;
        loudnessMeterDisplayData.hasTruePeak = snapshot.hasTruePeak;

        spectrumDisplay.setLoudnessMeterData (loudnessMeterDisplayData);
    }

    const auto frequencyCorrelationVisible =
        processorRef.shouldShowFrequencyCorrelation();

    spectrumDisplay.setFrequencyCorrelationVisible (
        frequencyCorrelationVisible);

    if (frequencyCorrelationVisible)
    {
        const auto snapshot = processorRef.getFrequencyCorrelationSnapshot();

        for (size_t i = 0; i < frequencyCorrelationDisplayData.bands.size(); ++i)
        {
            const auto& source = snapshot.bands[i];
            auto& destination = frequencyCorrelationDisplayData.bands[i];

            destination.centreFrequencyHz = source.centreFrequencyHz;
            destination.correlation = source.correlation;
            destination.smoothedCorrelation = source.smoothedCorrelation;
            destination.valid = source.valid;
        }

        spectrumDisplay.setFrequencyCorrelationData (
            frequencyCorrelationDisplayData);
    }

    if (processorRef.copyLatestAnalyzerFrameBundle (analyzerFrameBundle))
    {
        if (analyzerFrameBundle.hasPrimary)
        {
            const auto& primary = analyzerFrameBundle.primary;

            std::vector<SpectrumDisplay::DisplayNotePeak> displayNotePeaks;
            displayNotePeaks.reserve (primary.notePeaks.size());

            for (const auto& notePeak : primary.notePeaks)
            {
                displayNotePeaks.push_back ({
                    notePeak.frequencyHz,
                    notePeak.decibels,
                    notePeak.midiNote,
                    notePeak.pitchClass
                });
            }

            spectrumDisplay.setAnalyzerFrameData (primary.dataMinFrequencyHz,
                                                  primary.dataMaxFrequencyHz,
                                                  primary.liveDb,
                                                  primary.peakHoldDb,
                                                  primary.rmsDb,
                                                  primary.energyDb,
                                                  displayNotePeaks);
        }

        if (analyzerFrameBundle.hasSecondary)
        {
            const auto& secondary = analyzerFrameBundle.secondary;

            spectrumDisplay.setSecondaryAnalyzerFrameData (
                true,
                analyzerFrameBundle.primaryLabel,
                analyzerFrameBundle.secondaryLabel,
                secondary.dataMinFrequencyHz,
                secondary.dataMaxFrequencyHz,
                secondary.liveDb,
                secondary.peakHoldDb,
                secondary.rmsDb,
                secondary.energyDb);

            return;
        }
    }

    if (!processorRef.isSecondaryAnalyzerActive())
    {
        spectrumDisplay.setSecondaryAnalyzerFrameData (
            false,
            processorRef.getPrimaryAnalyzerCurveLabel(),
            {},
            AnalyzerFrequencyRange::minimumHz,
            AnalyzerFrequencyRange::maximumHz,
            {},
            {},
            {},
            {});
    }
}

void PluginEditor::updateFreezeButtonState()
{
    const auto hasReference = processorRef.getNumReferenceCurves() > 0;

    freezeButton.setButtonText ("Add Ref");
    freezeButton.setToggleState (false, juce::dontSendNotification);
    clearReferencesButton.setEnabled (hasReference);
    referenceMenuButton.setEnabled (hasReference);
}

void PluginEditor::updateReferenceControls()
{
    const auto references =
        processorRef.getReferenceCurvesSnapshot();

    const auto activeReferenceIndex =
        processorRef.getActiveReferenceIndex();

    spectrumDisplay.setReferenceCurves (references, activeReferenceIndex);

    updatingReferenceBox = true;
    referenceBox.clear (juce::dontSendNotification);

    for (size_t i = 0; i < references.size(); ++i)
    {
        auto label = references[i].name;

        if (!references[i].visible)
            label += " (hidden)";

        referenceBox.addItem (label,
                              static_cast<int> (i) + 1);
    }

    if (activeReferenceIndex >= 0
        && activeReferenceIndex < static_cast<int> (references.size()))
    {
        referenceBox.setSelectedId (activeReferenceIndex + 1,
                                    juce::dontSendNotification);
    }
    else
    {
        referenceBox.setSelectedItemIndex (-1, juce::dontSendNotification);
    }

    const auto hasReferences =
        !references.empty();

    referenceBox.setEnabled (hasReferences);
    referenceMenuButton.setEnabled (hasReferences);

    updatingReferenceBox = false;

    lastReferenceStateRevision =
        processorRef.getReferenceStateRevision();

    updateFreezeButtonState();
}

void PluginEditor::showReferenceMenu()
{
    const auto references =
        processorRef.getReferenceCurvesSnapshot();

    const auto activeReferenceIndex =
        processorRef.getActiveReferenceIndex();

    const auto hasActiveReference =
        activeReferenceIndex >= 0
        && activeReferenceIndex < static_cast<int> (references.size());

    const auto activeReferenceVisible =
        hasActiveReference
            ? references[static_cast<size_t> (activeReferenceIndex)].visible
            : false;

    juce::PopupMenu menu;

    menu.addItem (1,
                  "Rename Reference",
                  hasActiveReference);

    menu.addItem (2,
                  "Remove Reference",
                  hasActiveReference);

    menu.addSeparator();

    menu.addItem (3,
                  activeReferenceVisible ? "Hide Reference" : "Show Reference",
                  hasActiveReference);

    juce::Component::SafePointer<PluginEditor> safeThis (this);

    menu.showMenuAsync (
        juce::PopupMenu::Options()
            .withTargetComponent (referenceMenuButton),
        [safeThis] (int result)
        {
            if (safeThis == nullptr || result == 0)
                return;

            auto& editor =
                *safeThis;

            const auto currentReferences =
                editor.processorRef.getReferenceCurvesSnapshot();

            const auto currentActiveIndex =
                editor.processorRef.getActiveReferenceIndex();

            const auto hasCurrentActiveReference =
                currentActiveIndex >= 0
                && currentActiveIndex < static_cast<int> (currentReferences.size());

            if (!hasCurrentActiveReference)
                return;

            switch (result)
            {
                case 1:
                    editor.showRenameReferenceDialog();
                    break;

                case 2:
                    editor.processorRef.removeReferenceCurve (currentActiveIndex);
                    editor.updateReferenceControls();
                    break;

                case 3:
                {
                    const auto currentlyVisible =
                        currentReferences[static_cast<size_t> (currentActiveIndex)].visible;

                    editor.processorRef.setReferenceCurveVisible (
                        currentActiveIndex,
                        !currentlyVisible);

                    editor.updateReferenceControls();
                    break;
                }

                default:
                    break;
            }
        });
}

void PluginEditor::showRenameReferenceDialog()
{
    const auto activeReferenceIndex =
        processorRef.getActiveReferenceIndex();

    if (activeReferenceIndex < 0)
        return;

    const auto currentName =
        processorRef.getReferenceCurveName (activeReferenceIndex);

    auto* alertWindow =
        new juce::AlertWindow ("Rename Reference",
                               "Enter a new name for the selected reference curve.",
                               juce::MessageBoxIconType::NoIcon,
                               this);

    alertWindow->addTextEditor ("name",
                                currentName,
                                "Name:");

    alertWindow->addButton ("Rename",
                            1,
                            juce::KeyPress (juce::KeyPress::returnKey));

    alertWindow->addButton ("Cancel",
                            0,
                            juce::KeyPress (juce::KeyPress::escapeKey));

    juce::Component::SafePointer<PluginEditor> safeThis (this);

    alertWindow->enterModalState (
        true,
        juce::ModalCallbackFunction::create (
            [safeThis, alertWindow, activeReferenceIndex] (int result)
            {
                if (safeThis == nullptr)
                    return;

                if (result != 1)
                    return;

                const auto newName =
                    alertWindow->getTextEditorContents ("name").trim();

                if (newName.isEmpty())
                    return;

                safeThis->processorRef.renameReferenceCurve (
                    activeReferenceIndex,
                    newName);

                safeThis->updateReferenceControls();
            }),
        true);
}

void PluginEditor::setTooltipsEnabled (bool shouldBeEnabled)
{
    tooltipWindow.setTooltipsEnabled (shouldBeEnabled);
}
