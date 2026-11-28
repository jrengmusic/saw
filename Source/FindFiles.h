#pragma once
#include <JuceHeader.h>
#include "Arguments.h"
#include "Paths.h"
#include "Stdio.h"

/** @brief Predicate that filters directory entries by entry type, byte size, and modification time.
 */
class EntryFilter
{
public:
    /** @brief Parses the file-list filter options from arguments and builds an EntryFilter.
     *
     *  @param arguments The parsed arguments of a `find --files` command.
     *
     *  @return Ok paired with the filter, or a parse failure paired with nullptr.
     */
    static std::pair<juce::Result, std::unique_ptr<EntryFilter>> create (const Arguments& arguments);

    /** @brief Constructs an EntryFilter from parsed type, size, and time constraints.
     *
     *  @param newEntryTypes                Allowed entry types.
     *  @param newSizeRanges                Inclusive byte-size ranges a file must satisfy.
     *  @param newChangedWithinMilliseconds Lower modification-time cutoff in Unix milliseconds, if set.
     *  @param newChangedBeforeMilliseconds Upper modification-time cutoff in Unix milliseconds, if set.
     */
    EntryFilter (jam::Array<Paths::EntryType> newEntryTypes,
                 jam::Array<juce::Range<juce::int64>> newSizeRanges,
                 std::optional<juce::int64> newChangedWithinMilliseconds,
                 std::optional<juce::int64> newChangedBeforeMilliseconds);

    /** @brief Checks whether a listed entry satisfies every active constraint.
     *
     *  @param listedEntry The entry slot and its metadata.
     *
     *  @return True when the entry matches all constraints.
     */
    bool isMatching (const Paths::ListedEntry& listedEntry) const;

    /** @brief Tells whether the filter requests directories or symbolic links in addition to regular files.
     *
     *  @return True when any allowed entry type is not a regular file.
     */
    bool isIncludingNonFiles() const noexcept;

    /** @brief Tells whether any entry-level filter pass is needed after the directory walk.
     *
     *  @return True when non-file types, size ranges, or modification-time cutoffs are active.
     */
    bool isFiltering() const noexcept;

private:
    /** @brief Allowed entry types. */
    jam::Array<Paths::EntryType> entryTypes;
    /** @brief Byte-size ranges a file must satisfy. */
    jam::Array<juce::Range<juce::int64>> sizeRanges;
    /** @brief Lower modification-time cutoff in Unix milliseconds. */
    std::optional<juce::int64> changedWithinMilliseconds;
    /** @brief Upper modification-time cutoff in Unix milliseconds. */
    std::optional<juce::int64> changedBeforeMilliseconds;
};

/** @brief Implements the `saw find --files` file-listing mode.
 */
struct FindFiles
{
    /** @brief Walks roots, applies filters and sort order, and formats plain or JSON lines.
     *
     *  @param arguments The parsed arguments of `find --files`.
     *
     *  @return The exit code, the formatted listing documents, and any failures.
     */
    static Stdio::Output run (const Arguments& arguments);

    /** @brief Rejects file-list-only flags when `--files` is not set.
     *
     *  @param arguments The parsed arguments of `find`.
     *
     *  @return Ok when no list-only flag is present, otherwise a failure naming the first flag.
     */
    static juce::Result getListOnlyFailure (const Arguments& arguments);
};
