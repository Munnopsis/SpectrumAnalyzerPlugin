#include "SpectrumDisplay.h"

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

void SpectrumDisplay::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    drawBackground (g, bounds);
    drawFrequencyGrid (g, bounds);
    drawDecibelGrid (g, bounds);

    if (spectrumDb.empty())
        drawPlaceholderCurve (g, bounds);
    else
        drawSpectrumCurve (g, bounds);

    drawInputLevelMeter (g, bounds);

    g.setColour (juce::Colours::white.withAlpha (0.85f));
    g.setFont (juce::FontOptions (18.0f, juce::Font::bold));
    g.drawText ("FullSpectrum", bounds.reduced (16), juce::Justification::topLeft);
}

void SpectrumDisplay::resized()
{
}

void SpectrumDisplay::drawBackground (juce::Graphics& g, juce::Rectangle<int> bounds)
{
    juce::ignoreUnused (bounds);

    g.fillAll (juce::Colour::fromRGB (10, 12, 16));
}

void SpectrumDisplay::drawFrequencyGrid (juce::Graphics& g, juce::Rectangle<int> bounds)
{
    g.setColour (juce::Colours::white.withAlpha (0.10f));

    constexpr int numberOfVerticalLines = 10;

    for (int i = 0; i <= numberOfVerticalLines; ++i)
    {
        const auto x = bounds.getX() + bounds.getWidth() * i / numberOfVerticalLines;
        g.drawVerticalLine (x, static_cast<float> (bounds.getY()), static_cast<float> (bounds.getBottom()));
    }
}

void SpectrumDisplay::drawDecibelGrid (juce::Graphics& g, juce::Rectangle<int> bounds)
{
    g.setColour (juce::Colours::white.withAlpha (0.10f));

    constexpr int numberOfHorizontalLines = 8;

    for (int i = 0; i <= numberOfHorizontalLines; ++i)
    {
        const auto y = bounds.getY() + bounds.getHeight() * i / numberOfHorizontalLines;
        g.drawHorizontalLine (y, static_cast<float> (bounds.getX()), static_cast<float> (bounds.getRight()));
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

    const auto minDb = -100.0f;
    const auto maxDb = 0.0f;

    const auto firstDb = juce::jlimit (minDb, maxDb, spectrumDb.front());

    curve.startNewSubPath (
        area.getX(),
        juce::jmap (firstDb, minDb, maxDb, area.getBottom(), area.getY())
    );

    for (size_t i = 1; i < spectrumDb.size(); ++i)
    {
        const auto normalisedX =
            static_cast<float> (i) / static_cast<float> (spectrumDb.size() - 1);

        const auto x = area.getX() + normalisedX * area.getWidth();

        const auto db = juce::jlimit (minDb, maxDb, spectrumDb[i]);
        const auto y = juce::jmap (db, minDb, maxDb, area.getBottom(), area.getY());

        curve.lineTo (x, y);
    }

    g.setColour (juce::Colour::fromRGB (90, 220, 255));
    g.strokePath (curve, juce::PathStrokeType (2.0f));
}