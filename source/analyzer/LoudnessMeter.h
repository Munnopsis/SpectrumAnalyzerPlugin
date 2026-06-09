#pragma once

#include <array>
#include <atomic>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>

class LoudnessMeter
{
public:
    struct Snapshot
    {
        float momentaryLufs = -100.0f;
        float shortTermLufs = -100.0f;
        float integratedLufs = -100.0f;
        float loudnessRangeLu = 0.0f;
        float samplePeakDb = -100.0f;
        float truePeakDb = -100.0f;
        float rmsDb = -100.0f;
        float crestDb = 0.0f;
        float peakHoldDb = -100.0f;
        bool hasIntegratedMeasurement = false;
        bool hasLoudnessRange = false;
        bool hasTruePeak = false;
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
        void setCoefficients (double newB0,
                              double newB1,
                              double newB2,
                              double newA1,
                              double newA2) noexcept;
    };

    static constexpr int maxChannels = 2;
    static constexpr int momentaryBlockCount = 4;
    static constexpr int shortTermBlockCount = 30;
    static constexpr int maxIntegratedBlocks = 7200;
    static constexpr int maxLraBlocks = maxIntegratedBlocks;
    static constexpr int lraUpdateIntervalBlocks = 10;
    static constexpr double silenceMeanSquare = 1.0e-12;

    static double lufsFromMeanSquare (double meanSquare) noexcept;
    static float decibelsFromGain (float gain) noexcept;

    static float estimateCubicInterpolatedPeak (float previousPreviousSample,
                                                float previousSample,
                                                float currentSample,
                                                float nextSample) noexcept;

    static double percentileFromSortedValues (
        const std::array<double, maxLraBlocks>& sortedValues,
        int count,
        double percentile0To1) noexcept;

    void updateKWeightingCoefficients() noexcept;
    void finishMeasurementBlock() noexcept;
    double getRecentMeanSquare (int numBlocks) const noexcept;
    void pushIntegratedBlock (double meanSquare) noexcept;
    void updateIntegratedLoudness() noexcept;
    void pushLoudnessRangeBlock (double shortTermMeanSquare) noexcept;
    void updateLoudnessRange() noexcept;
    void storeSnapshotValue (std::atomic<float>& target, double value) noexcept;
    float processTruePeakSample (float input, int channel) noexcept;

    double currentSampleRate = 44100.0;
    int samplesPerMeasurementBlock = 4410;
    int samplesInCurrentBlock = 0;
    int rawRmsSamplesInCurrentBlock = 0;
    double currentKWeightedEnergySum = 0.0;
    double currentRawRmsSum = 0.0;
    float currentBlockSamplePeak = 0.0f;
    float currentBlockTruePeak = 0.0f;
    float peakHoldLinear = 0.0f;
    float truePeakHoldLinear = 0.0f;

    std::array<Biquad, maxChannels> highShelfFilters {};
    std::array<Biquad, maxChannels> highPassFilters {};

    std::array<double, shortTermBlockCount> recentMeanSquares {};
    std::array<double, maxIntegratedBlocks> integratedMeanSquares {};
    std::array<double, maxLraBlocks> lraShortTermMeanSquares {};
    std::array<double, maxLraBlocks> lraLoudnessScratch {};

    std::array<std::array<float, 4>, maxChannels> truePeakHistory {};
    std::array<int, maxChannels> truePeakHistoryCount {};

    int recentWriteIndex = 0;
    int recentBlockCount = 0;
    int integratedWriteIndex = 0;
    int integratedBlockCount = 0;
    int lraWriteIndex = 0;
    int lraBlockCount = 0;
    int lraBlocksSinceLastUpdate = 0;

    std::atomic<float> momentaryLufs { -100.0f };
    std::atomic<float> shortTermLufs { -100.0f };
    std::atomic<float> integratedLufs { -100.0f };
    std::atomic<float> loudnessRangeLu { 0.0f };
    std::atomic<float> samplePeakDb { -100.0f };
    std::atomic<float> truePeakDb { -100.0f };
    std::atomic<float> rmsDb { -100.0f };
    std::atomic<float> crestDb { 0.0f };
    std::atomic<float> peakHoldDb { -100.0f };
    std::atomic<bool> hasIntegratedMeasurement { false };
    std::atomic<bool> hasLoudnessRange { false };
    std::atomic<bool> hasTruePeak { false };
};