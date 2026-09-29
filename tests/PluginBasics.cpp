#include "helpers/test_helpers.h"
#include <PluginProcessor.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <cstring>

TEST_CASE ("Plugin instance", "[instance]")
{
    PluginProcessor testPlugin;

    SECTION ("name")
    {
        CHECK_THAT (testPlugin.getName().toStdString(),
            Catch::Matchers::Equals ("FullSpectrum"));
    }
}

TEST_CASE ("Analyzer leaves input audio unchanged", "[audio]")
{
    const auto channels = GENERATE (1, 2);
    const auto validationEnabled = GENERATE (false, true);
    PluginProcessor plugin;
    const auto channelSet = channels == 1 ? juce::AudioChannelSet::mono()
                                         : juce::AudioChannelSet::stereo();
    REQUIRE (plugin.setBusesLayout ({ { channelSet }, { channelSet } }));

    if (validationEnabled)
        plugin.getValueTreeState().getParameterAsValue (PluginProcessor::validationSignalParamId).setValue (1);

    plugin.prepareToPlay (48000.0, 256);
    juce::AudioBuffer<float> audio (channels, 256);
    for (int channel = 0; channel < channels; ++channel)
        for (int sample = 0; sample < audio.getNumSamples(); ++sample)
            audio.setSample (channel, sample, static_cast<float> ((sample % 31) - 15) / 32.0f);

    juce::AudioBuffer<float> original;
    original.makeCopyOf (audio);
    juce::MidiBuffer midi;
    plugin.processBlock (audio, midi);
    plugin.releaseResources();

    for (int channel = 0; channel < channels; ++channel)
        REQUIRE (std::memcmp (audio.getReadPointer (channel),
                             original.getReadPointer (channel),
                             static_cast<size_t> (audio.getNumSamples()) * sizeof (float)) == 0);
}

TEST_CASE ("Loading state restores settings and disables validation signals", "[state]")
{
    PluginProcessor plugin;
    auto& parameters = plugin.getValueTreeState();
    REQUIRE (plugin.getAnalyzerValidationSignal() == AnalyzerValidationSignal::off);

    parameters.getParameterAsValue (PluginProcessor::validationSignalParamId).setValue (1);
    parameters.getParameterAsValue (PluginProcessor::inputModeParamId).setValue (2);
    REQUIRE (plugin.getAnalyzerValidationSignal() != AnalyzerValidationSignal::off);
    juce::MemoryBlock state;
    plugin.getStateInformation (state);

    PluginProcessor restored;
    restored.setStateInformation (state.getData(), static_cast<int> (state.getSize()));
    CHECK (restored.getAnalyzerValidationSignal() == AnalyzerValidationSignal::off);
    CHECK_THAT (restored.getValueTreeState().getRawParameterValue (PluginProcessor::inputModeParamId)->load(),
                Catch::Matchers::WithinAbs (2.0, 0.0));
}


#ifdef PAMPLEJUCE_IPP
    #include <ipp.h>

TEST_CASE ("IPP version", "[ipp]")
{
    #if defined(__APPLE__)
        // macOS uses 2021.9.1 from pip wheel (only x86_64 version available)
        CHECK_THAT (ippsGetLibVersion()->Version, Catch::Matchers::Equals ("2021.9.1 (r0x7e208212)"));
    #else
        CHECK_THAT (ippsGetLibVersion()->Version, Catch::Matchers::Equals ("2026.0.0 (r0xa7ad6ebc)"));
    #endif
}
#endif
