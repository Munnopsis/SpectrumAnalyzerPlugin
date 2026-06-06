#pragma once

#include "../analyzer/AnalyzerFrequencyRange.h"

#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>

class SpectrumDisplay : public juce::Component
{
public:
    SpectrumDisplay();
    ~SpectrumDisplay() override = default;

    struct DisplayNotePeak
    {
        float frequencyHz = 0.0f;
        float decibels = -100.0f;
        int midiNote = -1;
        int pitchClass = -1;
    };

    void setInputLevelDb (float newLevelDb);
    void setSpectrumDb (const std::vector<float>& newSpectrumDb);
    void setPeakHoldSpectrumDb (const std::vector<float>& newPeakHoldDb);
    void setRmsSpectrumDb (const std::vector<float>& newRmsDb);
    void setNotePeaks (const std::vector<DisplayNotePeak>& newNotePeaks);
    void setMinimumDecibels (float newMinimumDecibels);
    void setSlopeDbPerOctave (float newSlopeDbPerOctave);
    void setVisibleFrequencyRange (float minimumHz, float maximumHz);

    void setCurveVisibility (bool shouldShowLive,
                         bool shouldShowRms,
                         bool shouldShowPeakHold);

    void paint (juce::Graphics& g) override;
    void resized() override;

    void mouseDown (const juce::MouseEvent& event) override;
    void mouseMove (const juce::MouseEvent& event) override;
    void mouseDrag (const juce::MouseEvent& event) override;
    void mouseUp (const juce::MouseEvent& event) override;
    void mouseExit (const juce::MouseEvent& event) override;
    void mouseWheelMove (const juce::MouseEvent& event,
                         const juce::MouseWheelDetails& wheel) override;
    void mouseDoubleClick (const juce::MouseEvent& event) override;

private:
    struct PeakNoteLabel
    {
        float frequencyHz = 0.0f;
        float decibels = -100.0f;
        float x = 0.0f;
        float y = 0.0f;
        int midiNote = -1;
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
    void drawCurveFromData (juce::Graphics& g,
                            juce::Rectangle<int> bounds,
                            const std::vector<float>& values,
                            juce::Colour colour,
                            float strokeWidth);
    void drawLegend (juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawVisibleFrequencyRangeIndicator (juce::Graphics& g,
                                             juce::Rectangle<int> bounds);
    void drawMouseReadout (juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawPeakNoteLabels (juce::Graphics& g, juce::Rectangle<int> bounds);

    juce::Rectangle<float> getSpectrumArea (juce::Rectangle<int> bounds) const;
    std::vector<PeakNoteLabel> buildPeakNoteLabels (juce::Rectangle<float> area) const;
    int frequencyToMidiNote (float frequencyHz) const;
    int midiNoteToPitchClass (int midiNote) const;
    float frequencyToX (float frequencyHz, juce::Rectangle<float> area) const;
    float xToFrequency (float x, juce::Rectangle<float> area) const;
    float decibelsToY (float decibels, juce::Rectangle<float> area) const;
    float yToDecibels (float y, juce::Rectangle<float> area) const;
    juce::String formatFrequency (float frequencyHz) const;
    void drawFrequencyGridLine (juce::Graphics& g,
                                juce::Rectangle<float> drawArea,
                                float frequencyHz,
                                float alpha,
                                bool shouldDrawLabel);
    juce::String formatFrequencyGridLabel (float frequencyHz) const;
    bool isVisibleFrequencyRangeDefault() const noexcept;
    juce::String formatFrequencyRangeValue (float frequencyHz) const;
    juce::String frequencyToNoteName (float frequencyHz) const;
    bool getInterpolatedCurveValueDb (const std::vector<float>& values,
                                      float frequencyHz,
                                      float& resultDb) const;
    juce::String formatCurveValue (const juce::String& label, float valueDb) const;
    juce::String buildCurveReadoutText (float frequencyHz) const;
    float applySlopeCorrection (float decibels, float frequencyHz) const;
    void updateMouseReadout (juce::Point<float> newPosition);
    void zoomVisibleFrequencyRangeAround (float centreFrequencyHz,
                                          float zoomFactor);
    void panVisibleFrequencyRangeByPixels (float deltaPixels,
                                           juce::Rectangle<float> area);
    void resetVisibleFrequencyRangeToDefault();

    static constexpr float defaultMinFrequencyHz = AnalyzerFrequencyRange::minimumHz;
    static constexpr float defaultMaxFrequencyHz = AnalyzerFrequencyRange::maximumHz;
    static constexpr float mouseWheelZoomBase = 1.18f;
    static constexpr float minimumVisibleFrequencyRatio = 2.0f;
    static constexpr float maxDecibels = 0.0f;
    static constexpr float slopeReferenceFrequencyHz = 1000.0f;

    float minDecibels = -100.0f;
    float slopeDbPerOctave = 0.0f;
    float inputLevelDb = -100.0f;
    float dataMinFrequencyHz = defaultMinFrequencyHz;
    float dataMaxFrequencyHz = defaultMaxFrequencyHz;
    float visibleMinFrequencyHz = defaultMinFrequencyHz;
    float visibleMaxFrequencyHz = defaultMaxFrequencyHz;
    std::vector<float> spectrumDb;
    std::vector<float> peakHoldDb;
    std::vector<float> rmsDb;
    std::vector<DisplayNotePeak> notePeaks;

    bool showLiveCurve = true;
    bool showRmsCurve = true;
    bool showPeakHoldCurve = true;
    bool hasMouseReadout = false;
    bool isPanningVisibleFrequencyRange = false;
    float lastPanMouseX = 0.0f;
    juce::Point<float> mousePosition;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectrumDisplay)
};
