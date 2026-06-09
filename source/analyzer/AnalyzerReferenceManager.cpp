#include "AnalyzerReferenceManager.h"

#include <algorithm>
#include <array>

namespace
{
    const juce::Identifier referencesTreeId { "AnalyzerReferences" };
    const juce::Identifier referenceTreeId { "ReferenceCurve" };
}

int AnalyzerReferenceManager::addReferenceFromFrame (
    const AnalyzerEngine::Frame& frame,
    const juce::String& name)
{
    if (frame.liveDb.size() < 2)
        return activeReferenceIndex;

    if (references.size() >= static_cast<size_t> (maxReferences))
    {
        references.erase (references.begin());

        if (activeReferenceIndex > 0)
            --activeReferenceIndex;
    }

    AnalyzerReferenceCurve reference;
    reference.id = juce::Uuid().toString();
    reference.name = name.isNotEmpty()
                         ? name
                         : makeDefaultName (static_cast<int> (references.size()) + 1);
    reference.dataMinFrequencyHz = frame.dataMinFrequencyHz;
    reference.dataMaxFrequencyHz = frame.dataMaxFrequencyHz;
    reference.liveDb = frame.liveDb;
    reference.rmsDb = frame.rmsDb;
    reference.energyDb = frame.energyDb;
    reference.peakHoldDb = frame.peakHoldDb;
    reference.visible = true;
    reference.colour =
        makeReferenceColour (static_cast<int> (references.size()));

    references.push_back (std::move (reference));
    activeReferenceIndex = static_cast<int> (references.size()) - 1;

    return activeReferenceIndex;
}

void AnalyzerReferenceManager::clear()
{
    references.clear();
    activeReferenceIndex = -1;
}

bool AnalyzerReferenceManager::removeReference (int index)
{
    if (index < 0 || index >= static_cast<int> (references.size()))
        return false;

    references.erase (references.begin() + index);
    clampActiveIndex();

    return true;
}

bool AnalyzerReferenceManager::renameReference (
    int index,
    const juce::String& name)
{
    if (index < 0
        || index >= static_cast<int> (references.size())
        || name.isEmpty())
    {
        return false;
    }

    references[static_cast<size_t> (index)].name = name;
    return true;
}

bool AnalyzerReferenceManager::setReferenceVisible (int index, bool visible)
{
    if (index < 0 || index >= static_cast<int> (references.size()))
        return false;

    references[static_cast<size_t> (index)].visible = visible;
    return true;
}

bool AnalyzerReferenceManager::setActiveReferenceIndex (int index)
{
    if (references.empty())
    {
        activeReferenceIndex = -1;
        return index < 0;
    }

    const auto clamped =
        juce::jlimit (0, static_cast<int> (references.size()) - 1, index);

    if (activeReferenceIndex == clamped)
        return false;

    activeReferenceIndex = clamped;
    return true;
}

int AnalyzerReferenceManager::getNumReferences() const noexcept
{
    return static_cast<int> (references.size());
}

int AnalyzerReferenceManager::getActiveReferenceIndex() const noexcept
{
    return activeReferenceIndex;
}

const AnalyzerReferenceCurve* AnalyzerReferenceManager::getReference (
    int index) const noexcept
{
    if (index < 0 || index >= static_cast<int> (references.size()))
        return nullptr;

    return &references[static_cast<size_t> (index)];
}

std::vector<AnalyzerReferenceCurve>
AnalyzerReferenceManager::getReferencesCopy() const
{
    return references;
}

juce::ValueTree AnalyzerReferenceManager::toValueTree() const
{
    juce::ValueTree tree { referencesTreeId };
    tree.setProperty ("version", 1, nullptr);
    tree.setProperty ("active", activeReferenceIndex, nullptr);

    for (const auto& reference : references)
    {
        juce::ValueTree child { referenceTreeId };
        child.setProperty ("id", reference.id, nullptr);
        child.setProperty ("name", reference.name, nullptr);
        child.setProperty ("visible", reference.visible, nullptr);
        child.setProperty ("colour", static_cast<int> (reference.colour.getARGB()), nullptr);
        child.setProperty ("dataMin", reference.dataMinFrequencyHz, nullptr);
        child.setProperty ("dataMax", reference.dataMaxFrequencyHz, nullptr);
        child.setProperty ("liveDb", vectorToVar (reference.liveDb), nullptr);
        child.setProperty ("rmsDb", vectorToVar (reference.rmsDb), nullptr);
        child.setProperty ("energyDb", vectorToVar (reference.energyDb), nullptr);
        child.setProperty ("peakHoldDb", vectorToVar (reference.peakHoldDb), nullptr);

        tree.addChild (child, -1, nullptr);
    }

    return tree;
}

void AnalyzerReferenceManager::restoreFromValueTree (
    const juce::ValueTree& tree)
{
    clear();

    if (!tree.isValid() || !tree.hasType (referencesTreeId))
        return;

    for (int i = 0; i < tree.getNumChildren(); ++i)
    {
        const auto child = tree.getChild (i);

        if (!child.hasType (referenceTreeId))
            continue;

        AnalyzerReferenceCurve reference;
        reference.id = child.getProperty ("id").toString();
        reference.name = child.getProperty ("name").toString();
        reference.visible = static_cast<bool> (child.getProperty ("visible", true));
        reference.colour = juce::Colour (
            static_cast<juce::uint32> (
                static_cast<int> (child.getProperty ("colour",
                    static_cast<int> (makeReferenceColour (i).getARGB())))));

        reference.dataMinFrequencyHz =
            static_cast<float> (child.getProperty ("dataMin",
                AnalyzerFrequencyRange::minimumHz));

        reference.dataMaxFrequencyHz =
            static_cast<float> (child.getProperty ("dataMax",
                AnalyzerFrequencyRange::maximumHz));

        if (reference.id.isEmpty())
            reference.id = juce::Uuid().toString();

        if (reference.name.isEmpty())
            reference.name = makeDefaultName (i + 1);

        if (!restoreVectorFromVar (child.getProperty ("liveDb"), reference.liveDb)
            || reference.liveDb.size() < 2)
        {
            continue;
        }

        restoreVectorFromVar (child.getProperty ("rmsDb"), reference.rmsDb);
        restoreVectorFromVar (child.getProperty ("energyDb"), reference.energyDb);
        restoreVectorFromVar (child.getProperty ("peakHoldDb"), reference.peakHoldDb);

        reference.dataMinFrequencyHz =
            juce::jlimit (AnalyzerFrequencyRange::minimumHz,
                          AnalyzerFrequencyRange::maximumHz - 1.0f,
                          reference.dataMinFrequencyHz);

        reference.dataMaxFrequencyHz =
            juce::jlimit (reference.dataMinFrequencyHz + 1.0f,
                          AnalyzerFrequencyRange::maximumHz,
                          reference.dataMaxFrequencyHz);

        references.push_back (std::move (reference));

        if (references.size() >= static_cast<size_t> (maxReferences))
            break;
    }

    activeReferenceIndex =
        static_cast<int> (tree.getProperty ("active", -1));

    clampActiveIndex();
}

juce::Colour AnalyzerReferenceManager::makeReferenceColour (int index) noexcept
{
    static constexpr std::array<juce::uint32, maxReferences> colours {{
        0xffffffff,
        0xffffd36a,
        0xff7df2a0,
        0xff67d8ff,
        0xffff8cc8,
        0xffb89cff,
        0xffff9b72,
        0xff9fe0d0
    }};

    return juce::Colour (colours[static_cast<size_t> (
        juce::jlimit (0, maxReferences - 1, index))]);
}

juce::var AnalyzerReferenceManager::vectorToVar (
    const std::vector<float>& values)
{
    juce::MemoryBlock block;
    juce::MemoryOutputStream stream (block, false);

    stream.writeInt (static_cast<int> (values.size()));

    if (!values.empty())
    {
        stream.write (values.data(),
                      values.size() * sizeof (float));
    }

    return juce::var (block);
}

bool AnalyzerReferenceManager::restoreVectorFromVar (
    const juce::var& value,
    std::vector<float>& destination)
{
    destination.clear();

    const auto* block = value.getBinaryData();

    if (block == nullptr || block->getSize() < sizeof (int))
        return false;

    juce::MemoryInputStream stream (*block, false);
    const auto count = stream.readInt();

    if (count < 0 || count > 8192)
        return false;

    destination.resize (static_cast<size_t> (count));

    if (count == 0)
        return true;

    const auto bytesToRead = static_cast<size_t> (count) * sizeof (float);

    if (stream.getNumBytesRemaining() < static_cast<juce::int64> (bytesToRead))
    {
        destination.clear();
        return false;
    }

    stream.read (destination.data(), static_cast<int> (bytesToRead));

    return true;
}

juce::String AnalyzerReferenceManager::makeDefaultName (int index)
{
    return juce::String ("Ref ") + juce::String (index);
}

void AnalyzerReferenceManager::clampActiveIndex() noexcept
{
    if (references.empty())
    {
        activeReferenceIndex = -1;
        return;
    }

    activeReferenceIndex = juce::jlimit (
        0,
        static_cast<int> (references.size()) - 1,
        activeReferenceIndex);
}
