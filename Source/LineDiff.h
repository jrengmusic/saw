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
        std::vector<std::string> insertedLines;
    };

    /** @brief Creates a diff with no changes. */
    LineDiff() = default;

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

    /** @brief Renders unified diff hunks between @p original and @p plannedBytes.
     *
     *  Changes that lie within 2 * 3 unchanged lines of each other share one hunk. Each hunk
     *  has up to three unchanged context lines before and after its changes, limited by the document bounds.
     *
     *  @param original     Document before changes.
     *  @param plannedBytes Bytes from getPlannedText().
     *
     *  @return Unified diff string, or empty when the diff holds no changes.
     */
    std::string getUnifiedDiff (const TextDocument& original,
                                std::string_view plannedBytes) const;

    /** @brief Splits replacement payload text into lines, stripping one trailing line ending.
     *
     *  @param text Payload text to split.
     *
     *  @return Array of line strings without line-ending bytes.
     */
    static std::vector<std::string> getLines (std::string_view text);

    /** @brief Splits text on `'\\n'` while preserving a trailing empty element after an ending newline.
     *
     *  A carriage return before a newline is removed. The last element is not followed by a newline,
     *  so it keeps its bytes as they are.
     *
     *  @param text Text to split without dropping a final empty line.
     *
     *  @return Array of line strings. Each one that ends in a newline has no `'\\r'` or `'\\n'`.
     */
    static std::vector<std::string> getLinesPreservingEmpty (std::string_view text);

private:
    /** @brief Ordered list of line-level changes.
     */
    jam::Array<Change> changes;

    /** @brief Returns the changes for rendering, with the last line of an unterminated document included in the last change.
     *
     *  A change that adds lines after a last line with no newline also changes that line, because
     *  the line gains its newline. The last change is therefore rewritten to replace that line,
     *  unless the change before it already does. The caller uses this only when the last line
     *  has no newline.
     *
     *  @param original The document before the changes.
     *
     *  @return The rewritten changes, in order.
     */
    jam::Array<Change> getDiffChanges (const TextDocument& original) const;

    /** @brief Formats the line range of a unified diff hunk header.
     *
     *  A count of 1 is written without the count. A count of 0 writes the start as given, with `,0`.
     *
     *  @param zeroBasedStart The zero-based index of the first line of the range.
     *  @param lineCount      The number of lines in the range.
     *
     *  @return The range text, such as `5`, `5,3`, or `4,0`.
     */
    static std::string getHunkRangeText (int zeroBasedStart, int lineCount);
};
