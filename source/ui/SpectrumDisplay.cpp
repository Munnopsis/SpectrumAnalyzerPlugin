#include "SpectrumDisplay.h"
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
}

void SpectrumDisplay::resized()
{
}

float SpectrumDisplay::frequencyToX (float frequencyHz, juce::Rectangle<float> area) const
{
    const auto clampedFrequency = juce::jlimit (minFrequencyHz, maxFrequencyHz, frequencyHz);

    const auto normalised =
        std::log (clampedFrequency / minFrequencyHz)
        / std::log (maxFrequencyHz / minFrequencyHz);

    return area.getX() + normalised * area.getWidth();
}

float SpectrumDisplay::decibelsToY (float decibels, juce::Rectangle<float> area) const
{
    const auto clampedDb = juce::jlimit (minDecibels, maxDecibels, decibels);
    return juce::jmap (clampedDb, minDecibels, maxDecibels, area.getBottom(), area.getY());
}

float SpectrumDisplay::applySlopeCorrection (float decibels, float frequencyHz) const
{
    const auto safeFrequency = juce::jmax (1.0f, frequencyHz);
    const auto correctionDb =
        slopeDbPerOctave * std::log2 (safeFrequency / slopeReferenceFrequencyHz);

    return decibels + correctionDb;
}

void SpectrumDisplay::drawBackground (juce::Graphics& g, juce::Rectangle<int> bounds)
{
    juce::ignoreUnused (bounds);

    g.fillAll (juce::Colour::fromRGB (10, 12, 16));
}

void SpectrumDisplay::drawFrequencyGrid (juce::Graphics& g, juce::Rectangle<int> bounds)
{
    auto area = bounds.reduced (40, 50);
    area.removeFromBottom (34);

    const auto drawArea = area.toFloat();

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
    auto area = bounds.reduced (40, 50);
    area.removeFromBottom (34);

    const auto drawArea = area.toFloat();

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

    const auto area = bounds.reduced (40, 50).toFloat();

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

    auto spectrumBounds = bounds.reduced (40, 50);
    spectrumBounds.removeFromBottom (34);

    const auto area = spectrumBounds.toFloat();

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

    auto spectrumBounds = bounds.reduced (40, 50);
    spectrumBounds.removeFromBottom (34);

    const auto area = spectrumBounds.toFloat();

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

    auto spectrumBounds = bounds.reduced (40, 50);
    spectrumBounds.removeFromBottom (34);

    const auto area = spectrumBounds.toFloat();

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
