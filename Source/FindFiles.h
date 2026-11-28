#pragma once
#include <JuceHeader.h>
#include "Arguments.h"
#include "Paths.h"
#include "Stdio.h"

class EntryFilter
{
public:
    static std::pair<juce::Result, std::unique_ptr<EntryFilter>> create (const Arguments& arguments);

    EntryFilter (jam::Array<Paths::EntryType> newEntryTypes,
                 jam::Array<juce::Range<juce::int64>> newSizeRanges,
                 std::optional<juce::int64> newChangedWithinMilliseconds,
                 std::optional<juce::int64> newChangedBeforeMilliseconds);

    bool isMatching (const Paths::ListedEntry& listedEntry) const;

private:
    jam::Array<Paths::EntryType> entryTypes;
    jam::Array<juce::Range<juce::int64>> sizeRanges;
    std::optional<juce::int64> changedWithinMilliseconds;
    std::optional<juce::int64> changedBeforeMilliseconds;
};

struct FindFiles
{
    static Stdio::Output run (const Arguments& arguments);

    static juce::Result getListOnlyFailure (const Arguments& arguments);
};
