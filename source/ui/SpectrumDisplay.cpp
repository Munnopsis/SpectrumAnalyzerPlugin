#include "SpectrumDisplay.h"
#include <algorithm>
#include <array>
#include <cmath>

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
    const auto clampedSlope = juce::jlimit (0.0f, 12.0f, newSlopeDbPerOctave);

    if (std::abs (slopeDbPerOctave - clampedSlope) < 0.001f)
        return;

    slopeDbPerOctave = clampedSlope;
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
}

void SpectrumDisplay::setCurveVisibility (bool shouldShowLive,
                                          bool shouldShowRms,
                                          bool shouldShowPeakHold)
{
    showLiveCurve = shouldShowLive;
    showRmsCurve = shouldShowRms;
    showPeakHoldCurve = shouldShowPeakHold;

    repaint();
}

void SpectrumDisplay::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    drawBackground (g, bounds);
    drawFrequencyGrid (g, bounds);
    drawDecibelGrid (g, bounds);

    if (spectrumDb.empty())
    {
        drawPlaceholderCurve (g, bounds);
    }
    else
    {
        if (showRmsCurve && ! rmsDb.empty())
            drawRmsCurve (g, bounds);

        if (showLiveCurve)
            drawSpectrumCurve (g, bounds);

        if (showPeakHoldCurve && ! peakHoldDb.empty())
            drawPeakHoldCurve (g, bounds);
    }

    drawPeakNoteLabels (g, bounds);

    drawLegend (g, bounds);

    drawInputLevelMeter (g, bounds);

    g.setColour (juce::Colours::white.withAlpha (0.85f));
    g.setFont (juce::FontOptions (18.0f, juce::Font::bold));
    g.drawText ("FullSpectrum", bounds.reduced (16), juce::Justification::topLeft);

    drawMouseReadout (g, bounds);
}

void SpectrumDisplay::resized()
{
}

void SpectrumDisplay::mouseMove (const juce::MouseEvent& event)
{
    updateMouseReadout (event.position);
}

void SpectrumDisplay::mouseDrag (const juce::MouseEvent& event)
{
    updateMouseReadout (event.position);
}

void SpectrumDisplay::mouseExit (const juce::MouseEvent& event)
{
    juce::ignoreUnused (event);

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
    auto area = bounds.reduced (40, 50);
    area.removeFromBottom (34);

    return area.toFloat();
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
    if (values.size() < 2 || frequencyHz <= 0.0f)
        return false;

    if (dataMinFrequencyHz <= 0.0f || dataMaxFrequencyHz <= dataMinFrequencyHz)
        return false;

    const auto clampedFrequency =
        juce::jlimit (dataMinFrequencyHz, dataMaxFrequencyHz, frequencyHz);

    const auto normalisedX =
        std::log (clampedFrequency / dataMinFrequencyHz)
        / std::log (dataMaxFrequencyHz / dataMinFrequencyHz);

    const auto clampedX = juce::jlimit (0.0f, 1.0f, normalisedX);
    const auto maxIndex = values.size() - 1;
    const auto position = clampedX * static_cast<float> (maxIndex);
    const auto lowerIndex = static_cast<size_t> (std::floor (position));
    const auto upperIndex = std::min (lowerIndex + 1, maxIndex);
    const auto alpha = position - static_cast<float> (lowerIndex);
    const auto interpolatedDb =
        values[lowerIndex] + alpha * (values[upperIndex] - values[lowerIndex]);

    resultDb = applySlopeCorrection (interpolatedDb, frequencyHz);
    return true;
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

    if (showPeakHoldCurve && getInterpolatedCurveValueDb (peakHoldDb, frequencyHz, valueDb))
        values.add (formatCurveValue ("Peak", valueDb));

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
                       2.0f);
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

void SpectrumDisplay::drawCurveFromData (juce::Graphics& g,
                                         juce::Rectangle<int> bounds,
                                         const std::vector<float>& values,
                                         juce::Colour colour,
                                         float strokeWidth)
{
    if (values.size() < 2)
        return;

    if (visibleMinFrequencyHz <= 0.0f || visibleMaxFrequencyHz <= visibleMinFrequencyHz)
        return;

    const auto area = getSpectrumArea (bounds);

    juce::Path curve;
    auto hasStartedPath = false;

    for (size_t i = 0; i < values.size(); ++i)
    {
        const auto normalisedX =
            static_cast<float> (i) / static_cast<float> (values.size() - 1);

        const auto frequency =
            visibleMinFrequencyHz
            * std::pow (visibleMaxFrequencyHz / visibleMinFrequencyHz, normalisedX);

        float valueDb = 0.0f;

        if (! getInterpolatedCurveValueDb (values, frequency, valueDb))
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
    };

    const std::array<LegendItem, 3> items {{
        { "Live", juce::Colour::fromRGB (90, 220, 255), showLiveCurve },
        { "RMS", juce::Colour::fromRGB (150, 120, 255).withAlpha (0.85f), showRmsCurve },
        { "Peak", juce::Colour::fromRGB (255, 190, 80).withAlpha (0.9f), showPeakHoldCurve }
    }};

    g.setFont (juce::FontOptions (12.0f));

    auto x = legendBounds.getX();

    for (const auto& item : items)
    {
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
