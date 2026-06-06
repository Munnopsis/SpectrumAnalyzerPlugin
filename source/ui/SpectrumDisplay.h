#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class SpectrumDisplay : public juce::Component
{
public:
    SpectrumDisplay();
    ~SpectrumDisplay() override = default;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    void drawBackground (juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawFrequencyGrid (juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawDecibelGrid (juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawPlaceholderCurve (juce::Graphics& g, juce::Rectangle<int> bounds);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectrumDisplay)
};