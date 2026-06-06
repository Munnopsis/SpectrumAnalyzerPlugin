#include "SpectrumDisplay.h"
#include <algorithm>
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

juce::Rectangle<float> SpectrumDisplay::getSpectrumArea (juce::Rectangle<int> bounds) const
{
    auto area = bounds.reduced (40, 50);
    area.removeFromBottom (34);

    return area.toFloat();
}

float SpectrumDisplay::frequencyToX (float frequencyHz, juce::Rectangle<float> area) const
{
    const auto clampedFrequency = juce::jlimit (minFrequencyHz, maxFrequencyHz, frequencyHz);

    const auto normalised =
        std::log (clampedFrequency / minFrequencyHz)
        / std::log (maxFrequencyHz / minFrequencyHz);

    return area.getX() + normalised * area.getWidth();
}

float SpectrumDisplay::xToFrequency (float x, juce::Rectangle<float> area) const
{
    const auto normalised = (x - area.getX()) / juce::jmax (1.0f, area.getWidth());
    const auto clamped = juce::jlimit (0.0f, 1.0f, normalised);

    return minFrequencyHz * std::pow (maxFrequencyHz / minFrequencyHz, clamped);
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

juce::String SpectrumDisplay::frequencyToNoteName (float frequencyHz) const
{
    if (frequencyHz <= 0.0f)
        return "-";

    static constexpr std::array<const char*, 12> noteNames {
        "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
    };

    const auto midiNote = juce::jlimit (
        0,
        127,
        juce::roundToInt (69.0f + 12.0f * std::log2 (frequencyHz / 440.0f)));

    const auto octave = midiNote / 12 - 1;
    return juce::String (noteNames[static_cast<size_t> (midiNote % 12)])
           + juce::String (octave);
}

bool SpectrumDisplay::getInterpolatedCurveValueDb (const std::vector<float>& values,
                                                   float frequencyHz,
                                                   float& resultDb) const
{
    if (values.size() < 2 || frequencyHz <= 0.0f)
        return false;

    const auto normalisedX =
        std::log (frequencyHz / minFrequencyHz)
        / std::log (maxFrequencyHz / minFrequencyHz);

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

void SpectrumDisplay::drawBackground (juce::Graphics& g, juce::Rectangle<int> bounds)
{
    juce::ignoreUnused (bounds);

    g.fillAll (juce::Colour::fromRGB (10, 12, 16));
}

void SpectrumDisplay::drawFrequencyGrid (juce::Graphics& g, juce::Rectangle<int> bounds)
{
    const auto drawArea = getSpectrumArea (bounds);

    const std::array<float, 10> frequencies {
        20.0f, 50.0f, 100.0f, 200.0f, 500.0f,
        1000.0f, 2000.0f, 5000.0f, 10000.0f, 20000.0f
    };

    g.setFont (juce::FontOptions (11.0f));

    for (const auto frequency : frequencies)
    {
        const auto x = frequencyToX (frequency, drawArea);

        g.setColour (juce::Colours::white.withAlpha (0.12f));
        g.drawVerticalLine (juce::roundToInt (x), drawArea.getY(), drawArea.getBottom());

        juce::String label;

        if (frequency >= 1000.0f)
            label = juce::String (frequency / 1000.0f, frequency >= 10000.0f ? 0 : 1) + "k";
        else
            label = juce::String (static_cast<int> (frequency));

        g.setColour (juce::Colours::white.withAlpha (0.55f));
        g.drawText (label,
                    juce::Rectangle<float> (x - 24.0f, drawArea.getBottom() + 4.0f, 48.0f, 16.0f),
                    juce::Justification::centred);
    }
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
    if (spectrumDb.size() < 2)
        return;

    const auto area = getSpectrumArea (bounds);

    juce::Path curve;

    constexpr auto firstNormalisedX = 0.0f;
    const auto firstFrequency =
        minFrequencyHz * std::pow (maxFrequencyHz / minFrequencyHz, firstNormalisedX);
    const auto firstDb =
        juce::jlimit (minDecibels,
                      maxDecibels,
                      applySlopeCorrection (spectrumDb.front(), firstFrequency));

    curve.startNewSubPath (
        area.getX(),
        decibelsToY (firstDb, area)
    );

    for (size_t i = 1; i < spectrumDb.size(); ++i)
    {
        const auto normalisedX =
            static_cast<float> (i) / static_cast<float> (spectrumDb.size() - 1);

        const auto x = area.getX() + normalisedX * area.getWidth();
        const auto frequency =
            minFrequencyHz * std::pow (maxFrequencyHz / minFrequencyHz, normalisedX);

        const auto db =
            juce::jlimit (minDecibels,
                          maxDecibels,
                          applySlopeCorrection (spectrumDb[i], frequency));
        const auto y = decibelsToY (db, area);

        curve.lineTo (x, y);
    }

    g.setColour (juce::Colour::fromRGB (90, 220, 255));
    g.strokePath (curve, juce::PathStrokeType (2.0f));
}

void SpectrumDisplay::drawPeakHoldCurve (juce::Graphics& g, juce::Rectangle<int> bounds)
{
    if (peakHoldDb.size() < 2)
        return;

    const auto area = getSpectrumArea (bounds);

    juce::Path curve;

    constexpr auto firstNormalisedX = 0.0f;
    const auto firstFrequency =
        minFrequencyHz * std::pow (maxFrequencyHz / minFrequencyHz, firstNormalisedX);
    const auto firstDb =
        juce::jlimit (minDecibels,
                      maxDecibels,
                      applySlopeCorrection (peakHoldDb.front(), firstFrequency));

    curve.startNewSubPath (
        area.getX(),
        decibelsToY (firstDb, area)
    );

    for (size_t i = 1; i < peakHoldDb.size(); ++i)
    {
        const auto normalisedX =
            static_cast<float> (i) / static_cast<float> (peakHoldDb.size() - 1);

        const auto x = area.getX() + normalisedX * area.getWidth();
        const auto frequency =
            minFrequencyHz * std::pow (maxFrequencyHz / minFrequencyHz, normalisedX);

        const auto db =
            juce::jlimit (minDecibels,
                          maxDecibels,
                          applySlopeCorrection (peakHoldDb[i], frequency));
        const auto y = decibelsToY (db, area);

        curve.lineTo (x, y);
    }

    g.setColour (juce::Colour::fromRGB (255, 190, 80).withAlpha (0.9f));
    g.strokePath (curve, juce::PathStrokeType (1.5f));
}

void SpectrumDisplay::drawRmsCurve (juce::Graphics& g, juce::Rectangle<int> bounds)
{
    if (rmsDb.size() < 2)
        return;

    const auto area = getSpectrumArea (bounds);

    juce::Path curve;

    constexpr auto firstNormalisedX = 0.0f;
    const auto firstFrequency =
        minFrequencyHz * std::pow (maxFrequencyHz / minFrequencyHz, firstNormalisedX);
    const auto firstDb =
        juce::jlimit (minDecibels,
                      maxDecibels,
                      applySlopeCorrection (rmsDb.front(), firstFrequency));

    curve.startNewSubPath (
        area.getX(),
        decibelsToY (firstDb, area)
    );

    for (size_t i = 1; i < rmsDb.size(); ++i)
    {
        const auto normalisedX =
            static_cast<float> (i) / static_cast<float> (rmsDb.size() - 1);

        const auto x = area.getX() + normalisedX * area.getWidth();
        const auto frequency =
            minFrequencyHz * std::pow (maxFrequencyHz / minFrequencyHz, normalisedX);

        const auto db =
            juce::jlimit (minDecibels,
                          maxDecibels,
                          applySlopeCorrection (rmsDb[i], frequency));
        const auto y = decibelsToY (db, area);

        curve.lineTo (x, y);
    }

    g.setColour (juce::Colour::fromRGB (150, 120, 255).withAlpha (0.85f));
    g.strokePath (curve, juce::PathStrokeType (2.0f));
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
