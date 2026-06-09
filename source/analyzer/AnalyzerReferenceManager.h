#pragma once

#include "AnalyzerEngine.h"

#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>
#include <juce_graphics/juce_graphics.h>
#include <vector>

struct AnalyzerReferenceCurve
{
    juce::String id;
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

class AnalyzerReferenceManager
{
public:
    static constexpr int maxReferences = 8;

    int addReferenceFromFrame (const AnalyzerEngine::Frame& frame,
                               const juce::String& name);

    void clear();
    bool removeReference (int index);
    bool renameReference (int index, const juce::String& name);
    bool setReferenceVisible (int index, bool visible);
    bool setActiveReferenceIndex (int index);

    int getNumReferences() const noexcept;
    int getActiveReferenceIndex() const noexcept;
    const AnalyzerReferenceCurve* getReference (int index) const noexcept;
    std::vector<AnalyzerReferenceCurve> getReferencesCopy() const;

    juce::ValueTree toValueTree() const;
    void restoreFromValueTree (const juce::ValueTree& tree);

private:
    // TODO: Reuse this ValueTree schema for .fsref file import/export.
    static juce::Colour makeReferenceColour (int index) noexcept;
    static juce::var vectorToVar (const std::vector<float>& values);
    static bool restoreVectorFromVar (const juce::var& value,
                                      std::vector<float>& destination);
    static juce::String makeDefaultName (int index);
    void clampActiveIndex() noexcept;

    std::vector<AnalyzerReferenceCurve> references;
    int activeReferenceIndex = -1;
};
