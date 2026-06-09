#pragma once

#include "../analyzer/AnalyzerCurveSource.h"
#include "../analyzer/AnalyzerDisplayResolution.h"
#include "../analyzer/AnalyzerFrequencyRange.h"
#include "../analyzer/AnalyzerReferenceManager.h"

#include <array>
#include <functional>
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

    struct StereoMeterDisplayData
    {
        float correlation = 0.0f;
        float smoothedCorrelation = 0.0f;
        float leftLevelDb = -100.0f;
        float rightLevelDb = -100.0f;
        float midLevelDb = -100.0f;
        float sideLevelDb = -100.0f;
        float balanceDb = 0.0f;
        float widthPercent = 0.0f;
        float monoCompatibilityDb = 0.0f;
        std::vector<juce::Point<float>> goniometerPoints;
    };

    struct LoudnessMeterDisplayData
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

    struct FrequencyCorrelationDisplayData
    {
        static constexpr int numBands = 31;

        struct Band
        {
            float centreFrequencyHz = 0.0f;
            float correlation = 0.0f;
            float smoothedCorrelation = 0.0f;
            bool valid = false;
        };

        std::array<Band, numBands> bands {};
    };

    void setInputLevelDb (float newLevelDb);
    void setSpectrumDb (const std::vector<float>& newSpectrumDb);
    void setPeakHoldSpectrumDb (const std::vector<float>& newPeakHoldDb);
    void setRmsSpectrumDb (const std::vector<float>& newRmsDb);
    void setNotePeaks (const std::vector<DisplayNotePeak>& newNotePeaks);
    void setAnalyzerFrameData (float dataMinimumFrequencyHz,
                               float dataMaximumFrequencyHz,
                               const std::vector<float>& liveDb,
                               const std::vector<float>& peakHoldDb,
                               const std::vector<float>& rmsDb,
                               const std::vector<float>& energyDb,
                               const std::vector<DisplayNotePeak>& newNotePeaks);
    void setSecondaryAnalyzerFrameData (
        bool shouldShowSecondary,
        const juce::String& primaryLabel,
        const juce::String& secondaryLabel,
        float secondaryDataMinimumFrequencyHz,
        float secondaryDataMaximumFrequencyHz,
        const std::vector<float>& secondaryLiveDb,
        const std::vector<float>& secondaryPeakHoldDb,
        const std::vector<float>& secondaryRmsDb,
        const std::vector<float>& secondaryEnergyDb);
    void setMinimumDecibels (float newMinimumDecibels);
    void setSlopeDbPerOctave (float newSlopeDbPerOctave);
    void setDisplayResolution (AnalyzerDisplayResolution newResolution);
    void setVisibleFrequencyRange (float minimumHz, float maximumHz);
    void setSpectrumDataFrequencyRange (float minimumHz, float maximumHz);
    void freezeCurrentSpectrumAsReference();
    void clearFrozenReferenceSpectrum();
    bool hasFrozenReferenceSpectrum() const noexcept;
    void addCurrentSpectrumAsReference();
    void setReferenceCurves (const std::vector<AnalyzerReferenceCurve>& references,
                             int activeIndex);
    void clearAllReferenceCurves();
    void removeActiveReferenceCurve();
    void setActiveReferenceIndex (int index);
    int getNumReferenceCurves() const noexcept;
    int getActiveReferenceIndex() const noexcept;
    juce::String getReferenceCurveName (int index) const;
    void setPeakDipMarkersVisible (bool shouldBeVisible);
    void setPeakDipCurveSource (AnalyzerCurveSource source);
    void setDifferenceCurveVisible (bool shouldBeVisible);
    void setDifferenceCurveSource (AnalyzerCurveSource source);
    void setStereoMeterData (const StereoMeterDisplayData& data);
    void setStereoMeterVisible (bool shouldBeVisible);
    void setLoudnessMeterData (const LoudnessMeterDisplayData& data);
    void setLoudnessMeterVisible (bool shouldBeVisible);
    void setFrequencyCorrelationData (
        const FrequencyCorrelationDisplayData& data);
    void setFrequencyCorrelationVisible (bool shouldBeVisible);
    void setValidationSignalLabels (const juce::String& activeSignalLabel,
                                    const juce::String& expectedBehaviourLabel);

    std::function<void (float minimumHz, float maximumHz)> onVisibleFrequencyRangeChanged;

    void setCurveVisibility (bool shouldShowLive,
                             bool shouldShowRms,
                             bool shouldShowEnergy,
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

    enum class SpectrumExtremumKind
    {
        peak,
        dip
    };

    struct SpectrumExtremumMarker
    {
        SpectrumExtremumKind kind = SpectrumExtremumKind::peak;
        float frequencyHz = 0.0f;
        float decibels = -100.0f;
        float prominenceDb = 0.0f;
        float x = 0.0f;
        float y = 0.0f;
    };

    struct ReferenceCurveSnapshot
    {
        juce::String name;
        float dataMinFrequencyHz = AnalyzerFrequencyRange::minimumHz;
        float dataMaxFrequencyHz = AnalyzerFrequencyRange::maximumHz;
        std::vector<float> liveDb;
        std::vector<float> rmsDb;
        std::vector<float> energyDb;
        std::vector<float> peakHoldDb;
        bool visible = true;
        juce::Colour colour = juce::Colours::white;
    };

    enum class CurveRenderMode
    {
        dataPoints,
        pixelResampled
    };

    void drawBackground (juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawFrequencyGrid (juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawDecibelGrid (juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawPlaceholderCurve (juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawInputLevelMeter (juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawSpectrumCurve (juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawFrozenReferenceCurve (juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawEnergyCurve (juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawPeakHoldCurve (juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawRmsCurve (juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawSecondaryAnalyzerCurves (juce::Graphics& g,
                                      juce::Rectangle<int> bounds);
    void drawPeakDipMarkers (juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawDifferenceCurve (juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawCurveFromData (juce::Graphics& g,
                            juce::Rectangle<int> bounds,
                            const std::vector<float>& values,
                            juce::Colour colour,
                            float strokeWidth,
                            CurveRenderMode renderMode = CurveRenderMode::dataPoints);
    void drawCurveFromDataRange (juce::Graphics& g,
                                 juce::Rectangle<int> bounds,
                                 const std::vector<float>& values,
                                 float sourceMinFrequencyHz,
                                 float sourceMaxFrequencyHz,
                                 juce::Colour colour,
                                 float strokeWidth,
                                 CurveRenderMode renderMode = CurveRenderMode::dataPoints);
    void drawLegend (juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawVisibleFrequencyRangeIndicator (juce::Graphics& g,
                                             juce::Rectangle<int> bounds);
    void drawMouseReadout (juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawPeakNoteLabels (juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawValidationSignalBanner (juce::Graphics& g,
                                     juce::Rectangle<int> bounds);
    bool hasAnyVisibleSpectrumCurve() const noexcept;
    void drawNoVisibleCurvesHint (juce::Graphics& g,
                                  juce::Rectangle<int> bounds);
    void drawMissingDifferenceReferenceHint (juce::Graphics& g,
                                             juce::Rectangle<int> bounds);
    void drawStereoMeterPanel (juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawLoudnessMeterPanel (juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawFrequencyCorrelationPanel (juce::Graphics& g,
                                        juce::Rectangle<int> bounds);
    void drawCorrelationMeter (juce::Graphics& g, juce::Rectangle<float> area);
    void drawGoniometer (juce::Graphics& g, juce::Rectangle<float> area);
    void drawStereoBalanceAndWidthText (juce::Graphics& g,
                                        juce::Rectangle<float> area);

    juce::Rectangle<float> getSpectrumArea (juce::Rectangle<int> bounds) const;
    std::vector<PeakNoteLabel> buildPeakNoteLabels (juce::Rectangle<float> area) const;
    std::vector<SpectrumExtremumMarker> buildPeakDipMarkers (
        juce::Rectangle<float> area) const;
    const std::vector<float>* getCurveDataForSource (
        AnalyzerCurveSource source) const noexcept;
    const std::vector<float>* getReferenceCurveDataForSource (
        const ReferenceCurveSnapshot& reference,
        AnalyzerCurveSource source) const noexcept;
    juce::String getCurveSourceLabel (AnalyzerCurveSource source) const;
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
    bool getInterpolatedCurveValueDbForDataRange (const std::vector<float>& values,
                                                  float frequencyHz,
                                                  float sourceMinFrequencyHz,
                                                  float sourceMaxFrequencyHz,
                                                  float& resultDb) const;
    float getDisplayResolutionOctaveWidth() const noexcept;
    bool getRawInterpolatedCurveValueDbForDataRange (const std::vector<float>& values,
                                                     float frequencyHz,
                                                     float sourceMinFrequencyHz,
                                                     float sourceMaxFrequencyHz,
                                                     float& resultDb) const;
    bool getDisplayResolutionCurveValueDbForDataRange (
        const std::vector<float>& values,
        float frequencyHz,
        float sourceMinFrequencyHz,
        float sourceMaxFrequencyHz,
        float& resultDb) const;
    juce::String formatCurveValue (const juce::String& label, float valueDb) const;
    juce::String buildCurveReadoutText (float frequencyHz) const;
    float applySlopeCorrection (float decibels, float frequencyHz) const;
    bool getDifferenceCurveValueDb (float frequencyHz, float& differenceDb) const;
    float differenceDecibelsToY (float differenceDb,
                                 juce::Rectangle<float> area) const;
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
    static constexpr int peakDipSamplingPoints = 512;
    static constexpr int maxPeakDipMarkersPerKind = 6;
    static constexpr int maxReferenceCurves = 8;
    static constexpr float minimumPeakDipProminenceDb = 2.5f;
    static constexpr float minimumPeakDipSpacingOctaves = 1.0f / 12.0f;
    static constexpr float peakDipNeighbourWindowOctaves = 1.0f / 6.0f;
    static constexpr float differenceViewRangeDb = 24.0f;

    float minDecibels = -100.0f;
    float slopeDbPerOctave = 0.0f;
    AnalyzerDisplayResolution displayResolution =
        AnalyzerDisplayResolution::highResolution;
    float inputLevelDb = -100.0f;
    float dataMinFrequencyHz = defaultMinFrequencyHz;
    float dataMaxFrequencyHz = defaultMaxFrequencyHz;
    float visibleMinFrequencyHz = defaultMinFrequencyHz;
    float visibleMaxFrequencyHz = defaultMaxFrequencyHz;
    std::vector<float> spectrumDb;
    std::vector<float> peakHoldDb;
    std::vector<float> rmsDb;
    std::vector<float> energyDb;
    bool showSecondaryAnalyzerCurves = false;
    juce::String primaryCurveLabel = "Main";
    juce::String secondaryCurveLabel = "Secondary";
    float secondaryDataMinFrequencyHz = defaultMinFrequencyHz;
    float secondaryDataMaxFrequencyHz = defaultMaxFrequencyHz;
    std::vector<float> secondaryLiveDb;
    std::vector<float> secondaryPeakHoldDb;
    std::vector<float> secondaryRmsDb;
    std::vector<float> secondaryEnergyDb;
    std::vector<ReferenceCurveSnapshot> referenceCurves;
    int activeReferenceIndex = -1;
    std::vector<DisplayNotePeak> notePeaks;

    bool showLiveCurve = true;
    bool showRmsCurve = true;
    bool showEnergyCurve = true;
    bool showPeakHoldCurve = true;
    bool showPeakDipMarkers = false;
    bool showPeakMarkers = true;
    bool showDipMarkers = true;
    bool showDifferenceCurve = false;
    bool showStereoMeter = true;
    bool showLoudnessMeter = true;
    bool showFrequencyCorrelation = true;
    bool hasMouseReadout = false;
    bool isPanningVisibleFrequencyRange = false;
    AnalyzerCurveSource peakDipCurveSource = AnalyzerCurveSource::live;
    AnalyzerCurveSource differenceCurveSource = AnalyzerCurveSource::live;
    StereoMeterDisplayData stereoMeterData;
    LoudnessMeterDisplayData loudnessMeterData;
    FrequencyCorrelationDisplayData frequencyCorrelationData;
    juce::String validationSignalLabel;
    juce::String validationExpectedLabel;
    float lastPanMouseX = 0.0f;
    juce::Point<float> mousePosition;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectrumDisplay)
};
