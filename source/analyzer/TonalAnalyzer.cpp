#include "TonalAnalyzer.h"

#include <algorithm>
#include <cmath>
#include <juce_core/juce_core.h>

TonalAnalyzer::TonalAnalyzer()
{
    reset();
}

void TonalAnalyzer::reset()
{
    pitchClassEnergy.fill (0.0f);
    totalEnergy = 0.0f;
}

int TonalAnalyzer::frequencyToMidiNote (float frequencyHz) noexcept
{
    if (frequencyHz <= 0.0f)
        return -1;

    const auto midi =
        juce::roundToInt (69.0f + 12.0f * std::log2 (frequencyHz / 440.0f));

    return juce::jlimit (0, 127, midi);
}

int TonalAnalyzer::midiNoteToPitchClass (int midiNote) noexcept
{
    if (midiNote < 0)
        return -1;

    return midiNote % numPitchClasses;
}

const char* TonalAnalyzer::getPitchClassName (int pitchClass) noexcept
{
    static constexpr std::array<const char*, numPitchClasses> names {
        "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
    };

    if (pitchClass < 0 || pitchClass >= numPitchClasses)
        return "-";

    return names[static_cast<size_t> (pitchClass)];
}

void TonalAnalyzer::addPeak (float frequencyHz, float magnitudeLinear)
{
    if (frequencyHz < minimumFrequencyHz || frequencyHz > maximumFrequencyHz)
        return;

    if (magnitudeLinear < minimumMagnitude)
        return;

    const auto midiNote = frequencyToMidiNote (frequencyHz);
    const auto pitchClass = midiNoteToPitchClass (midiNote);

    if (pitchClass < 0)
        return;

    const auto energy = magnitudeLinear * magnitudeLinear;

    pitchClassEnergy[static_cast<size_t> (pitchClass)] += energy;
    totalEnergy += energy;
}

TonalAnalyzer::ChromaSnapshot TonalAnalyzer::getSnapshot() const
{
    ChromaSnapshot snapshot;
    snapshot.pitchClassEnergy = pitchClassEnergy;
    snapshot.totalEnergy = totalEnergy;

    if (totalEnergy <= 0.0f)
        return snapshot;

    const auto maxEnergy =
        *std::max_element (snapshot.pitchClassEnergy.begin(),
                           snapshot.pitchClassEnergy.end());

    if (maxEnergy <= 0.0f)
        return snapshot;

    for (auto& value : snapshot.pitchClassEnergy)
        value /= maxEnergy;

    return snapshot;
}
