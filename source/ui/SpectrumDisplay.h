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

    void setCurveVisibility (bool shouldShowLive,
                         bool shouldShowRms,
                         bool shouldShowPeakHold);

    void paint (juce::Graphics& g) override;
    void resized() override;


private:
    void drawBackground (juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawFrequencyGrid (juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawDecibelGrid (juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawPlaceholderCurve (juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawInputLevelMeter (juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawSpectrumCurve (juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawPeakHoldCurve (juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawRmsCurve (juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawLegend (juce::Graphics& g, juce::Rectangle<int> bounds);

    float frequencyToX (float frequencyHz, juce::Rectangle<float> area) const;
    float decibelsToY (float decibels, juce::Rectangle<float> area) const;

    static constexpr float minFrequencyHz = 20.0f;
    static constexpr float maxFrequencyHz = 20000.0f;
    static constexpr float maxDecibels = 0.0f;

    float minDecibels = -100.0f;
    float inputLevelDb = -100.0f;
    std::vector<float> spectrumDb;
    std::vector<float> peakHoldDb;
    std::vector<float> rmsDb;

    bool showLiveCurve = true;
    bool showRmsCurve = true;
    bool showPeakHoldCurve = true;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectrumDisplay)
};
