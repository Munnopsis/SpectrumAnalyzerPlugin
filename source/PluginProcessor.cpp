#include "PluginProcessor.h"
#include "PluginEditor.h"

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
    vqtLiveCurveProfileParameter =
        parameters.getRawParameterValue (vqtLiveCurveProfileParamId);

    jassert (inputModeParameter != nullptr);
    jassert (fftSizeParameter != nullptr);
    jassert (peakHoldDecayParameter != nullptr);
    jassert (rmsTimeParameter != nullptr);
    jassert (dbRangeParameter != nullptr);
    jassert (slopeParameter != nullptr);
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
        "Slope",
        getAnalyzerSlopeChoices(),
        0));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { vqtLiveCurveProfileParamId, 1 },
        "VQT Live Curve",
        getAnalyzerVqtLiveCurveProfileChoices(),
        analyzerVqtLiveCurveProfileToIndex (AnalyzerVqtLiveCurveProfile::balanced)));

    return { params.begin(), params.end() };
}

PluginProcessor::~PluginProcessor()
{
    analyzerEngine.stop();
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

    analyzerFifo.prepare (sampleRate, samplesPerBlock);

    analyzerEngine.setRequestedFftOrder (getAnalyzerFftOrder());
    analyzerEngine.setFrequencyDependentResolutionEnabled (
        isFrequencyDependentAnalyzerResolution());
    analyzerEngine.setFrequencyDependentTunedResolutionEnabled ( isFrequencyDependentAnalyzerResolutionTuned());
    analyzerEngine.setVqtLikeFilterbankEnabled (isVqtLikeAnalyzerFilterbank());
    analyzerEngine.setVqtLikeLiveCurveProfile (getVqtLiveCurveProfile());
    analyzerEngine.setPeakHoldDecayDbPerSecond (getPeakHoldDecayDbPerSecond());
    analyzerEngine.setRmsTimeSeconds (getRmsTimeSeconds());

    analyzerEngine.prepare (sampleRate, analyzerFifo);
    analyzerEngine.start();
}

void PluginProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
    analyzerEngine.stop();
    analyzerFifo.reset();
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

    analyzerEngine.setRequestedFftOrder (getAnalyzerFftOrder());
    analyzerEngine.setFrequencyDependentResolutionEnabled (
        isFrequencyDependentAnalyzerResolution());
    analyzerEngine.setFrequencyDependentTunedResolutionEnabled ( isFrequencyDependentAnalyzerResolutionTuned());
    analyzerEngine.setVqtLikeFilterbankEnabled (isVqtLikeAnalyzerFilterbank());
    analyzerEngine.setVqtLikeLiveCurveProfile (getVqtLiveCurveProfile());
    analyzerEngine.setPeakHoldDecayDbPerSecond (getPeakHoldDecayDbPerSecond());
    analyzerEngine.setRmsTimeSeconds (getRmsTimeSeconds());

    analyzerFifo.pushMonoFromBuffer (
        buffer,
        numChannelsToAnalyse,
        getAnalyzerInputMode());
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
