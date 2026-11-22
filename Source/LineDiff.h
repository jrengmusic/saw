#pragma once
#include <JuceHeader.h>
#include "TextDocument.h"

/** @brief Stores line-level edits, computes Myers diffs, and renders planned text or unified diffs.
 */
class LineDiff
{
public:
    /** @brief Contiguous replacement of zero-based original lines with zero or more new lines.
     */
    struct Change
    {
        /** @brief Zero-based starting line index in the original document.
         */
        int start { 0 };

        /** @brief Number of original lines removed or replaced (`0` for insertions).
         */
        int length { 0 };

        /** @brief Replacement lines inserted at @ref start (empty for deletions).
         */
        jam::Strings insertedLines;
    };

    /** @brief Unchanged context lines emitted before and after each unified diff hunk.
     */
    static constexpr int contextLineCount { 3 };

    /** @brief Stores prebuilt line-level changes ordered by non-decreasing @ref Change::start.
     *
     *  @param explicitChanges Ordered list of changes to apply.
     */
    explicit LineDiff (jam::Array<Change> explicitChanges);

    /** @brief Computes the shortest line-level edit script from @p original to @p target using Myers diff.
     *
     *  @param original Baseline document before edits.
     *  @param target   Target document after edits.
     */
    LineDiff (const TextDocument& original, const TextDocument& target);

    /** @brief Applies @ref changes onto @p original while preserving its line-ending convention.
     *
     *  @param original Document to transform.
     *
     *  @return Materialized full text after applying all changes.
     */
    std::string getPlannedText (const TextDocument& original) const;

    /** @brief Renders unified diff hunks between @p original and @p planned.
     *
     *  @param original Document before changes.
     *  @param planned  Document constructed from getPlannedText().
     *
     *  @return Unified diff string, or empty when both documents match including trailing newline.
     */
    std::string getUnifiedDiff (const TextDocument& original,
                                const TextDocument& planned) const;

    /** @brief Splits replacement payload text into lines, stripping one trailing line ending.
     *
     *  @param text Payload text to split.
     *
     *  @return Array of line strings without line-ending bytes.
     */
    static jam::Strings getLines (std::string_view text);

    /** @brief Splits text on `'\\n'` while preserving a trailing empty element after an ending newline.
     *
     *  @param text Text to split without dropping a final empty line.
     *
     *  @return Array of line strings without `'\\r'` or `'\\n'`.
     */
    static jam::Strings getLinesPreservingEmpty (std::string_view text);

    /** @brief Ordered list of line-level changes.
     */
    jam::Array<Change> changes;

private:
    static std::string getHunkRangeText (int zeroBasedStart, int lineCount);
};
