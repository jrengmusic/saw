#pragma once
#include <JuceHeader.h>

/** @brief Parses line range texts, and selects, joins, numbers, and marks the lines that a page shows. */
class PageRanges
{
public:
    /** @brief Returns the number of lines of bytes. A last line without a line feed counts as a line.
     *
     *  @param bytes The bytes to count.
     *
     *  @return The line count. Zero for empty bytes.
     */
    static int getLineCount (std::string_view bytes) noexcept;

    /** @brief Parses one range text into a one-based half-open line range, with the line count clamping it.
     *
     *  The text is a line, or a start and an end separated by a colon, with an optional context count after a second colon.
     *  Relative and from-end forms are resolved against the line count.
     *
     *  @param rangeText The range text.
     *  @param lineCount The number of lines of the input.
     *
     *  @return Status result paired with the range. A text that does not parse gives a failure.
     */
    static std::pair<juce::Result, juce::Range<int>> getLineRange (std::string_view rangeText, int lineCount);

    /** @brief Parses each range text, and returns the union of the ranges.
     *
     *  With no range text, the whole input is one range.
     *
     *  @param rangeTexts The range texts, in the order given.
     *  @param lineCount  The number of lines of the input.
     *
     *  @return Status result paired with the merged ranges. The first failing text gives the failure, with no ranges.
     */
    static std::pair<juce::Result, jam::Array<juce::Range<int>>> getRanges (juce::Span<const std::string_view> rangeTexts, int lineCount);

    /** @brief Sorts the ranges by start, drops the empty ones, and merges the ranges that touch or overlap.
     *
     *  @param lineRanges The ranges to merge.
     *
     *  @return The merged ranges, in ascending order.
     */
    static jam::Array<juce::Range<int>> getUnion (jam::Array<juce::Range<int>> lineRanges);

    /** @brief Tests whether a range starts after the end of the previous range.
     *
     *  @param previousRange The earlier range.
     *  @param nextRange     The later range.
     *
     *  @return True when the next range starts after the previous range ends.
     */
    static bool isGapBetween (juce::Range<int> previousRange, juce::Range<int> nextRange) noexcept;

    /** @brief Returns the bytes of a line range, with the line endings.
     *
     *  @param bytes     The bytes of the input.
     *  @param lineRange The one-based half-open line range.
     *
     *  @return A view of the bytes of the lines.
     */
    static std::string_view getRangeBytes (std::string_view bytes, juce::Range<int> lineRange);

    /** @brief Returns the bytes of several line ranges, joined in the order given.
     *
     *  @param bytes      The bytes of the input.
     *  @param lineRanges The line ranges, in the order to join.
     *
     *  @return The joined bytes.
     */
    static std::string getJoinedBytes (std::string_view bytes, const jam::Array<juce::Range<int>>& lineRanges);

    /** @brief Returns the number shown for each line of a text: the original number, or the count of non-blank lines when numbering skips blank lines.
     *
     *  @param text                 The text of the shown lines.
     *  @param originalNumbers      The original line numbers, used when numbering does not skip blank lines.
     *  @param isNumberingNonblank  True to number only the non-blank lines.
     *
     *  @return One number for each line of the text. A blank line has zero when numbering skips blank lines.
     */
    static jam::Array<int> getShownNumbers (std::string_view text, const jam::Array<int>& originalNumbers, bool isNumberingNonblank);

    /** @brief Returns the change mark of each shown line, from the change marks of the document.
     *
     *  @param documentChanges The change mark of each line of the document.
     *  @param originalNumbers The original line numbers of the shown lines.
     *  @param lineCount       The number of shown lines.
     *
     *  @return One change mark for each shown line, or an empty array when the document has no changes.
     */
    static jam::Array<int> getShownChanges (const jam::Array<int>& documentChanges, const jam::Array<int>& originalNumbers, int lineCount);

    /** @brief Returns the ranges of the lines around each changed line, within the context count.
     *
     *  @param shownChanges The change mark of each shown line.
     *  @param diffContext  The number of lines kept on each side of a change.
     *  @param lineCount    The number of shown lines.
     *
     *  @return The merged ranges, in ascending order.
     */
    static jam::Array<juce::Range<int>> getChangedRanges (const jam::Array<int>& shownChanges, int diffContext, int lineCount);

private:
    /** @brief Returns the number of each line index, from the original numbers, or the index plus one past their end.
     *
     *  @param originalNumbers The original line numbers.
     *  @param lineCount       The number of lines.
     *
     *  @return The number of each line.
     */
    static jam::Array<int> getRealNumbers (const jam::Array<int>& originalNumbers, int lineCount);

    /** @brief Returns the number of each line of a text, counting only the non-blank lines. A blank line has zero.
     *
     *  @param text      The text.
     *  @param lineCount The number of lines to number.
     *
     *  @return One number for each line.
     */
    static jam::Array<int> getNonblankNumbers (std::string_view text, int lineCount);

    /** @brief Returns the original number of a line index, or the index plus one when the index is past the original numbers.
     *
     *  @param originalNumbers The original line numbers.
     *  @param lineIndex       The zero-based index of the line.
     *
     *  @return The one-based line number.
     */
    static int getNumberOfLine (const jam::Array<int>& originalNumbers, int lineIndex);

    /** @brief Returns the change mark of a one-based line number, or none when the line is outside the document changes.
     *
     *  @param documentChanges The change mark of each line of the document.
     *  @param lineNumber      The one-based line number.
     *
     *  @return The change mark of the line.
     */
    static int getLineChange (const jam::Array<int>& documentChanges, int lineNumber);

    /** @brief Returns the one-based half-open range of the lines within the context count of a line.
     *
     *  @param lineNumber  The one-based number of the changed line.
     *  @param diffContext The number of lines kept on each side.
     *  @param lineCount   The number of lines.
     *
     *  @return The range, clamped to the lines.
     */
    static juce::Range<int> getContextRange (int lineNumber, int diffContext, int lineCount);
};
