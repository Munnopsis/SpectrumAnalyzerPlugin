#include "SpectrumDisplay.h"

SpectrumDisplay::SpectrumDisplay()
{
    setOpaque (true);
}

void SpectrumDisplay::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    drawBackground (g, bounds);
    drawFrequencyGrid (g, bounds);
    drawDecibelGrid (g, bounds);
    drawPlaceholderCurve (g, bounds);

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