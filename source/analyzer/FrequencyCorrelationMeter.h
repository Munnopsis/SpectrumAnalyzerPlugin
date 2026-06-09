#pragma once

#include <array>
#include <atomic>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>

class FrequencyCorrelationMeter
{
public:
    static constexpr int numBands = 31;

    struct Band
    {
        float centreFrequencyHz = 0.0f;
        float minimumFrequencyHz = 0.0f;
        float maximumFrequencyHz = 0.0f;
        float correlation = 0.0f;
        float smoothedCorrelation = 0.0f;
        float energyDb = -100.0f;
        bool valid = false;
    };

    struct Snapshot
    {
        std::array<Band, numBands> bands {};
    };

    void prepare (double sampleRate, int maximumBlockSize);
    void reset() noexcept;
    void processBlock (const juce::AudioBuffer<float>& buffer) noexcept;
    void processBlock (const juce::AudioBuffer<float>& buffer,
                       int numInputChannels) noexcept;
    Snapshot getSnapshot() const noexcept;

private:
    struct Biquad
    {
        double b0 = 1.0;
        double b1 = 0.0;
        double b2 = 0.0;
        double a1 = 0.0;
        double a2 = 0.0;
        double z1 = 0.0;
        double z2 = 0.0;

        float process (float input) noexcept;
        void reset() noexcept;
        void setBandPass (double sampleRate,
                          double centreFrequencyHz,
                          double q) noexcept;
    };

    static constexpr int maxChannels = 2;
    static constexpr float minimumFrequencyHz = 20.0f;
    static constexpr float maximumFrequencyHz = 20000.0f;
    static constexpr float smoothingAlpha = 0.18f;
    static constexpr float invalidReleaseAlpha = 0.08f;
    static constexpr double lowEnergyMeanSquare = 1.0e-10;

    void updateBands() noexcept;
    void updateFilterCoefficients() noexcept;
    void finishMeasurementBlock() noexcept;
    static float decibelsFromBandEnergy (double meanPower) noexcept;

    double currentSampleRate = 44100.0;
    int samplesPerMeasurementBlock = 4410;
    int samplesInCurrentBlock = 0;

    std::array<std::array<Biquad, numBands>, maxChannels> filters {};
    std::array<double, numBands> blockCross {};
    std::array<double, numBands> blockPowerL {};
    std::array<double, numBands> blockPowerR {};

    std::array<float, numBands> bandCentres {};
    std::array<float, numBands> bandMinimums {};
    std::array<float, numBands> bandMaximums {};
    std::array<std::atomic<float>, numBands> bandCorrelations {};
    std::array<std::atomic<float>, numBands> bandSmoothedCorrelations {};
    std::array<std::atomic<float>, numBands> bandEnergyDb {};
    std::array<std::atomic<bool>, numBands> bandValid {};
};
