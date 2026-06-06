#pragma once

#include <array>

class TonalAnalyzer
{
public:
    static constexpr int numPitchClasses = 12;

    struct ChromaSnapshot
    {
        std::array<float, numPitchClasses> pitchClassEnergy {};
        float totalEnergy = 0.0f;
    };

    TonalAnalyzer();

    void reset();

    void addPeak (float frequencyHz, float magnitudeLinear);

    ChromaSnapshot getSnapshot() const;

    static int frequencyToMidiNote (float frequencyHz) noexcept;
    static int midiNoteToPitchClass (int midiNote) noexcept;
    static const char* getPitchClassName (int pitchClass) noexcept;

private:
    std::array<float, numPitchClasses> pitchClassEnergy {};
    float totalEnergy = 0.0f;

    static constexpr float minimumFrequencyHz = 40.0f;
    static constexpr float maximumFrequencyHz = 8000.0f;
    static constexpr float minimumMagnitude = 0.000001f;
};
