#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>

class SpectrumDisplay : public juce::Component
{
public:
    SpectrumDisplay();
    ~SpectrumDisplay() override = default;

    void setInputLevelDb (float newLevelDb);
    void setSpectrumDb (const std::vector<float>& newSpectrumDb);
    void setPeakHoldSpectrumDb (const std::vector<float>& newPeakHoldDb);
    void setRmsSpectrumDb (const std::vector<float>& newRmsDb);
    void setMinimumDecibels (float newMinimumDecibels);
    void setSlopeDbPerOctave (float newSlopeDbPerOctave);

    void setCurveVisibility (bool shouldShowLive,
                         bool shouldShowRms,
                         bool shouldShowPeakHold);

    void paint (juce::Graphics& g) override;
    void resized() override;

    void mouseMove (const juce::MouseEvent& event) override;
    void mouseDrag (const juce::MouseEvent& event) override;
    void mouseExit (const juce::MouseEvent& event) override;

private:
    struct PeakNoteLabel
    {
        float frequencyHz = 0.0f;
        float decibels = -100.0f;
        float x = 0.0f;
        float y = 0.0f;
        juce::String noteName;
    };

    void drawBackground (juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawFrequencyGrid (juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawDecibelGrid (juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawPlaceholderCurve (juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawInputLevelMeter (juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawSpectrumCurve (juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawPeakHoldCurve (juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawRmsCurve (juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawLegend (juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawMouseReadout (juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawPeakNoteLabels (juce::Graphics& g, juce::Rectangle<int> bounds);

    juce::Rectangle<float> getSpectrumArea (juce::Rectangle<int> bounds) const;
    std::vector<PeakNoteLabel> buildPeakNoteLabels (juce::Rectangle<float> area) const;
    bool isPeakCandidate (size_t index) const;
    float indexToFrequency (size_t index, size_t numPoints) const;
    float frequencyToNormalisedX (float frequencyHz) const;
    float frequencyToX (float frequencyHz, juce::Rectangle<float> area) const;
    float xToFrequency (float x, juce::Rectangle<float> area) const;
    float decibelsToY (float decibels, juce::Rectangle<float> area) const;
    float yToDecibels (float y, juce::Rectangle<float> area) const;
    juce::String formatFrequency (float frequencyHz) const;
    juce::String frequencyToNoteName (float frequencyHz) const;
    bool getInterpolatedCurveValueDb (const std::vector<float>& values,
                                      float frequencyHz,
                                      float& resultDb) const;
    juce::String formatCurveValue (const juce::String& label, float valueDb) const;
    juce::String buildCurveReadoutText (float frequencyHz) const;
    float applySlopeCorrection (float decibels, float frequencyHz) const;
    void updateMouseReadout (juce::Point<float> newPosition);

    static constexpr float minFrequencyHz = 20.0f;
    static constexpr float maxFrequencyHz = 20000.0f;
    static constexpr float maxDecibels = 0.0f;
    static constexpr float slopeReferenceFrequencyHz = 1000.0f;

    float minDecibels = -100.0f;
    float slopeDbPerOctave = 0.0f;
    float inputLevelDb = -100.0f;
    std::vector<float> spectrumDb;
    std::vector<float> peakHoldDb;
    std::vector<float> rmsDb;

    bool showLiveCurve = true;
    bool showRmsCurve = true;
    bool showPeakHoldCurve = true;
    bool hasMouseReadout = false;
    juce::Point<float> mousePosition;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectrumDisplay)
};
