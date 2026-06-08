#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cmath>

//==============================================================================
PluginProcessor::PluginProcessor()
    : AudioProcessor (BusesProperties()
#if !JucePlugin_IsMidiEffect
    #if !JucePlugin_IsSynth
              .withInput ("Input", juce::AudioChannelSet::stereo(), true)
    #endif
              .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
#endif
              ),
      parameters (*this, nullptr, "Parameters", createParameterLayout())
{
    inputModeParameter = parameters.getRawParameterValue (inputModeParamId);
    fftSizeParameter = parameters.getRawParameterValue (fftSizeParamId);
    peakHoldDecayParameter = parameters.getRawParameterValue (peakHoldDecayParamId);
    rmsTimeParameter = parameters.getRawParameterValue (rmsTimeParamId);
    dbRangeParameter = parameters.getRawParameterValue (dbRangeParamId);
    slopeParameter = parameters.getRawParameterValue (slopeParamId);
    displayResolutionParameter =
        parameters.getRawParameterValue (displayResolutionParamId);
    vqtLiveCurveProfileParameter =
        parameters.getRawParameterValue (vqtLiveCurveProfileParamId);

    jassert (inputModeParameter != nullptr);
    jassert (fftSizeParameter != nullptr);
    jassert (peakHoldDecayParameter != nullptr);
    jassert (rmsTimeParameter != nullptr);
    jassert (dbRangeParameter != nullptr);
    jassert (slopeParameter != nullptr);
    jassert (displayResolutionParameter != nullptr);
    jassert (vqtLiveCurveProfileParameter != nullptr);
}

juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { showLiveCurveParamId, 1 },
        "Show Live Curve",
        true));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { showRmsCurveParamId, 1 },
        "Show RMS Curve",
        true));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { showPeakHoldCurveParamId, 1 },
        "Show Peak Hold Curve",
        true));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { showEnergyCurveParamId, 1 },
        "Show Energy Curve",
        true));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { showPeakDipMarkersParamId, 1 },
        "Show Peak/Dip Markers",
        false));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { showDifferenceCurveParamId, 1 },
        "Show Difference Curve",
        false));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { inputModeParamId, 1 },
        "Input Mode",
        getAnalyzerInputModeChoices(),
        0));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { fftSizeParamId, 1 },
        "FFT Size",
        getAnalyzerFftSizeChoices(),
        1));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { peakHoldDecayParamId, 1 },
        "Peak Hold Decay",
        getAnalyzerPeakHoldDecayChoices(),
        3));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { rmsTimeParamId, 1 },
        "RMS Time",
        getAnalyzerRmsTimeChoices(),
        1));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { dbRangeParamId, 1 },
        "dB Range",
        getAnalyzerDbRangeChoices(),
        2));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { slopeParamId, 1 },
        "Weighting",
        getAnalyzerSlopeChoices(),
        0));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { displayResolutionParamId, 1 },
        "Display Resolution",
        getAnalyzerDisplayResolutionChoices(),
        analyzerDisplayResolutionToIndex (
            AnalyzerDisplayResolution::highResolution)));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { vqtLiveCurveProfileParamId, 1 },
        "VQT Live Curve",
        getAnalyzerVqtLiveCurveProfileChoices(),
        analyzerVqtLiveCurveProfileToIndex (AnalyzerVqtLiveCurveProfile::balanced)));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { peakDipSourceParamId, 1 },
        "Peak/Dip Source",
        getAnalyzerCurveSourceChoices(),
        analyzerCurveSourceToIndex (AnalyzerCurveSource::live)));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { differenceCurveSourceParamId, 1 },
        "Difference Source",
        getAnalyzerCurveSourceChoices(),
        analyzerCurveSourceToIndex (AnalyzerCurveSource::live)));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { showStereoMeterParamId, 1 },
        "Show Stereo Meter",
        true));

    return { params.begin(), params.end() };
}

PluginProcessor::~PluginProcessor()
{
    analyzerEngine.stop();
    secondaryAnalyzerEngine.stop();
}

//==============================================================================
const juce::String PluginProcessor::getName() const
{
    return JucePlugin_Name;
}

bool PluginProcessor::acceptsMidi() const
{
#if JucePlugin_WantsMidiInput
    return true;
#else
    return false;
#endif
}

bool PluginProcessor::producesMidi() const
{
#if JucePlugin_ProducesMidiOutput
    return true;
#else
    return false;
#endif
}

bool PluginProcessor::isMidiEffect() const
{
#if JucePlugin_IsMidiEffect
    return true;
#else
    return false;
#endif
}

double PluginProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int PluginProcessor::getNumPrograms()
{
    return 1; // NB: some hosts don't cope very well if you tell them there are 0 programs,
    // so this should be at least 1, even if you're not really implementing programs.
}

int PluginProcessor::getCurrentProgram()
{
    return 0;
}

void PluginProcessor::setCurrentProgram (int index)
{
    juce::ignoreUnused (index);
}

const juce::String PluginProcessor::getProgramName (int index)
{
    juce::ignoreUnused (index);
    return {};
}

void PluginProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}

AnalyzerInputMode PluginProcessor::getAnalyzerInputMode() const noexcept
{
    if (inputModeParameter == nullptr)
        return AnalyzerInputMode::stereoSum;

    return analyzerInputModeFromParameterValue (
        inputModeParameter->load (std::memory_order_relaxed));
}

juce::String PluginProcessor::getAnalyzerCurveLabelForMode (AnalyzerInputMode mode)
{
    switch (mode)
    {
        case AnalyzerInputMode::stereoSum:     return "Main";
        case AnalyzerInputMode::left:          return "L";
        case AnalyzerInputMode::right:         return "R";
        case AnalyzerInputMode::mid:           return "M";
        case AnalyzerInputMode::side:          return "S";
        case AnalyzerInputMode::leftRightDual: return "L";
        case AnalyzerInputMode::midSideDual:   return "M";
        case AnalyzerInputMode::count:         break;
    }

    return "Main";
}

juce::String PluginProcessor::getPrimaryAnalyzerCurveLabel() const
{
    const auto selectedMode = getAnalyzerInputMode();

    return getAnalyzerCurveLabelForMode (
        analyzerInputModeGetPrimaryMode (selectedMode));
}

juce::String PluginProcessor::getSecondaryAnalyzerCurveLabel() const
{
    const auto selectedMode = getAnalyzerInputMode();

    if (!analyzerInputModeIsDual (selectedMode))
        return {};

    return getAnalyzerCurveLabelForMode (
        analyzerInputModeGetSecondaryMode (selectedMode));
}

bool PluginProcessor::isSecondaryAnalyzerActive() const noexcept
{
    return secondaryAnalyzerEnabled.load (std::memory_order_relaxed);
}

void PluginProcessor::syncSecondaryAnalyzerRuntimeForCurrentInputMode()
{
    const auto shouldRunSecondary =
        analyzerInputModeIsDual (getAnalyzerInputMode());

    const auto isSecondaryThreadStarted =
        secondaryAnalyzerThreadStarted.load (std::memory_order_relaxed);

    if (shouldRunSecondary && !isSecondaryThreadStarted)
    {
        secondaryAnalyzerEngine.start();
        secondaryAnalyzerThreadStarted.store (true, std::memory_order_relaxed);
    }
    else if (!shouldRunSecondary && isSecondaryThreadStarted)
    {
        secondaryAnalyzerEngine.stop();
        secondaryAnalyzerFifo.reset();
        secondaryAnalyzerThreadStarted.store (false, std::memory_order_relaxed);
    }

    secondaryAnalyzerEnabled.store (
        shouldRunSecondary,
        std::memory_order_relaxed);
}

bool PluginProcessor::copyLatestAnalyzerFrameBundle (
    AnalyzerFrameBundle& destination)
{
    destination.primaryLabel = getPrimaryAnalyzerCurveLabel();
    destination.secondaryLabel = getSecondaryAnalyzerCurveLabel();
    destination.hasPrimary = analyzerEngine.copyLatestFrame (destination.primary);

    const auto secondaryEnabled =
        secondaryAnalyzerEnabled.load (std::memory_order_relaxed);

    destination.hasSecondary =
        secondaryEnabled
        && secondaryAnalyzerEngine.copyLatestFrame (destination.secondary);

    return destination.hasPrimary || destination.hasSecondary;
}

PluginProcessor::StereoMeterSnapshot PluginProcessor::getStereoMeterSnapshot() const noexcept
{
    StereoMeterSnapshot snapshot;
    snapshot.correlation = stereoCorrelation.load (std::memory_order_relaxed);
    snapshot.smoothedCorrelation =
        stereoSmoothedCorrelation.load (std::memory_order_relaxed);
    snapshot.leftLevelDb = stereoLeftLevelDb.load (std::memory_order_relaxed);
    snapshot.rightLevelDb = stereoRightLevelDb.load (std::memory_order_relaxed);
    snapshot.midLevelDb = stereoMidLevelDb.load (std::memory_order_relaxed);
    snapshot.sideLevelDb = stereoSideLevelDb.load (std::memory_order_relaxed);
    snapshot.balanceDb = stereoBalanceDb.load (std::memory_order_relaxed);
    snapshot.widthPercent = stereoWidthPercent.load (std::memory_order_relaxed);
    snapshot.monoCompatibilityDb =
        stereoMonoCompatibilityDb.load (std::memory_order_relaxed);

    return snapshot;
}

void PluginProcessor::copyGoniometerPoints (
    std::vector<juce::Point<float>>& destination) const
{
    destination.resize (static_cast<size_t> (goniometerPointCount));

    const auto rawWriteIndex =
        goniometerWriteIndex.load (std::memory_order_relaxed);

    auto writeIndex = rawWriteIndex % goniometerPointCount;

    if (writeIndex < 0)
        writeIndex += goniometerPointCount;

    for (int i = 0; i < goniometerPointCount; ++i)
    {
        const auto index =
            static_cast<size_t> ((writeIndex + i) % goniometerPointCount);

        destination[static_cast<size_t> (i)] = {
            goniometerX[index].load (std::memory_order_relaxed),
            goniometerY[index].load (std::memory_order_relaxed)
        };
    }
}

float PluginProcessor::getPeakHoldDecayDbPerSecond() const noexcept
{
    if (peakHoldDecayParameter == nullptr)
        return 8.0f;

    return analyzerPeakHoldDecayFromParameterValue (
        peakHoldDecayParameter->load (std::memory_order_relaxed));
}

float PluginProcessor::getRmsTimeSeconds() const noexcept
{
    if (rmsTimeParameter == nullptr)
        return 0.300f;

    return analyzerRmsTimeSecondsFromParameterValue (
        rmsTimeParameter->load (std::memory_order_relaxed));
}

AnalyzerVqtLiveCurveProfile PluginProcessor::getVqtLiveCurveProfile() const noexcept
{
    if (vqtLiveCurveProfileParameter == nullptr)
        return AnalyzerVqtLiveCurveProfile::balanced;

    return analyzerVqtLiveCurveProfileFromParameterValue (
        vqtLiveCurveProfileParameter->load (std::memory_order_relaxed));
}

float PluginProcessor::getAnalyzerMinimumDecibels() const noexcept
{
    if (dbRangeParameter == nullptr)
        return -100.0f;

    return analyzerDbRangeMinimumDecibelsFromParameterValue (
        dbRangeParameter->load (std::memory_order_relaxed));
}

float PluginProcessor::getAnalyzerSlopeDbPerOctave() const noexcept
{
    if (slopeParameter == nullptr)
        return 0.0f;

    return analyzerSlopeDbPerOctaveFromParameterValue (
        slopeParameter->load (std::memory_order_relaxed));
}

AnalyzerDisplayResolution PluginProcessor::getAnalyzerDisplayResolution() const noexcept
{
    if (displayResolutionParameter == nullptr)
        return AnalyzerDisplayResolution::highResolution;

    return analyzerDisplayResolutionFromParameterValue (
        displayResolutionParameter->load (std::memory_order_relaxed));
}

AnalyzerCurveSource PluginProcessor::getPeakDipSource() const noexcept
{
    const auto* parameter = parameters.getRawParameterValue (peakDipSourceParamId);

    if (parameter == nullptr)
        return AnalyzerCurveSource::live;

    return analyzerCurveSourceFromParameterValue (
        parameter->load (std::memory_order_relaxed));
}

AnalyzerCurveSource PluginProcessor::getDifferenceCurveSource() const noexcept
{
    const auto* parameter = parameters.getRawParameterValue (differenceCurveSourceParamId);

    if (parameter == nullptr)
        return AnalyzerCurveSource::live;

    return analyzerCurveSourceFromParameterValue (
        parameter->load (std::memory_order_relaxed));
}

void PluginProcessor::configureAnalyzerEngineForCurrentSettings (
    AnalyzerEngine& engine) noexcept
{
    engine.setRequestedFftOrder (getAnalyzerFftOrder());
    engine.setFrequencyDependentResolutionEnabled (
        isFrequencyDependentAnalyzerResolution());
    engine.setFrequencyDependentTunedResolutionEnabled (
        isFrequencyDependentAnalyzerResolutionTuned());
    engine.setVqtLikeFilterbankEnabled (isVqtLikeAnalyzerFilterbank());
    engine.setVqtLikeLiveCurveProfile (getVqtLiveCurveProfile());
    engine.setPeakHoldDecayDbPerSecond (getPeakHoldDecayDbPerSecond());
    engine.setRmsTimeSeconds (getRmsTimeSeconds());
}

void PluginProcessor::updateStereoMeterData (
    const juce::AudioBuffer<float>& buffer,
    int numInputChannels) noexcept
{
    const auto numSamples = buffer.getNumSamples();

    if (numSamples <= 0 || numInputChannels <= 0 || buffer.getNumChannels() <= 0)
        return;

    const auto* left = buffer.getReadPointer (0);
    const auto* right =
        numInputChannels > 1 && buffer.getNumChannels() > 1
            ? buffer.getReadPointer (1)
            : left;

    double sumL2 = 0.0;
    double sumR2 = 0.0;
    double sumLR = 0.0;
    double sumMid2 = 0.0;
    double sumSide2 = 0.0;

    const auto goniometerStep = juce::jmax (1, numSamples / 32);
    constexpr auto invSqrt2 = 0.70710678118f;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const auto leftSample = left[sample];
        const auto rightSample = right[sample];
        const auto midSample = 0.5f * (leftSample + rightSample);
        const auto sideSample = 0.5f * (leftSample - rightSample);

        sumL2 += static_cast<double> (leftSample) * leftSample;
        sumR2 += static_cast<double> (rightSample) * rightSample;
        sumLR += static_cast<double> (leftSample) * rightSample;
        sumMid2 += static_cast<double> (midSample) * midSample;
        sumSide2 += static_cast<double> (sideSample) * sideSample;

        if (sample % goniometerStep == 0)
        {
            const auto rawIndex =
                goniometerWriteIndex.fetch_add (1, std::memory_order_relaxed);

            auto wrappedIndex = rawIndex % goniometerPointCount;

            if (wrappedIndex < 0)
                wrappedIndex += goniometerPointCount;

            const auto index =
                static_cast<size_t> (wrappedIndex);

            const auto x =
                juce::jlimit (-1.0f,
                    1.0f,
                    invSqrt2 * (leftSample - rightSample));

            const auto y =
                juce::jlimit (-1.0f,
                    1.0f,
                    invSqrt2 * (leftSample + rightSample));

            goniometerX[index].store (x, std::memory_order_relaxed);
            goniometerY[index].store (y, std::memory_order_relaxed);
        }
    }

    constexpr auto epsilon = 1.0e-9;
    const auto sampleCount = static_cast<double> (numSamples);
    const auto leftRms = std::sqrt (sumL2 / sampleCount);
    const auto rightRms = std::sqrt (sumR2 / sampleCount);
    const auto midRms = std::sqrt (sumMid2 / sampleCount);
    const auto sideRms = std::sqrt (sumSide2 / sampleCount);

    auto correlation = 1.0f;

    if (numInputChannels > 1 && sumL2 > epsilon && sumR2 > epsilon)
    {
        correlation = static_cast<float> (
            sumLR / std::sqrt (sumL2 * sumR2));
    }

    correlation = juce::jlimit (-1.0f, 1.0f, correlation);

    const auto leftDb =
        juce::Decibels::gainToDecibels (static_cast<float> (leftRms), -100.0f);

    const auto rightDb =
        juce::Decibels::gainToDecibels (static_cast<float> (rightRms), -100.0f);

    const auto midDb =
        juce::Decibels::gainToDecibels (static_cast<float> (midRms), -100.0f);

    const auto sideDb =
        juce::Decibels::gainToDecibels (static_cast<float> (sideRms), -100.0f);

    const auto previousSmoothed =
        stereoSmoothedCorrelation.load (std::memory_order_relaxed);

    const auto smoothed =
        previousSmoothed + 0.08f * (correlation - previousSmoothed);

    const auto widthPercent =
        numInputChannels > 1
            ? juce::jlimit (0.0f,
                  300.0f,
                  100.0f * static_cast<float> (
                               sideRms / juce::jmax (midRms, epsilon)))
            : 0.0f;

    const auto monoCompatibilityDb =
        midDb - juce::jmax (leftDb, rightDb);

    stereoCorrelation.store (correlation, std::memory_order_relaxed);
    stereoSmoothedCorrelation.store (smoothed, std::memory_order_relaxed);
    stereoLeftLevelDb.store (leftDb, std::memory_order_relaxed);
    stereoRightLevelDb.store (rightDb, std::memory_order_relaxed);
    stereoMidLevelDb.store (midDb, std::memory_order_relaxed);
    stereoSideLevelDb.store (numInputChannels > 1 ? sideDb : -100.0f,
        std::memory_order_relaxed);
    stereoBalanceDb.store (numInputChannels > 1 ? rightDb - leftDb : 0.0f,
        std::memory_order_relaxed);
    stereoWidthPercent.store (widthPercent, std::memory_order_relaxed);
    stereoMonoCompatibilityDb.store (monoCompatibilityDb, std::memory_order_relaxed);
}

bool PluginProcessor::isFrequencyDependentAnalyzerResolution() const noexcept
{
    if (fftSizeParameter == nullptr)
        return false;

    if (isVqtLikeAnalyzerFilterbank())
        return false;

    return analyzerFftSizeIsFrequencyDependent (
        fftSizeParameter->load (std::memory_order_relaxed));
}

bool PluginProcessor::isFrequencyDependentAnalyzerResolutionTuned() const noexcept
{
    if (fftSizeParameter == nullptr)
        return false;
    return analyzerFftSizeIsFrequencyDependentTuned (fftSizeParameter->load (std::memory_order_relaxed));
}

bool PluginProcessor::isVqtLikeAnalyzerFilterbank() const noexcept
{
    if (fftSizeParameter == nullptr)
        return false;

    return analyzerFftSizeIsVqtLike (
        fftSizeParameter->load (std::memory_order_relaxed));
}

//==============================================================================

void PluginProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    analyzerEngine.stop();
    secondaryAnalyzerEngine.stop();

    analyzerFifo.prepare (sampleRate, samplesPerBlock);
    secondaryAnalyzerFifo.prepare (sampleRate, samplesPerBlock);

    configureAnalyzerEngineForCurrentSettings (analyzerEngine);
    configureAnalyzerEngineForCurrentSettings (secondaryAnalyzerEngine);

    analyzerEngine.prepare (sampleRate, analyzerFifo);
    secondaryAnalyzerEngine.prepare (sampleRate, secondaryAnalyzerFifo);

    analyzerEngine.start();

    const auto selectedInputMode = getAnalyzerInputMode();
    const auto shouldEnableSecondary =
        analyzerInputModeIsDual (selectedInputMode);

    secondaryAnalyzerEnabled.store (
        shouldEnableSecondary,
        std::memory_order_relaxed);

    secondaryAnalyzerThreadStarted.store (
        shouldEnableSecondary,
        std::memory_order_relaxed);

    if (shouldEnableSecondary)
        secondaryAnalyzerEngine.start();
}

void PluginProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
    analyzerEngine.stop();
    secondaryAnalyzerEngine.stop();
    analyzerFifo.reset();
    secondaryAnalyzerFifo.reset();
    secondaryAnalyzerEnabled.store (false, std::memory_order_relaxed);
    secondaryAnalyzerThreadStarted.store (false, std::memory_order_relaxed);
}

bool PluginProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
#if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
#else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
    #if !JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
    #endif

    return true;
#endif
}

void PluginProcessor::processBlock (juce::AudioBuffer<float>& buffer,
    juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);

    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    float maxSample = 0.0f;

    const auto numChannelsToAnalyse =
        juce::jmin (totalNumInputChannels, buffer.getNumChannels());

    for (int channel = 0; channel < numChannelsToAnalyse; ++channel)
    {
        maxSample = juce::jmax (
            maxSample,
            buffer.getMagnitude (channel, 0, buffer.getNumSamples()));
    }

    const auto levelDb = juce::Decibels::gainToDecibels (maxSample, -100.0f);
    inputLevelDb.store (levelDb, std::memory_order_relaxed);

    updateStereoMeterData (buffer, numChannelsToAnalyse);

    configureAnalyzerEngineForCurrentSettings (analyzerEngine);
    configureAnalyzerEngineForCurrentSettings (secondaryAnalyzerEngine);

    const auto selectedInputMode = getAnalyzerInputMode();
    const auto primaryMode =
        analyzerInputModeGetPrimaryMode (selectedInputMode);

    const auto secondaryMode =
        analyzerInputModeGetSecondaryMode (selectedInputMode);

    const auto dualEnabled =
        analyzerInputModeIsDual (selectedInputMode);

    secondaryAnalyzerEnabled.store (dualEnabled, std::memory_order_relaxed);

    analyzerFifo.pushMonoFromBuffer (
        buffer,
        numChannelsToAnalyse,
        primaryMode);

    if (dualEnabled)
    {
        secondaryAnalyzerFifo.pushMonoFromBuffer (
            buffer,
            numChannelsToAnalyse,
            secondaryMode);
    }
}

//==============================================================================
bool PluginProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new PluginEditor (*this);
}

//==============================================================================
void PluginProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.

    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());

    copyXmlToBinary (*xml, destData);
}

void PluginProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));

    if (xmlState != nullptr)
    {
        if (xmlState->hasTagName (parameters.state.getType()))
            parameters.replaceState (juce::ValueTree::fromXml (*xmlState));
    }
}

int PluginProcessor::getAnalyzerFftOrder() const noexcept
{
    if (fftSizeParameter == nullptr)
        return 11;

    return analyzerFftOrderFromParameterValue (
        fftSizeParameter->load (std::memory_order_relaxed));
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}
