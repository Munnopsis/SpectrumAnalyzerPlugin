#include "SpectrumDisplay.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

namespace
{
    constexpr float displayMinusInfinityDb = -100.0f;

    float displayDecibelsToGain (float decibels) noexcept
    {
        if (!std::isfinite (decibels) || decibels <= displayMinusInfinityDb)
            return 0.0f;

        return std::pow (10.0f, decibels * 0.05f);
    }

    float displayGainToDecibels (float gain) noexcept
    {
        if (!std::isfinite (gain) || gain <= 0.0f)
            return displayMinusInfinityDb;

        return juce::jmax (
            displayMinusInfinityDb,
            20.0f * std::log10 (gain));
    }
}

SpectrumDisplay::SpectrumDisplay()
{
    setOpaque (true);
}

void SpectrumDisplay::setInputLevelDb (float newLevelDb)
{
    inputLevelDb = newLevelDb;
    repaint();
}

void SpectrumDisplay::setSpectrumDb (const std::vector<float>& newSpectrumDb)
{
    spectrumDb = newSpectrumDb;
    repaint();
}

void SpectrumDisplay::setPeakHoldSpectrumDb (const std::vector<float>& newPeakHoldDb)
{
    peakHoldDb = newPeakHoldDb;
    repaint();
}

void SpectrumDisplay::setRmsSpectrumDb (const std::vector<float>& newRmsDb)
{
    rmsDb = newRmsDb;
    repaint();
}

void SpectrumDisplay::setNotePeaks (const std::vector<DisplayNotePeak>& newNotePeaks)
{
    notePeaks = newNotePeaks;
    repaint();
}

void SpectrumDisplay::setAnalyzerFrameData (
    float dataMinimumFrequencyHz,
    float dataMaximumFrequencyHz,
    const std::vector<float>& newLiveDb,
    const std::vector<float>& newPeakHoldDb,
    const std::vector<float>& newRmsDb,
    const std::vector<float>& newEnergyDb,
    const std::vector<DisplayNotePeak>& newNotePeaks)
{
    const auto clampedMinimum =
        juce::jlimit (AnalyzerFrequencyRange::minimumHz,
                      AnalyzerFrequencyRange::maximumHz - 1.0f,
                      dataMinimumFrequencyHz);

    const auto clampedMaximum =
        juce::jlimit (clampedMinimum + 1.0f,
                      AnalyzerFrequencyRange::maximumHz,
                      dataMaximumFrequencyHz);

    dataMinFrequencyHz = clampedMinimum;
    dataMaxFrequencyHz = clampedMaximum;

    spectrumDb = newLiveDb;
    peakHoldDb = newPeakHoldDb;
    rmsDb = newRmsDb;
    energyDb = newEnergyDb;
    notePeaks = newNotePeaks;

    repaint();
}

void SpectrumDisplay::setMinimumDecibels (float newMinimumDecibels)
{
    const auto clampedMinimum = juce::jlimit (-120.0f, -20.0f, newMinimumDecibels);

    if (std::abs (minDecibels - clampedMinimum) < 0.001f)
        return;

    minDecibels = clampedMinimum;
    repaint();
}

void SpectrumDisplay::setSlopeDbPerOctave (float newSlopeDbPerOctave)
{
    const auto clampedSlope = juce::jlimit (-12.0f, 12.0f, newSlopeDbPerOctave);

    if (std::abs (slopeDbPerOctave - clampedSlope) < 0.001f)
        return;

    slopeDbPerOctave = clampedSlope;
    repaint();
}

void SpectrumDisplay::setDisplayResolution (
    AnalyzerDisplayResolution newResolution)
{
    if (displayResolution == newResolution)
        return;

    displayResolution = newResolution;
    repaint();
}

void SpectrumDisplay::setVisibleFrequencyRange (float minimumHz, float maximumHz)
{
    const auto clampedMinimum =
        juce::jlimit (AnalyzerFrequencyRange::minimumHz,
                      AnalyzerFrequencyRange::maximumHz - 1.0f,
                      minimumHz);

    const auto clampedMaximum =
        juce::jlimit (clampedMinimum + 1.0f,
                      AnalyzerFrequencyRange::maximumHz,
                      maximumHz);

    if (std::abs (visibleMinFrequencyHz - clampedMinimum) < 0.001f
        && std::abs (visibleMaxFrequencyHz - clampedMaximum) < 0.001f)
    {
        return;
    }

    visibleMinFrequencyHz = clampedMinimum;
    visibleMaxFrequencyHz = clampedMaximum;

    repaint();

    if (onVisibleFrequencyRangeChanged)
        onVisibleFrequencyRangeChanged (visibleMinFrequencyHz, visibleMaxFrequencyHz);
}

void SpectrumDisplay::setSpectrumDataFrequencyRange (float minimumHz, float maximumHz)
{
    const auto clampedMinimum =
        juce::jlimit (AnalyzerFrequencyRange::minimumHz,
                      AnalyzerFrequencyRange::maximumHz - 1.0f,
                      minimumHz);

    const auto clampedMaximum =
        juce::jlimit (clampedMinimum + 1.0f,
                      AnalyzerFrequencyRange::maximumHz,
                      maximumHz);

    if (std::abs (dataMinFrequencyHz - clampedMinimum) < 0.001f
        && std::abs (dataMaxFrequencyHz - clampedMaximum) < 0.001f)
    {
        return;
    }

    dataMinFrequencyHz = clampedMinimum;
    dataMaxFrequencyHz = clampedMaximum;

    repaint();
}

void SpectrumDisplay::freezeCurrentSpectrumAsReference()
{
    addCurrentSpectrumAsReference();
}

void SpectrumDisplay::clearFrozenReferenceSpectrum()
{
    clearAllReferenceCurves();
}

bool SpectrumDisplay::hasFrozenReferenceSpectrum() const noexcept
{
    return !referenceCurves.empty();
}

void SpectrumDisplay::addCurrentSpectrumAsReference()
{
    if (spectrumDb.size() < 2)
        return;

    if (referenceCurves.size() >= static_cast<size_t> (maxReferenceCurves))
    {
        referenceCurves.erase (referenceCurves.begin());

        if (activeReferenceIndex > 0)
            --activeReferenceIndex;
    }

    ReferenceCurveSnapshot snapshot;
    snapshot.name = "Ref " + juce::String (referenceCurves.size() + 1);
    snapshot.dataMinFrequencyHz = dataMinFrequencyHz;
    snapshot.dataMaxFrequencyHz = dataMaxFrequencyHz;
    snapshot.liveDb = spectrumDb;
    snapshot.rmsDb = rmsDb;
    snapshot.energyDb = energyDb;
    snapshot.peakHoldDb = peakHoldDb;
    snapshot.visible = true;

    referenceCurves.push_back (std::move (snapshot));
    activeReferenceIndex = static_cast<int> (referenceCurves.size()) - 1;

    repaint();
}

void SpectrumDisplay::clearAllReferenceCurves()
{
    if (referenceCurves.empty() && activeReferenceIndex < 0)
        return;

    referenceCurves.clear();
    activeReferenceIndex = -1;
    repaint();
}

void SpectrumDisplay::removeActiveReferenceCurve()
{
    if (activeReferenceIndex < 0
        || activeReferenceIndex >= static_cast<int> (referenceCurves.size()))
    {
        return;
    }

    referenceCurves.erase (referenceCurves.begin() + activeReferenceIndex);

    if (referenceCurves.empty())
        activeReferenceIndex = -1;
    else
        activeReferenceIndex = juce::jlimit (
            0,
            static_cast<int> (referenceCurves.size()) - 1,
            activeReferenceIndex);

    repaint();
}

void SpectrumDisplay::setActiveReferenceIndex (int index)
{
    const auto newIndex =
        referenceCurves.empty()
            ? -1
            : juce::jlimit (0, static_cast<int> (referenceCurves.size()) - 1, index);

    if (activeReferenceIndex == newIndex)
        return;

    activeReferenceIndex = newIndex;
    repaint();
}

int SpectrumDisplay::getNumReferenceCurves() const noexcept
{
    return static_cast<int> (referenceCurves.size());
}

int SpectrumDisplay::getActiveReferenceIndex() const noexcept
{
    return activeReferenceIndex;
}

juce::String SpectrumDisplay::getReferenceCurveName (int index) const
{
    if (index < 0 || index >= static_cast<int> (referenceCurves.size()))
        return {};

    return referenceCurves[static_cast<size_t> (index)].name;
}

void SpectrumDisplay::setPeakDipMarkersVisible (bool shouldBeVisible)
{
    if (showPeakDipMarkers == shouldBeVisible)
        return;

    showPeakDipMarkers = shouldBeVisible;
    repaint();
}

void SpectrumDisplay::setPeakDipCurveSource (AnalyzerCurveSource source)
{
    if (peakDipCurveSource == source)
        return;

    peakDipCurveSource = source;
    repaint();
}

void SpectrumDisplay::setDifferenceCurveVisible (bool shouldBeVisible)
{
    if (showDifferenceCurve == shouldBeVisible)
        return;

    showDifferenceCurve = shouldBeVisible;
    repaint();
}

void SpectrumDisplay::setDifferenceCurveSource (AnalyzerCurveSource source)
{
    if (differenceCurveSource == source)
        return;

    differenceCurveSource = source;
    repaint();
}

void SpectrumDisplay::setCurveVisibility (bool shouldShowLive,
                                          bool shouldShowRms,
                                          bool shouldShowEnergy,
                                          bool shouldShowPeakHold)
{
    showLiveCurve = shouldShowLive;
    showRmsCurve = shouldShowRms;
    showEnergyCurve = shouldShowEnergy;
    showPeakHoldCurve = shouldShowPeakHold;

    repaint();
}

void SpectrumDisplay::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    drawBackground (g, bounds);
    drawFrequencyGrid (g, bounds);
    drawDecibelGrid (g, bounds);

    if (spectrumDb.empty() && energyDb.empty())
    {
        drawPlaceholderCurve (g, bounds);
    }
    else
    {
        drawFrozenReferenceCurve (g, bounds);

        if (showEnergyCurve && ! energyDb.empty())
            drawEnergyCurve (g, bounds);

        if (showRmsCurve && ! rmsDb.empty())
            drawRmsCurve (g, bounds);

        if (showLiveCurve && ! spectrumDb.empty())
            drawSpectrumCurve (g, bounds);

        if (showPeakHoldCurve && ! peakHoldDb.empty())
            drawPeakHoldCurve (g, bounds);

        if (showDifferenceCurve)
            drawDifferenceCurve (g, bounds);
    }

    if (showPeakDipMarkers)
        drawPeakDipMarkers (g, bounds);

    drawPeakNoteLabels (g, bounds);

    drawLegend (g, bounds);
    drawVisibleFrequencyRangeIndicator (g, bounds);

    drawInputLevelMeter (g, bounds);

    g.setColour (juce::Colours::white.withAlpha (0.85f));
    g.setFont (juce::FontOptions (18.0f, juce::Font::bold));
    g.drawText ("FullSpectrum", bounds.reduced (16), juce::Justification::topLeft);

    drawMouseReadout (g, bounds);
}

void SpectrumDisplay::resized()
{
}

void SpectrumDisplay::mouseDown (const juce::MouseEvent& event)
{
    const auto area = getSpectrumArea (getLocalBounds());

    if (! area.contains (event.position))
    {
        isPanningVisibleFrequencyRange = false;
        return;
    }

    isPanningVisibleFrequencyRange = true;
    lastPanMouseX = event.position.x;
    updateMouseReadout (event.position);
}

void SpectrumDisplay::mouseMove (const juce::MouseEvent& event)
{
    updateMouseReadout (event.position);
}

void SpectrumDisplay::mouseDrag (const juce::MouseEvent& event)
{
    const auto area = getSpectrumArea (getLocalBounds());

    if (isPanningVisibleFrequencyRange)
    {
        const auto deltaPixels = event.position.x - lastPanMouseX;
        lastPanMouseX = event.position.x;

        panVisibleFrequencyRangeByPixels (deltaPixels, area);
        updateMouseReadout (event.position);
        return;
    }

    updateMouseReadout (event.position);
}

void SpectrumDisplay::mouseUp (const juce::MouseEvent& event)
{
    juce::ignoreUnused (event);

    isPanningVisibleFrequencyRange = false;
}

void SpectrumDisplay::mouseExit (const juce::MouseEvent& event)
{
    juce::ignoreUnused (event);

    isPanningVisibleFrequencyRange = false;

    if (! hasMouseReadout)
        return;

    hasMouseReadout = false;
    repaint();
}

void SpectrumDisplay::mouseWheelMove (const juce::MouseEvent& event,
                                      const juce::MouseWheelDetails& wheel)
{
    const auto area = getSpectrumArea (getLocalBounds());

    if (! area.contains (event.position))
        return;

    if (std::abs (wheel.deltaY) < 0.000001f)
        return;

    const auto centreFrequencyHz = xToFrequency (event.position.x, area);
    const auto wheelSteps = juce::jlimit (-4.0f, 4.0f, wheel.deltaY * 8.0f);

    if (std::abs (wheelSteps) < 0.000001f)
        return;

    const auto zoomFactor = std::pow (mouseWheelZoomBase, std::abs (wheelSteps));

    if (wheelSteps > 0.0f)
        zoomVisibleFrequencyRangeAround (centreFrequencyHz, zoomFactor);
    else
        zoomVisibleFrequencyRangeAround (centreFrequencyHz, 1.0f / zoomFactor);

    updateMouseReadout (event.position);
}

void SpectrumDisplay::mouseDoubleClick (const juce::MouseEvent& event)
{
    const auto area = getSpectrumArea (getLocalBounds());

    if (! area.contains (event.position))
        return;

    resetVisibleFrequencyRangeToDefault();
    updateMouseReadout (event.position);
}

juce::Rectangle<float> SpectrumDisplay::getSpectrumArea (juce::Rectangle<int> bounds) const
{
    auto area = bounds.toFloat();

    constexpr auto horizontalInset = 40.0f;
    constexpr auto topInset = 96.0f;
    constexpr auto bottomInset = 84.0f;

    area.removeFromLeft (horizontalInset);
    area.removeFromRight (horizontalInset);
    area.removeFromTop (topInset);
    area.removeFromBottom (bottomInset);

    return area;
}

float SpectrumDisplay::frequencyToX (float frequencyHz, juce::Rectangle<float> area) const
{
    const auto clampedFrequency =
        juce::jlimit (visibleMinFrequencyHz, visibleMaxFrequencyHz, frequencyHz);

    const auto normalised =
        std::log (clampedFrequency / visibleMinFrequencyHz)
        / std::log (visibleMaxFrequencyHz / visibleMinFrequencyHz);

    return area.getX() + normalised * area.getWidth();
}

float SpectrumDisplay::xToFrequency (float x, juce::Rectangle<float> area) const
{
    const auto normalised = (x - area.getX()) / juce::jmax (1.0f, area.getWidth());
    const auto clamped = juce::jlimit (0.0f, 1.0f, normalised);

    return visibleMinFrequencyHz
           * std::pow (visibleMaxFrequencyHz / visibleMinFrequencyHz, clamped);
}

float SpectrumDisplay::decibelsToY (float decibels, juce::Rectangle<float> area) const
{
    const auto clampedDb = juce::jlimit (minDecibels, maxDecibels, decibels);
    return juce::jmap (clampedDb, minDecibels, maxDecibels, area.getBottom(), area.getY());
}

float SpectrumDisplay::yToDecibels (float y, juce::Rectangle<float> area) const
{
    const auto normalised = (area.getBottom() - y) / juce::jmax (1.0f, area.getHeight());
    const auto clamped = juce::jlimit (0.0f, 1.0f, normalised);

    return juce::jmap (clamped, 0.0f, 1.0f, minDecibels, maxDecibels);
}

juce::String SpectrumDisplay::formatFrequency (float frequencyHz) const
{
    if (frequencyHz < 1000.0f)
        return juce::String (juce::roundToInt (frequencyHz)) + " Hz";

    return juce::String (frequencyHz / 1000.0f, 2) + " kHz";
}

juce::String SpectrumDisplay::formatFrequencyGridLabel (float frequencyHz) const
{
    if (frequencyHz < 1000.0f)
        return juce::String (juce::roundToInt (frequencyHz));

    const auto kilohertz = frequencyHz / 1000.0f;
    const auto roundedKilohertz = std::round (kilohertz);

    if (std::abs (kilohertz - roundedKilohertz) < 0.001f)
        return juce::String (static_cast<int> (roundedKilohertz)) + "k";

    return juce::String (kilohertz, 1) + "k";
}

bool SpectrumDisplay::isVisibleFrequencyRangeDefault() const noexcept
{
    return std::abs (visibleMinFrequencyHz - defaultMinFrequencyHz) < 0.01f
           && std::abs (visibleMaxFrequencyHz - defaultMaxFrequencyHz) < 0.01f;
}

juce::String SpectrumDisplay::formatFrequencyRangeValue (float frequencyHz) const
{
    if (frequencyHz < 1000.0f)
        return juce::String (juce::roundToInt (frequencyHz)) + " Hz";

    const auto kilohertz = frequencyHz / 1000.0f;
    const auto roundedKilohertz = std::round (kilohertz);

    if (std::abs (kilohertz - roundedKilohertz) < 0.01f)
        return juce::String (static_cast<int> (roundedKilohertz)) + " kHz";

    return juce::String (kilohertz, 1) + " kHz";
}

juce::String SpectrumDisplay::frequencyToNoteName (float frequencyHz) const
{
    const auto midiNote = frequencyToMidiNote (frequencyHz);

    if (midiNote < 0)
        return "-";

    static constexpr std::array<const char*, 12> noteNames {
        "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
    };

    const auto octave = midiNote / 12 - 1;
    return juce::String (noteNames[static_cast<size_t> (midiNoteToPitchClass (midiNote))])
           + juce::String (octave);
}

bool SpectrumDisplay::getInterpolatedCurveValueDb (const std::vector<float>& values,
                                                   float frequencyHz,
                                                   float& resultDb) const
{
    return getInterpolatedCurveValueDbForDataRange (values,
                                                    frequencyHz,
                                                    dataMinFrequencyHz,
                                                    dataMaxFrequencyHz,
                                                    resultDb);
}

bool SpectrumDisplay::getInterpolatedCurveValueDbForDataRange (
    const std::vector<float>& values,
    float frequencyHz,
    float sourceMinFrequencyHz,
    float sourceMaxFrequencyHz,
    float& resultDb) const
{
    float displayResolutionDb = 0.0f;

    if (! getDisplayResolutionCurveValueDbForDataRange (values,
                                                        frequencyHz,
                                                        sourceMinFrequencyHz,
                                                        sourceMaxFrequencyHz,
                                                        displayResolutionDb))
    {
        return false;
    }

    resultDb = applySlopeCorrection (displayResolutionDb, frequencyHz);
    return true;
}

float SpectrumDisplay::getDisplayResolutionOctaveWidth() const noexcept
{
    switch (displayResolution)
    {
        case AnalyzerDisplayResolution::highResolution: return 0.0f;
        case AnalyzerDisplayResolution::detailed:       return 1.0f / 48.0f;
        case AnalyzerDisplayResolution::balanced:       return 1.0f / 24.0f;
        case AnalyzerDisplayResolution::smooth:         return 1.0f / 12.0f;
        case AnalyzerDisplayResolution::oneSixthOctave: return 1.0f / 6.0f;
        case AnalyzerDisplayResolution::oneThirdOctave: return 1.0f / 3.0f;
        case AnalyzerDisplayResolution::octave:         return 1.0f;
        case AnalyzerDisplayResolution::count:          break;
    }

    return 0.0f;
}

bool SpectrumDisplay::getRawInterpolatedCurveValueDbForDataRange (
    const std::vector<float>& values,
    float frequencyHz,
    float sourceMinFrequencyHz,
    float sourceMaxFrequencyHz,
    float& resultDb) const
{
    if (values.size() < 2 || frequencyHz <= 0.0f)
        return false;

    if (sourceMinFrequencyHz <= 0.0f || sourceMaxFrequencyHz <= sourceMinFrequencyHz)
        return false;

    const auto clampedFrequency =
        juce::jlimit (sourceMinFrequencyHz, sourceMaxFrequencyHz, frequencyHz);

    const auto normalisedX =
        std::log (clampedFrequency / sourceMinFrequencyHz)
        / std::log (sourceMaxFrequencyHz / sourceMinFrequencyHz);

    const auto clampedX = juce::jlimit (0.0f, 1.0f, normalisedX);
    const auto maxIndex = values.size() - 1;
    const auto position = clampedX * static_cast<float> (maxIndex);
    const auto lowerIndex = static_cast<size_t> (std::floor (position));
    const auto upperIndex = std::min (lowerIndex + 1, maxIndex);
    const auto alpha = position - static_cast<float> (lowerIndex);
    const auto interpolatedDb =
        values[lowerIndex] + alpha * (values[upperIndex] - values[lowerIndex]);

    resultDb = interpolatedDb;
    return true;
}

bool SpectrumDisplay::getDisplayResolutionCurveValueDbForDataRange (
    const std::vector<float>& values,
    float frequencyHz,
    float sourceMinFrequencyHz,
    float sourceMaxFrequencyHz,
    float& resultDb) const
{
    const auto octaveWidth = getDisplayResolutionOctaveWidth();

    if (octaveWidth <= 0.0f)
    {
        return getRawInterpolatedCurveValueDbForDataRange (values,
                                                           frequencyHz,
                                                           sourceMinFrequencyHz,
                                                           sourceMaxFrequencyHz,
                                                           resultDb);
    }

    if (values.size() < 2 || frequencyHz <= 0.0f)
        return false;

    if (sourceMinFrequencyHz <= 0.0f || sourceMaxFrequencyHz <= sourceMinFrequencyHz)
        return false;

    const auto clampedFrequency =
        juce::jlimit (sourceMinFrequencyHz, sourceMaxFrequencyHz, frequencyHz);

    const auto centreLog2 = std::log2 (clampedFrequency);
    const auto halfWidth = octaveWidth * 0.5f;
    const auto minLog2 = centreLog2 - halfWidth;
    const auto maxLog2 = centreLog2 + halfWidth;
    auto weightedPowerSum = 0.0f;
    auto weightSum = 0.0f;
    const auto maxIndex = values.size() - 1;

    for (size_t i = 0; i < values.size(); ++i)
    {
        const auto normalisedX =
            static_cast<float> (i) / static_cast<float> (maxIndex);

        const auto binFrequencyHz =
            sourceMinFrequencyHz
            * std::pow (sourceMaxFrequencyHz / sourceMinFrequencyHz, normalisedX);

        if (binFrequencyHz <= 0.0f)
            continue;

        const auto binLog2 = std::log2 (binFrequencyHz);

        if (binLog2 < minLog2 || binLog2 > maxLog2)
            continue;

        const auto distance = std::abs (binLog2 - centreLog2);
        const auto weight =
            juce::jlimit (0.0f, 1.0f, 1.0f - distance / halfWidth);

        if (weight <= 0.0f)
            continue;

        const auto binDb =
            std::isfinite (values[i]) ? values[i] : -100.0f;

        const auto amplitude = displayDecibelsToGain (binDb);

        weightedPowerSum += weight * amplitude * amplitude;
        weightSum += weight;
    }

    if (weightSum <= 0.0f)
    {
        return getRawInterpolatedCurveValueDbForDataRange (values,
                                                           frequencyHz,
                                                           sourceMinFrequencyHz,
                                                           sourceMaxFrequencyHz,
                                                           resultDb);
    }

    const auto meanPower = weightedPowerSum / weightSum;
    const auto meanAmplitude = std::sqrt (juce::jmax (0.0f, meanPower));
    resultDb = displayGainToDecibels (meanAmplitude);
    return true;
}

const std::vector<float>* SpectrumDisplay::getCurveDataForSource (
    AnalyzerCurveSource source) const noexcept
{
    switch (source)
    {
        case AnalyzerCurveSource::live:     return &spectrumDb;
        case AnalyzerCurveSource::rms:      return &rmsDb;
        case AnalyzerCurveSource::energy:   return &energyDb;
        case AnalyzerCurveSource::peakHold: return &peakHoldDb;
        case AnalyzerCurveSource::count:    break;
    }

    return &spectrumDb;
}

const std::vector<float>* SpectrumDisplay::getReferenceCurveDataForSource (
    const ReferenceCurveSnapshot& reference,
    AnalyzerCurveSource source) const noexcept
{
    switch (source)
    {
        case AnalyzerCurveSource::live:     return &reference.liveDb;
        case AnalyzerCurveSource::rms:      return &reference.rmsDb;
        case AnalyzerCurveSource::energy:   return &reference.energyDb;
        case AnalyzerCurveSource::peakHold: return &reference.peakHoldDb;
        case AnalyzerCurveSource::count:    break;
    }

    return &reference.liveDb;
}

juce::String SpectrumDisplay::getCurveSourceLabel (AnalyzerCurveSource source) const
{
    switch (source)
    {
        case AnalyzerCurveSource::live:     return "Live";
        case AnalyzerCurveSource::rms:      return "RMS";
        case AnalyzerCurveSource::energy:   return "Energy";
        case AnalyzerCurveSource::peakHold: return "Peak";
        case AnalyzerCurveSource::count:    break;
    }

    return "Live";
}

bool SpectrumDisplay::getDifferenceCurveValueDb (
    float frequencyHz,
    float& differenceDb) const
{
    if (!showDifferenceCurve
        || activeReferenceIndex < 0
        || activeReferenceIndex >= static_cast<int> (referenceCurves.size()))
    {
        return false;
    }

    const auto* currentCurve = getCurveDataForSource (differenceCurveSource);

    if (currentCurve == nullptr || currentCurve->size() < 2)
        return false;

    const auto& reference =
        referenceCurves[static_cast<size_t> (activeReferenceIndex)];

    const auto* referenceCurve =
        getReferenceCurveDataForSource (reference, differenceCurveSource);

    if (referenceCurve == nullptr || referenceCurve->size() < 2)
        return false;

    auto currentDb = 0.0f;
    auto referenceDb = 0.0f;

    if (! getInterpolatedCurveValueDbForDataRange (*currentCurve,
                                                   frequencyHz,
                                                   dataMinFrequencyHz,
                                                   dataMaxFrequencyHz,
                                                   currentDb))
    {
        return false;
    }

    if (! getInterpolatedCurveValueDbForDataRange (*referenceCurve,
                                                   frequencyHz,
                                                   reference.dataMinFrequencyHz,
                                                   reference.dataMaxFrequencyHz,
                                                   referenceDb))
    {
        return false;
    }

    differenceDb =
        juce::jlimit (-differenceViewRangeDb,
            differenceViewRangeDb,
            currentDb - referenceDb);

    return true;
}

float SpectrumDisplay::differenceDecibelsToY (
    float differenceDb,
    juce::Rectangle<float> area) const
{
    const auto clamped =
        juce::jlimit (-differenceViewRangeDb,
            differenceViewRangeDb,
            differenceDb);

    const auto normalised =
        (clamped + differenceViewRangeDb) / (2.0f * differenceViewRangeDb);

    return juce::jmap (normalised,
        0.0f,
        1.0f,
        area.getBottom(),
        area.getY());
}

juce::String SpectrumDisplay::formatCurveValue (const juce::String& label, float valueDb) const
{
    return label + " " + juce::String (valueDb, 1);
}

juce::String SpectrumDisplay::buildCurveReadoutText (float frequencyHz) const
{
    juce::StringArray values;
    float valueDb = 0.0f;

    if (showLiveCurve && getInterpolatedCurveValueDb (spectrumDb, frequencyHz, valueDb))
        values.add (formatCurveValue ("Live", valueDb));

    if (showRmsCurve && getInterpolatedCurveValueDb (rmsDb, frequencyHz, valueDb))
        values.add (formatCurveValue ("RMS", valueDb));

    if (showEnergyCurve && getInterpolatedCurveValueDb (energyDb, frequencyHz, valueDb))
        values.add (formatCurveValue ("Energy", valueDb));

    if (showPeakHoldCurve && getInterpolatedCurveValueDb (peakHoldDb, frequencyHz, valueDb))
        values.add (formatCurveValue ("Peak", valueDb));

    auto differenceDb = 0.0f;

    if (showDifferenceCurve && getDifferenceCurveValueDb (frequencyHz, differenceDb))
        values.add (formatCurveValue ("Diff", differenceDb));

    return values.joinIntoString ("  ");
}

float SpectrumDisplay::applySlopeCorrection (float decibels, float frequencyHz) const
{
    const auto safeFrequency = juce::jmax (1.0f, frequencyHz);
    const auto correctionDb =
        slopeDbPerOctave * std::log2 (safeFrequency / slopeReferenceFrequencyHz);

    return decibels + correctionDb;
}

void SpectrumDisplay::updateMouseReadout (juce::Point<float> newPosition)
{
    const auto area = getSpectrumArea (getLocalBounds());

    if (! area.contains (newPosition))
    {
        if (hasMouseReadout)
        {
            hasMouseReadout = false;
            repaint();
        }

        return;
    }

    mousePosition = newPosition;
    hasMouseReadout = true;
    repaint();
}

void SpectrumDisplay::panVisibleFrequencyRangeByPixels (float deltaPixels,
                                                        juce::Rectangle<float> area)
{
    if (std::abs (deltaPixels) < 0.001f)
        return;

    if (area.getWidth() <= 1.0f)
        return;

    if (visibleMinFrequencyHz <= 0.0f
        || visibleMaxFrequencyHz <= visibleMinFrequencyHz)
    {
        resetVisibleFrequencyRangeToDefault();
        return;
    }

    const auto defaultLogMin = std::log (defaultMinFrequencyHz);
    const auto defaultLogMax = std::log (defaultMaxFrequencyHz);

    const auto currentLogMin = std::log (visibleMinFrequencyHz);
    const auto currentLogMax = std::log (visibleMaxFrequencyHz);
    const auto currentLogWidth = currentLogMax - currentLogMin;
    const auto fullLogWidth = defaultLogMax - defaultLogMin;

    if (currentLogWidth >= fullLogWidth - 0.000001f)
        return;

    const auto normalisedDelta = deltaPixels / juce::jmax (1.0f, area.getWidth());

    auto newLogMin = currentLogMin - normalisedDelta * currentLogWidth;
    auto newLogMax = newLogMin + currentLogWidth;

    if (newLogMin < defaultLogMin)
    {
        newLogMin = defaultLogMin;
        newLogMax = newLogMin + currentLogWidth;
    }

    if (newLogMax > defaultLogMax)
    {
        newLogMax = defaultLogMax;
        newLogMin = newLogMax - currentLogWidth;
    }

    setVisibleFrequencyRange (std::exp (newLogMin),
                              std::exp (newLogMax));
}

void SpectrumDisplay::zoomVisibleFrequencyRangeAround (float centreFrequencyHz,
                                                       float zoomFactor)
{
    if (zoomFactor <= 0.0f)
        return;

    if (visibleMinFrequencyHz <= 0.0f
        || visibleMaxFrequencyHz <= visibleMinFrequencyHz)
    {
        resetVisibleFrequencyRangeToDefault();
        return;
    }

    const auto clampedCentre =
        juce::jlimit (defaultMinFrequencyHz,
                      defaultMaxFrequencyHz,
                      centreFrequencyHz);

    const auto currentRatio = visibleMaxFrequencyHz / visibleMinFrequencyHz;
    const auto fullRatio = defaultMaxFrequencyHz / defaultMinFrequencyHz;

    auto targetRatio = currentRatio / zoomFactor;
    targetRatio = juce::jlimit (minimumVisibleFrequencyRatio, fullRatio, targetRatio);

    const auto logMin = std::log (visibleMinFrequencyHz);
    const auto logMax = std::log (visibleMaxFrequencyHz);
    const auto logCentre = std::log (clampedCentre);

    const auto centrePosition =
        juce::jlimit (0.0f,
                      1.0f,
                      (logCentre - logMin) / juce::jmax (0.000001f, logMax - logMin));

    const auto targetLogWidth = std::log (targetRatio);

    auto newLogMin = logCentre - centrePosition * targetLogWidth;
    auto newLogMax = newLogMin + targetLogWidth;

    const auto defaultLogMin = std::log (defaultMinFrequencyHz);
    const auto defaultLogMax = std::log (defaultMaxFrequencyHz);

    if (newLogMin < defaultLogMin)
    {
        newLogMin = defaultLogMin;
        newLogMax = newLogMin + targetLogWidth;
    }

    if (newLogMax > defaultLogMax)
    {
        newLogMax = defaultLogMax;
        newLogMin = newLogMax - targetLogWidth;
    }

    const auto newMinFrequency = std::exp (newLogMin);
    const auto newMaxFrequency = std::exp (newLogMax);

    setVisibleFrequencyRange (newMinFrequency, newMaxFrequency);
}

void SpectrumDisplay::resetVisibleFrequencyRangeToDefault()
{
    setVisibleFrequencyRange (defaultMinFrequencyHz, defaultMaxFrequencyHz);
}

int SpectrumDisplay::frequencyToMidiNote (float frequencyHz) const
{
    if (frequencyHz <= 0.0f)
        return -1;

    return juce::jlimit (
        0,
        127,
        juce::roundToInt (69.0f + 12.0f * std::log2 (frequencyHz / 440.0f)));
}

int SpectrumDisplay::midiNoteToPitchClass (int midiNote) const
{
    if (midiNote < 0)
        return -1;

    return midiNote % 12;
}

std::vector<SpectrumDisplay::PeakNoteLabel> SpectrumDisplay::buildPeakNoteLabels (
    juce::Rectangle<float> area) const
{
    std::vector<PeakNoteLabel> candidates;

    if (! showPeakHoldCurve || notePeaks.empty())
        return candidates;

    for (const auto& notePeak : notePeaks)
    {
        if (notePeak.frequencyHz < visibleMinFrequencyHz
            || notePeak.frequencyHz > visibleMaxFrequencyHz
            || notePeak.midiNote < 0)
        {
            continue;
        }

        auto displayDb = applySlopeCorrection (notePeak.decibels, notePeak.frequencyHz);

        float peakHoldCurveDb = displayDb;

        if (getInterpolatedCurveValueDb (peakHoldDb, notePeak.frequencyHz, peakHoldCurveDb))
            displayDb = peakHoldCurveDb;

        const auto x = frequencyToX (notePeak.frequencyHz, area);
        const auto yDb = juce::jlimit (minDecibels, maxDecibels, displayDb);
        const auto y = decibelsToY (yDb, area);

        if (! area.contains (juce::Point<float> (x, y)))
            continue;

        candidates.push_back ({
            notePeak.frequencyHz,
            displayDb,
            x,
            y,
            notePeak.midiNote,
            frequencyToNoteName (notePeak.frequencyHz)
        });
    }

    std::sort (candidates.begin(),
               candidates.end(),
               [] (const auto& first, const auto& second)
               {
                   return first.decibels > second.decibels;
               });

    std::vector<PeakNoteLabel> selected;
    selected.reserve (10);

    constexpr auto maxLabels = static_cast<size_t> (10);
    constexpr auto minNoteLabelDistancePixels = 34.0f;

    for (const auto& candidate : candidates)
    {
        const auto duplicateMidiNote =
            std::any_of (selected.begin(),
                         selected.end(),
                         [&candidate] (const auto& existing)
                         {
                             return existing.midiNote == candidate.midiNote;
                         });

        if (duplicateMidiNote)
            continue;

        const auto tooClose =
            std::any_of (selected.begin(),
                         selected.end(),
                         [&candidate] (const auto& existing)
                         {
                             return std::abs (candidate.x - existing.x) < minNoteLabelDistancePixels;
                         });

        if (tooClose)
            continue;

        selected.push_back (candidate);

        if (selected.size() >= maxLabels)
            break;
    }

    std::sort (selected.begin(),
               selected.end(),
               [] (const auto& first, const auto& second)
               {
                   return first.x < second.x;
               });

    return selected;
}

std::vector<SpectrumDisplay::SpectrumExtremumMarker>
    SpectrumDisplay::buildPeakDipMarkers (juce::Rectangle<float> area) const
{
    std::vector<SpectrumExtremumMarker> markers;

    const auto* curveData = getCurveDataForSource (peakDipCurveSource);

    if (curveData == nullptr || curveData->size() < 2)
        return markers;

    if (visibleMinFrequencyHz <= 0.0f || visibleMaxFrequencyHz <= visibleMinFrequencyHz)
        return markers;

    std::array<float, peakDipSamplingPoints> frequencies {};
    std::array<float, peakDipSamplingPoints> decibels {};
    std::array<bool, peakDipSamplingPoints> isValid {};

    for (int i = 0; i < peakDipSamplingPoints; ++i)
    {
        const auto normalisedX =
            static_cast<float> (i)
            / static_cast<float> (peakDipSamplingPoints - 1);

        const auto frequency =
            visibleMinFrequencyHz
            * std::pow (visibleMaxFrequencyHz / visibleMinFrequencyHz, normalisedX);

        auto valueDb = 0.0f;

        const auto valid =
            getInterpolatedCurveValueDbForDataRange (*curveData,
                                                     frequency,
                                                     dataMinFrequencyHz,
                                                     dataMaxFrequencyHz,
                                                     valueDb)
            && valueDb > minDecibels + 1.0f;

        frequencies[static_cast<size_t> (i)] = frequency;
        decibels[static_cast<size_t> (i)] = valueDb;
        isValid[static_cast<size_t> (i)] = valid;
    }

    const auto visibleOctaves =
        std::log2 (visibleMaxFrequencyHz / visibleMinFrequencyHz);

    if (visibleOctaves <= 0.0f)
        return markers;

    const auto samplesPerOctave =
        static_cast<float> (peakDipSamplingPoints - 1) / visibleOctaves;

    const auto neighbourWindowSamples =
        juce::jmax (2,
            juce::roundToInt (peakDipNeighbourWindowOctaves * samplesPerOctave));

    std::vector<SpectrumExtremumMarker> peakCandidates;
    std::vector<SpectrumExtremumMarker> dipCandidates;
    peakCandidates.reserve (24);
    dipCandidates.reserve (24);

    for (int i = 1; i < peakDipSamplingPoints - 1; ++i)
    {
        const auto index = static_cast<size_t> (i);

        if (!isValid[index]
            || !isValid[static_cast<size_t> (i - 1)]
            || !isValid[static_cast<size_t> (i + 1)])
        {
            continue;
        }

        const auto valueDb = decibels[index];
        const auto isPeak =
            showPeakMarkers
            && valueDb > decibels[static_cast<size_t> (i - 1)]
            && valueDb >= decibels[static_cast<size_t> (i + 1)];

        const auto isDip =
            showDipMarkers
            && valueDb < decibels[static_cast<size_t> (i - 1)]
            && valueDb <= decibels[static_cast<size_t> (i + 1)];

        if (!isPeak && !isDip)
            continue;

        const auto leftStart = juce::jmax (0, i - neighbourWindowSamples);
        const auto rightEnd =
            juce::jmin (peakDipSamplingPoints - 1, i + neighbourWindowSamples);

        auto leftMin = valueDb;
        auto rightMin = valueDb;
        auto leftMax = valueDb;
        auto rightMax = valueDb;

        for (int j = leftStart; j < i; ++j)
        {
            const auto sampleIndex = static_cast<size_t> (j);

            if (!isValid[sampleIndex])
                continue;

            leftMin = juce::jmin (leftMin, decibels[sampleIndex]);
            leftMax = juce::jmax (leftMax, decibels[sampleIndex]);
        }

        for (int j = i + 1; j <= rightEnd; ++j)
        {
            const auto sampleIndex = static_cast<size_t> (j);

            if (!isValid[sampleIndex])
                continue;

            rightMin = juce::jmin (rightMin, decibels[sampleIndex]);
            rightMax = juce::jmax (rightMax, decibels[sampleIndex]);
        }

        auto prominenceDb = 0.0f;
        auto kind = SpectrumExtremumKind::peak;

        if (isPeak)
        {
            prominenceDb = valueDb - juce::jmax (leftMin, rightMin);
            kind = SpectrumExtremumKind::peak;
        }
        else
        {
            prominenceDb = juce::jmin (leftMax, rightMax) - valueDb;
            kind = SpectrumExtremumKind::dip;
        }

        if (prominenceDb < minimumPeakDipProminenceDb)
            continue;

        const auto frequencyHz = frequencies[index];
        const auto x = frequencyToX (frequencyHz, area);
        const auto y = decibelsToY (valueDb, area);

        if (!area.contains (juce::Point<float> (x, y)))
            continue;

        auto marker = SpectrumExtremumMarker {};
        marker.kind = kind;
        marker.frequencyHz = frequencyHz;
        marker.decibels = valueDb;
        marker.prominenceDb = prominenceDb;
        marker.x = x;
        marker.y = y;

        if (kind == SpectrumExtremumKind::peak)
            peakCandidates.push_back (marker);
        else
            dipCandidates.push_back (marker);
    }

    const auto maxMarkersPerKind =
        static_cast<size_t> (maxPeakDipMarkersPerKind);

    const auto minimumSpacingOctaves = minimumPeakDipSpacingOctaves;

    const auto selectMarkers =
        [maxMarkersPerKind, minimumSpacingOctaves] (
            std::vector<SpectrumExtremumMarker>& candidates)
        {
            std::sort (candidates.begin(),
                       candidates.end(),
                       [] (const auto& first, const auto& second)
                       {
                           return first.prominenceDb > second.prominenceDb;
                       });

            std::vector<SpectrumExtremumMarker> selected;
            selected.reserve (maxMarkersPerKind);

            for (const auto& candidate : candidates)
            {
                const auto isFarEnough =
                    std::all_of (selected.begin(),
                                 selected.end(),
                                 [&candidate, minimumSpacingOctaves] (
                                     const auto& existing)
                                 {
                                     if (candidate.frequencyHz <= 0.0f
                                         || existing.frequencyHz <= 0.0f)
                                     {
                                         return false;
                                     }

                                     return std::abs (std::log2 (
                                                candidate.frequencyHz
                                                / existing.frequencyHz))
                                            >= minimumSpacingOctaves;
                                 });

                if (!isFarEnough)
                    continue;

                selected.push_back (candidate);

                if (selected.size() >= maxMarkersPerKind)
                    break;
            }

            return selected;
        };

    auto selectedPeaks = selectMarkers (peakCandidates);
    auto selectedDips = selectMarkers (dipCandidates);

    markers.reserve (selectedPeaks.size() + selectedDips.size());
    markers.insert (markers.end(), selectedPeaks.begin(), selectedPeaks.end());
    markers.insert (markers.end(), selectedDips.begin(), selectedDips.end());

    std::sort (markers.begin(),
               markers.end(),
               [] (const auto& first, const auto& second)
               {
                   return first.frequencyHz < second.frequencyHz;
               });

    return markers;
}

void SpectrumDisplay::drawBackground (juce::Graphics& g, juce::Rectangle<int> bounds)
{
    juce::ignoreUnused (bounds);

    g.fillAll (juce::Colour::fromRGB (10, 12, 16));
}

void SpectrumDisplay::drawFrequencyGrid (juce::Graphics& g, juce::Rectangle<int> bounds)
{
    const auto drawArea = getSpectrumArea (bounds);

    if (visibleMinFrequencyHz <= 0.0f || visibleMaxFrequencyHz <= visibleMinFrequencyHz)
        return;

    g.setFont (juce::FontOptions (11.0f));

    const auto visibleRatio = visibleMaxFrequencyHz / visibleMinFrequencyHz;
    const auto shouldLabelSecondaryMarkers = visibleRatio <= 32.0f;

    drawFrequencyGridLine (g, drawArea, visibleMinFrequencyHz, 0.12f, true);

    const std::array<float, 3> multipliers { 1.0f, 2.0f, 5.0f };

    const auto startPower =
        static_cast<int> (std::floor (std::log10 (visibleMinFrequencyHz))) - 1;

    const auto endPower =
        static_cast<int> (std::ceil (std::log10 (visibleMaxFrequencyHz))) + 1;

    for (auto power = startPower; power <= endPower; ++power)
    {
        const auto decade = std::pow (10.0f, static_cast<float> (power));

        for (const auto multiplier : multipliers)
        {
            const auto frequency = decade * multiplier;

            if (frequency <= visibleMinFrequencyHz || frequency >= visibleMaxFrequencyHz)
                continue;

            const auto isMajor = std::abs (multiplier - 1.0f) < 0.001f;
            const auto shouldDrawLabel = isMajor || shouldLabelSecondaryMarkers;
            const auto lineAlpha = isMajor ? 0.16f : 0.10f;

            drawFrequencyGridLine (g, drawArea, frequency, lineAlpha, shouldDrawLabel);
        }
    }

    drawFrequencyGridLine (g, drawArea, visibleMaxFrequencyHz, 0.12f, true);
}

void SpectrumDisplay::drawFrequencyGridLine (juce::Graphics& g,
                                             juce::Rectangle<float> drawArea,
                                             float frequencyHz,
                                             float alpha,
                                             bool shouldDrawLabel)
{
    if (frequencyHz < visibleMinFrequencyHz || frequencyHz > visibleMaxFrequencyHz)
        return;

    const auto x = frequencyToX (frequencyHz, drawArea);

    g.setColour (juce::Colours::white.withAlpha (alpha));
    g.drawVerticalLine (juce::roundToInt (x), drawArea.getY(), drawArea.getBottom());

    if (! shouldDrawLabel)
        return;

    g.setColour (juce::Colours::white.withAlpha (0.55f));
    g.drawText (formatFrequencyGridLabel (frequencyHz),
                juce::Rectangle<float> (x - 24.0f, drawArea.getBottom() + 4.0f, 48.0f, 16.0f),
                juce::Justification::centred);
}

void SpectrumDisplay::drawDecibelGrid (juce::Graphics& g, juce::Rectangle<int> bounds)
{
    const auto drawArea = getSpectrumArea (bounds);

    g.setFont (juce::FontOptions (11.0f));

    constexpr auto numGridLines = 6;

    for (int i = 0; i < numGridLines; ++i)
    {
        const auto normalised = static_cast<float> (i) / static_cast<float> (numGridLines - 1);
        const auto db = juce::jmap (normalised, 0.0f, 1.0f, maxDecibels, minDecibels);
        const auto y = decibelsToY (db, drawArea);

        g.setColour (juce::Colours::white.withAlpha (0.12f));
        g.drawHorizontalLine (juce::roundToInt (y), drawArea.getX(), drawArea.getRight());

        g.setColour (juce::Colours::white.withAlpha (0.55f));
        g.drawText (juce::String (static_cast<int> (db)) + " dB",
                    juce::Rectangle<float> (4.0f, y - 8.0f, 34.0f, 16.0f),
                    juce::Justification::centredRight);
    }
}

void SpectrumDisplay::drawPlaceholderCurve (juce::Graphics& g, juce::Rectangle<int> bounds)
{
    juce::Path curve;

    const auto area = getSpectrumArea (bounds);

    curve.startNewSubPath (area.getX(), area.getCentreY());

    for (int i = 0; i < area.getWidth(); ++i)
    {
        const auto x = area.getX() + static_cast<float> (i);
        const auto normalisedX = static_cast<float> (i) / juce::jmax (1.0f, area.getWidth());

        const auto y =
            area.getCentreY()
            - std::sin (normalisedX * juce::MathConstants<float>::twoPi * 3.0f) * 35.0f
            - std::sin (normalisedX * juce::MathConstants<float>::twoPi * 11.0f) * 12.0f;

        curve.lineTo (x, y);
    }

    g.setColour (juce::Colour::fromRGB (90, 220, 255));
    g.strokePath (curve, juce::PathStrokeType (2.0f));
}

void SpectrumDisplay::drawInputLevelMeter (juce::Graphics& g, juce::Rectangle<int> bounds)
{
    auto meterBounds = bounds.reduced (16).removeFromBottom (24).toFloat();

    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillRoundedRectangle (meterBounds, 4.0f);

    const auto clampedDb = juce::jlimit (-100.0f, 0.0f, inputLevelDb);
    const auto normalised = juce::jmap (clampedDb, -100.0f, 0.0f, 0.0f, 1.0f);

    auto fillBounds = meterBounds;
    fillBounds.setWidth (meterBounds.getWidth() * normalised);

    g.setColour (juce::Colour::fromRGB (90, 220, 255));
    g.fillRoundedRectangle (fillBounds, 4.0f);

    g.setColour (juce::Colours::white.withAlpha (0.8f));
    g.setFont (juce::FontOptions (12.0f));
    g.drawText ("Input: " + juce::String (inputLevelDb, 1) + " dB",
                meterBounds.toNearestInt().reduced (6, 0),
                juce::Justification::centredLeft);
}

void SpectrumDisplay::drawSpectrumCurve (juce::Graphics& g, juce::Rectangle<int> bounds)
{
    drawCurveFromData (g,
                       bounds,
                       spectrumDb,
                       juce::Colour::fromRGB (90, 220, 255),
                       2.0f,
                       CurveRenderMode::pixelResampled);
}

void SpectrumDisplay::drawFrozenReferenceCurve (juce::Graphics& g,
                                                juce::Rectangle<int> bounds)
{
    if (referenceCurves.empty())
        return;

    for (size_t i = 0; i < referenceCurves.size(); ++i)
    {
        const auto& reference = referenceCurves[i];

        if (!reference.visible)
            continue;

        const auto* referenceData =
            getReferenceCurveDataForSource (reference, AnalyzerCurveSource::live);

        if (referenceData == nullptr || referenceData->size() < 2)
            continue;

        const auto isActive =
            static_cast<int> (i) == activeReferenceIndex;

        drawCurveFromDataRange (g,
                                bounds,
                                *referenceData,
                                reference.dataMinFrequencyHz,
                                reference.dataMaxFrequencyHz,
                                juce::Colours::white.withAlpha (isActive ? 0.40f : 0.20f),
                                isActive ? 1.35f : 1.0f);
    }
}

void SpectrumDisplay::drawEnergyCurve (juce::Graphics& g, juce::Rectangle<int> bounds)
{
    drawCurveFromData (g,
                       bounds,
                       energyDb,
                       juce::Colour::fromRGB (120, 255, 160).withAlpha (0.78f),
                       1.75f);
}

void SpectrumDisplay::drawPeakHoldCurve (juce::Graphics& g, juce::Rectangle<int> bounds)
{
    drawCurveFromData (g,
                       bounds,
                       peakHoldDb,
                       juce::Colour::fromRGB (255, 190, 80).withAlpha (0.9f),
                       1.5f);
}

void SpectrumDisplay::drawRmsCurve (juce::Graphics& g, juce::Rectangle<int> bounds)
{
    drawCurveFromData (g,
                       bounds,
                       rmsDb,
                       juce::Colour::fromRGB (150, 120, 255).withAlpha (0.85f),
                       2.0f);
}

void SpectrumDisplay::drawDifferenceCurve (juce::Graphics& g,
                                           juce::Rectangle<int> bounds)
{
    if (!showDifferenceCurve
        || activeReferenceIndex < 0
        || activeReferenceIndex >= static_cast<int> (referenceCurves.size()))
    {
        return;
    }

    if (visibleMinFrequencyHz <= 0.0f || visibleMaxFrequencyHz <= visibleMinFrequencyHz)
        return;

    const auto area = getSpectrumArea (bounds);
    const auto zeroY = differenceDecibelsToY (0.0f, area);

    g.setColour (juce::Colours::white.withAlpha (0.18f));
    g.drawHorizontalLine (juce::roundToInt (zeroY), area.getX(), area.getRight());

    const auto pixelCount =
        juce::jmax (2, juce::roundToInt (area.getWidth()));

    juce::Path curve;
    auto hasStartedPath = false;

    for (int px = 0; px <= pixelCount; ++px)
    {
        const auto normalisedX =
            static_cast<float> (px) / static_cast<float> (pixelCount);

        const auto frequency =
            visibleMinFrequencyHz
            * std::pow (visibleMaxFrequencyHz / visibleMinFrequencyHz, normalisedX);

        auto differenceDb = 0.0f;

        if (! getDifferenceCurveValueDb (frequency, differenceDb))
            continue;

        const auto x = area.getX() + normalisedX * area.getWidth();
        const auto y = differenceDecibelsToY (differenceDb, area);

        if (!hasStartedPath)
        {
            curve.startNewSubPath (x, y);
            hasStartedPath = true;
        }
        else
        {
            curve.lineTo (x, y);
        }
    }

    if (!hasStartedPath)
        return;

    const auto colour = juce::Colour::fromRGB (255, 110, 205);
    g.setColour (colour.withAlpha (0.92f));
    g.strokePath (curve, juce::PathStrokeType (1.75f));

    const auto& reference =
        referenceCurves[static_cast<size_t> (activeReferenceIndex)];

    const auto label =
        "Diff: "
        + getCurveSourceLabel (differenceCurveSource)
        + " - "
        + reference.name
        + " +/-"
        + juce::String (juce::roundToInt (differenceViewRangeDb))
        + " dB";

    auto labelBounds =
        juce::Rectangle<float> (0.0f, 0.0f, 168.0f, 20.0f);

    labelBounds.setX (area.getRight() - labelBounds.getWidth() - 8.0f);
    labelBounds.setY (area.getY() + 34.0f);

    g.setColour (juce::Colours::black.withAlpha (0.58f));
    g.fillRoundedRectangle (labelBounds, 4.0f);

    g.setColour (colour.withAlpha (0.85f));
    g.setFont (juce::FontOptions (11.0f));
    g.drawText (label,
                labelBounds.toNearestInt().reduced (7, 0),
                juce::Justification::centredLeft,
                true);
}

void SpectrumDisplay::drawPeakDipMarkers (juce::Graphics& g,
                                          juce::Rectangle<int> bounds)
{
    if (!showPeakDipMarkers)
        return;

    const auto area = getSpectrumArea (bounds);
    const auto markers = buildPeakDipMarkers (area);

    if (markers.empty())
        return;

    g.setFont (juce::FontOptions (10.5f));

    for (const auto& marker : markers)
    {
        const auto isPeak = marker.kind == SpectrumExtremumKind::peak;
        const auto colour =
            isPeak
                ? juce::Colour::fromRGB (255, 214, 96)
                : juce::Colour::fromRGB (100, 210, 255);

        juce::Path triangle;

        if (isPeak)
        {
            triangle.startNewSubPath (marker.x, marker.y - 11.0f);
            triangle.lineTo (marker.x - 5.0f, marker.y - 3.0f);
            triangle.lineTo (marker.x + 5.0f, marker.y - 3.0f);
        }
        else
        {
            triangle.startNewSubPath (marker.x, marker.y + 11.0f);
            triangle.lineTo (marker.x - 5.0f, marker.y + 3.0f);
            triangle.lineTo (marker.x + 5.0f, marker.y + 3.0f);
        }

        triangle.closeSubPath();

        g.setColour (colour.withAlpha (0.92f));
        g.fillPath (triangle);

        const auto label =
            juce::String (isPeak ? "Peak " : "Dip ")
            + formatFrequency (marker.frequencyHz)
            + " "
            + juce::String (marker.decibels, 1);

        constexpr auto labelWidth = 82.0f;
        constexpr auto labelHeight = 17.0f;

        const auto labelX =
            juce::jlimit (area.getX(),
                area.getRight() - labelWidth,
                marker.x - labelWidth * 0.5f);

        const auto rawLabelY =
            isPeak
                ? marker.y - 31.0f
                : marker.y + 14.0f;

        const auto labelY =
            juce::jlimit (area.getY(),
                area.getBottom() - labelHeight,
                rawLabelY);

        const auto labelBounds =
            juce::Rectangle<float> (labelX, labelY, labelWidth, labelHeight);

        g.setColour (juce::Colours::black.withAlpha (0.56f));
        g.fillRoundedRectangle (labelBounds, 3.0f);

        g.setColour (colour.withAlpha (0.92f));
        g.drawText (label,
                    labelBounds.toNearestInt().reduced (5, 0),
                    juce::Justification::centred,
                    true);
    }
}

void SpectrumDisplay::drawCurveFromData (juce::Graphics& g,
                                         juce::Rectangle<int> bounds,
                                         const std::vector<float>& values,
                                         juce::Colour colour,
                                         float strokeWidth,
                                         CurveRenderMode renderMode)
{
    drawCurveFromDataRange (g,
                            bounds,
                            values,
                            dataMinFrequencyHz,
                            dataMaxFrequencyHz,
                            colour,
                            strokeWidth,
                            renderMode);
}

void SpectrumDisplay::drawCurveFromDataRange (juce::Graphics& g,
                                              juce::Rectangle<int> bounds,
                                              const std::vector<float>& values,
                                              float sourceMinFrequencyHz,
                                              float sourceMaxFrequencyHz,
                                              juce::Colour colour,
                                              float strokeWidth,
                                              CurveRenderMode renderMode)
{
    if (values.size() < 2)
        return;

    if (visibleMinFrequencyHz <= 0.0f || visibleMaxFrequencyHz <= visibleMinFrequencyHz)
        return;

    const auto area = getSpectrumArea (bounds);

    juce::Path curve;
    auto hasStartedPath = false;

    if (renderMode == CurveRenderMode::dataPoints)
    {
        for (size_t i = 0; i < values.size(); ++i)
        {
            const auto normalisedX =
                static_cast<float> (i) / static_cast<float> (values.size() - 1);

            const auto frequency =
                visibleMinFrequencyHz
                * std::pow (visibleMaxFrequencyHz / visibleMinFrequencyHz, normalisedX);

            float valueDb = 0.0f;

            if (! getInterpolatedCurveValueDbForDataRange (values,
                                                           frequency,
                                                           sourceMinFrequencyHz,
                                                           sourceMaxFrequencyHz,
                                                           valueDb))
                continue;

            const auto x = area.getX() + normalisedX * area.getWidth();

            const auto db =
                juce::jlimit (minDecibels, maxDecibels, valueDb);

            const auto y = decibelsToY (db, area);

            if (! hasStartedPath)
            {
                curve.startNewSubPath (x, y);
                hasStartedPath = true;
            }
            else
            {
                curve.lineTo (x, y);
            }
        }
    }
    else
    {
        const auto pixelCount =
            juce::jmax (2, juce::roundToInt (area.getWidth()));

        for (int px = 0; px <= pixelCount; ++px)
        {
            const auto normalisedX =
                static_cast<float> (px) / static_cast<float> (pixelCount);

            const auto frequency =
                visibleMinFrequencyHz
                * std::pow (visibleMaxFrequencyHz / visibleMinFrequencyHz, normalisedX);

            float valueDb = 0.0f;

            if (! getInterpolatedCurveValueDbForDataRange (values,
                                                           frequency,
                                                           sourceMinFrequencyHz,
                                                           sourceMaxFrequencyHz,
                                                           valueDb))
                continue;

            const auto x = area.getX() + normalisedX * area.getWidth();

            const auto db =
                juce::jlimit (minDecibels, maxDecibels, valueDb);

            const auto y = decibelsToY (db, area);

            if (! hasStartedPath)
            {
                curve.startNewSubPath (x, y);
                hasStartedPath = true;
            }
            else
            {
                curve.lineTo (x, y);
            }
        }
    }

    if (! hasStartedPath)
        return;

    g.setColour (colour);
    g.strokePath (curve, juce::PathStrokeType (strokeWidth));
}

void SpectrumDisplay::drawLegend (juce::Graphics& g, juce::Rectangle<int> bounds)
{
    auto legendBounds = bounds.reduced (16).removeFromTop (26).toFloat();
    legendBounds.removeFromLeft (140.0f);

    struct LegendItem
    {
        juce::String label;
        juce::Colour colour;
        bool visible;
        bool shouldDraw;
    };

    const std::array<LegendItem, 5> items {{
        { "Live", juce::Colour::fromRGB (90, 220, 255), true, showLiveCurve && ! spectrumDb.empty() },
        { "Energy", juce::Colour::fromRGB (120, 255, 160).withAlpha (0.78f), true, showEnergyCurve && ! energyDb.empty() },
        { "RMS", juce::Colour::fromRGB (150, 120, 255).withAlpha (0.85f), true, showRmsCurve && ! rmsDb.empty() },
        { "Peak", juce::Colour::fromRGB (255, 190, 80).withAlpha (0.9f), true, showPeakHoldCurve && ! peakHoldDb.empty() },
        { "Ref", juce::Colours::white.withAlpha (0.34f), true, hasFrozenReferenceSpectrum() }
    }};

    g.setFont (juce::FontOptions (12.0f));

    auto x = legendBounds.getX();

    for (const auto& item : items)
    {
        if (! item.shouldDraw)
            continue;

        const auto itemWidth = 64.0f;
        auto itemArea = juce::Rectangle<float> (x, legendBounds.getY(), itemWidth, legendBounds.getHeight());

        g.setColour (item.colour.withAlpha (item.visible ? 1.0f : 0.25f));
        g.fillRoundedRectangle (itemArea.getX(), itemArea.getCentreY() - 3.0f, 18.0f, 6.0f, 3.0f);

        g.setColour (juce::Colours::white.withAlpha (item.visible ? 0.75f : 0.28f));
        g.drawText (item.label,
                    itemArea.withTrimmedLeft (24.0f),
                    juce::Justification::centredLeft,
                    false);

        x += itemWidth + 10.0f;
    }
}

void SpectrumDisplay::drawVisibleFrequencyRangeIndicator (juce::Graphics& g,
                                                          juce::Rectangle<int> bounds)
{
    if (isVisibleFrequencyRangeDefault())
        return;

    const auto drawArea = getSpectrumArea (bounds);

    const auto rangeSeparator =
        juce::String (juce::CharPointer_UTF8 (" \xe2\x80\x93 "));

    const auto text =
        juce::String ("Zoom: ")
        + formatFrequencyRangeValue (visibleMinFrequencyHz)
        + rangeSeparator
        + formatFrequencyRangeValue (visibleMaxFrequencyHz);

    g.setFont (juce::FontOptions (12.0f));

    const auto textWidth =
        juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), text);

    const auto indicatorWidth =
        juce::jmin (drawArea.getWidth() - 16.0f,
                    textWidth + 18.0f);

    if (indicatorWidth <= 40.0f)
        return;

    const auto indicatorHeight = 22.0f;

    auto indicatorBounds =
        juce::Rectangle<float> (0.0f, 0.0f, indicatorWidth, indicatorHeight);

    indicatorBounds.setX (drawArea.getRight() - indicatorWidth - 8.0f);
    indicatorBounds.setY (drawArea.getY() + 8.0f);

    g.setColour (juce::Colours::black.withAlpha (0.58f));
    g.fillRoundedRectangle (indicatorBounds, 5.0f);

    g.setColour (juce::Colours::white.withAlpha (0.16f));
    g.drawRoundedRectangle (indicatorBounds, 5.0f, 1.0f);

    g.setColour (juce::Colours::white.withAlpha (0.78f));
    g.drawText (text,
                indicatorBounds.toNearestInt().reduced (8, 0),
                juce::Justification::centred,
                true);
}

void SpectrumDisplay::drawPeakNoteLabels (juce::Graphics& g, juce::Rectangle<int> bounds)
{
    const auto area = getSpectrumArea (bounds);
    const auto labels = buildPeakNoteLabels (area);

    if (labels.empty())
        return;

    g.setFont (juce::FontOptions (11.0f));

    constexpr auto labelWidth = 38.0f;
    constexpr auto labelHeight = 18.0f;

    for (const auto& label : labels)
    {
        auto labelY = label.y - 24.0f;

        if (labelY < area.getY())
            labelY = label.y + 8.0f;

        labelY = juce::jlimit (area.getY(),
                               area.getBottom() - labelHeight,
                               labelY);

        const auto labelX =
            juce::jlimit (area.getX(),
                          area.getRight() - labelWidth,
                          label.x - labelWidth * 0.5f);

        const auto labelBounds =
            juce::Rectangle<float> (labelX, labelY, labelWidth, labelHeight);

        const auto markerEndY =
            labelBounds.getCentreY() < label.y
                ? labelBounds.getBottom()
                : labelBounds.getY();

        g.setColour (juce::Colour::fromRGB (255, 190, 80).withAlpha (0.32f));
        g.drawLine (label.x, label.y, label.x, markerEndY, 1.0f);

        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillRoundedRectangle (labelBounds, 4.0f);

        g.setColour (juce::Colours::white.withAlpha (0.18f));
        g.drawRoundedRectangle (labelBounds, 4.0f, 1.0f);

        g.setColour (juce::Colours::white.withAlpha (0.78f));
        g.drawText (label.noteName,
                    labelBounds.toNearestInt(),
                    juce::Justification::centred,
                    true);
    }
}

void SpectrumDisplay::drawMouseReadout (juce::Graphics& g, juce::Rectangle<int> bounds)
{
    if (! hasMouseReadout)
        return;

    const auto area = getSpectrumArea (bounds);

    if (! area.contains (mousePosition) || area.getWidth() <= 96.0f || area.getHeight() <= 40.0f)
        return;

    const auto frequency = xToFrequency (mousePosition.x, area);
    const auto db = yToDecibels (mousePosition.y, area);
    const auto firstLineText =
        formatFrequency (frequency)
        + "  "
        + frequencyToNoteName (frequency)
        + "  "
        + juce::String (db, 1)
        + " dB";
    const auto curveReadoutText = buildCurveReadoutText (frequency);
    const auto hasCurveReadout = curveReadoutText.isNotEmpty();

    g.setColour (juce::Colours::white.withAlpha (0.18f));
    g.drawLine (mousePosition.x, area.getY(), mousePosition.x, area.getBottom(), 1.0f);
    g.drawLine (area.getX(), mousePosition.y, area.getRight(), mousePosition.y, 1.0f);

    g.setFont (juce::FontOptions (12.0f));

    const auto readoutHeight = hasCurveReadout ? 40.0f : 24.0f;
    const auto maxReadoutWidth = area.getWidth() - 12.0f;
    const auto readoutWidth =
        juce::jmin (maxReadoutWidth,
                    juce::jmax (
                        juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), firstLineText),
                        juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), curveReadoutText))
                    + 18.0f);

    const auto readoutX =
        juce::jlimit (area.getX() + 6.0f,
                      area.getRight() - readoutWidth - 6.0f,
                      mousePosition.x + 12.0f);

    const auto readoutY =
        juce::jlimit (area.getY() + 6.0f,
                      area.getBottom() - readoutHeight - 6.0f,
                      mousePosition.y - 34.0f);

    const auto readoutBounds =
        juce::Rectangle<float> (readoutX, readoutY, readoutWidth, readoutHeight);

    g.setColour (juce::Colours::black.withAlpha (0.72f));
    g.fillRoundedRectangle (readoutBounds, 4.0f);

    g.setColour (juce::Colours::white.withAlpha (0.18f));
    g.drawRoundedRectangle (readoutBounds, 4.0f, 1.0f);

    g.setColour (juce::Colours::white.withAlpha (0.88f));
    auto textBounds = readoutBounds.toNearestInt().reduced (8, 0);
    g.drawText (firstLineText,
                hasCurveReadout ? textBounds.removeFromTop (20) : textBounds,
                juce::Justification::centredLeft,
                true);

    if (hasCurveReadout)
    {
        g.setColour (juce::Colours::white.withAlpha (0.72f));
        g.drawText (curveReadoutText,
                    textBounds,
                    juce::Justification::centredLeft,
                    true);
    }
}
