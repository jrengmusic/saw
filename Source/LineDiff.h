#pragma once
#include <JuceHeader.h>
#include "TextDocument.h"

/** @brief Stores line-level edits, computes Myers diffs, and renders planned text or unified diffs.
 *
 *  A diff is built in one of two ways. The callable constructor builds the planned bytes while it adds the
 *  changes. The Myers constructor computes the changes only, and its inserted lines index the bytes of the target.
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

        /** @brief Range of indexes into insertedLines, one index for each inserted line.
         *
         *  An empty range is a deletion.
         */
        juce::Range<int> insertedRange;
    };

    /** @brief Creates a diff with no changes. */
    LineDiff() = default;

    /** @brief Builds the changes and the planned bytes in one pass, from a callable that adds the changes.
     *
     *  The callable receives addChange, and calls it as addChange (start, length, insertedText) for each change,
     *  in ascending order of start. The inserted text replaces the length lines from start. A nullopt text is a
     *  deletion. Each inserted line takes the line ending of the original line that it replaces, or the line ending
     *  of the document when no original line is replaced.
     *
     *  @tparam Function   Callable that accepts the addChange callable.
     *  @param original    The document before the changes.
     *  @param addChanges  The callable that adds the changes.
     *  @param isVerbatim   True to write the inserted text through its own line endings, instead of the endings of the original lines.
     */
    template <typename Function>
    LineDiff (const TextDocument& original, const Function& addChanges, bool isVerbatim = false)
    {
        std::optional<std::string_view> lastEnding;
        const auto addChange { [this, &original, &lastEnding, isVerbatim] (int start, int length, std::optional<std::string_view> insertedText)
        {
            lastEnding = appendChange (original, start, length, insertedText, lastEnding, isVerbatim);
        } };

        addChanges (addChange);
        setPlannedEnd (original, lastEnding, isVerbatim);
    }

    /** @brief Computes the shortest line-level edit script from @p original to @p target using Myers diff.
     *
     *  The inserted lines are offsets into the bytes of @p target, so the bytes of @p target must not change
     *  while this diff is used. No planned bytes are built, so getPlannedBytes is empty.
     *
     *  @param original Baseline document before edits.
     *  @param target   Target document after edits.
     */
    LineDiff (const TextDocument& original, const TextDocument& target);

    /** @brief Returns the planned bytes that the changes produce.
     *
     *  The view is empty for the Myers constructor, because its inserted lines index the bytes of the target document.
     *
     *  @return The planned bytes. The view is valid while this diff lives.
     */
    std::string_view getPlannedBytes() const noexcept;

    /** @brief Returns one mark for each planned line, which shows where the changes fall.
     *
     *  A removal marks the planned line above it, or the first line when none is above it. An insertion marks its lines as added,
     *  and a replacement marks its lines as modified.
     *
     *  @param numLines The number of lines of the planned document.
     *
     *  @return One mark for each planned line.
     */
    jam::Array<int> getLineChanges (int numLines) const;

    /** @brief Appends the unified diff of the changes, with one hunk for each group of changes that lie close together.
     *
     *  Nothing is appended when there are no changes. The header names the original and the planned file. A file that does not exist
     *  is named by the null device. Changes whose gap is at most twice the context line count share one hunk.
     *
     *  @param output             The text that receives the unified diff.
     *  @param original           The document before the changes.
     *  @param plannedBytes       The planned bytes that the inserted lines index.
     *  @param isOriginalExisting True when the original file exists.
     *  @param isPlannedExisting  True when the planned file exists.
     */
    void appendUnifiedDiff (std::string& output,
                            const TextDocument& original,
                            std::string_view plannedBytes,
                            bool isOriginalExisting = true,
                            bool isPlannedExisting = true) const;

    /** @brief Builds the change shape of a deletion, for the addChange callable of the callable constructor.
     *
     *  The one-based line range is converted to the zero-based start here, so that every caller uses the same arithmetic.
     *
     *  @param lineRange One-based half-open range of the original lines to delete.
     *
     *  @return The zero-based start, the number of lines, and nullopt for the text, which marks a deletion.
     */
    static std::tuple<int, int, std::optional<std::string_view>> getDeletion (juce::Range<int> lineRange);

    /** @brief Builds the change shape of a replacement, for the addChange callable of the callable constructor.
     *
     *  The one-based line range is converted to the zero-based start here. The text is used as it is.
     *  A caller that must drop a trailing line feed passes the text through getTextWithoutSingleTrailingNewline first.
     *
     *  @param lineRange One-based half-open range of the original lines to replace.
     *  @param text      The replacement text.
     *
     *  @return The zero-based start, the number of replaced lines, and the text as given.
     */
    static std::tuple<int, int, std::optional<std::string_view>> getReplacement (juce::Range<int> lineRange, std::string_view text);

    /** @brief Builds the change shape of an insertion, for the addChange callable of the callable constructor.
     *
     *  @param start The zero-based index of the original line before which the text is inserted.
     *  @param text  The inserted text, used as it is.
     *
     *  @return The start, zero lines replaced, and the text as given.
     */
    static std::tuple<int, int, std::optional<std::string_view>> getInsertion (int start, std::string_view text);

    /** @brief Removes one trailing line feed from a payload text, and the carriage returns beside it.
     *
     *  Each inserted line takes a line ending from the document. A payload that ends in a line feed would
     *  add one empty line, so the trailing line feed is removed. The carriage return before that line feed
     *  is removed too, and then one more trailing carriage return, when present.
     *
     *  @param text The payload text.
     *
     *  @return The text without its trailing line feed and carriage returns.
     */
    static std::string_view getTextWithoutSingleTrailingNewline (std::string_view text);

    /** @brief Calls a function with the change shape of each change, in order.
     *
     *  @tparam Function Callable accepting `(int start, int length, juce::Range<int> insertedRange)`.
     *  @param function  The callable for each change.
     */
    template <typename Function>
    void applyToChanges (const Function& function) const
    {
        for (int index { 0 }; index < changes.size(); ++index)
        {
            const auto& change { changes.at (index) };
            function (change.start, change.length, change.insertedRange);
        }
    }

private:
    /** @brief Appends the original lines before a change, then the inserted lines of the change.
     *
     *  Each inserted line takes the line ending of the original line that it replaces, or the document line ending
     *  when it replaces none. So the line endings of the original are kept. The offset and the size of each inserted
     *  line are recorded in insertedLines.
     *
     *  @param original     The document before the changes.
     *  @param start        The first original line that the change replaces.
     *  @param length       The number of original lines that the change replaces.
     *  @param insertedText The inserted text, or nullopt for a deletion.
     *  @param lastEnding   The line ending of the last piece that was appended, or nullopt before the first piece.
     *  @param isVerbatim   True to write the inserted text through its own line endings, instead of the endings of the original lines.
     *
     *  @return The line ending of the last piece that was appended. An empty ending is an incomplete last line.
     */
    std::optional<std::string_view> appendChange (const TextDocument& original,
                                                  int start,
                                                  int length,
                                                  std::optional<std::string_view> insertedText,
                                                  std::optional<std::string_view> lastEnding,
                                                  bool isVerbatim);

    /** @brief Appends the inserted text, with each line given the line ending of the original line that it replaces.
     *
     *  The lines beyond the replaced lines take the line ending of the document. Each inserted line is recorded in insertedLines.
     *
     *  @param original   The document before the changes.
     *  @param start      The first original line that the change replaces.
     *  @param length     The number of original lines that the change replaces.
     *  @param text       The inserted text.
     *  @param lastEnding The line ending of the last piece that was appended, or nullopt before the first piece.
     *
     *  @return The line ending of the last piece that was appended.
     */
    std::optional<std::string_view> appendInsertedText (const TextDocument& original,
                                                        int start,
                                                        int length,
                                                        std::string_view text,
                                                        std::optional<std::string_view> lastEnding);

    /** @brief Appends the text as it is, with the line ending that each of its lines carries.
     *
     *  @param original   The document before the changes.
     *  @param text       The inserted text.
     *  @param lastEnding The line ending of the last piece that was appended, or nullopt before the first piece.
     *
     *  @return The line ending of the last piece that was appended.
     */
    std::optional<std::string_view> appendVerbatimText (const TextDocument& original,
                                                        std::string_view text,
                                                        std::optional<std::string_view> lastEnding);

    /** @brief Appends the original lines from startLine up to endLine, which the changes do not touch.
     *
     *  The run is written with the line ending of its last line. An empty ending, which means an incomplete
     *  last line, is returned, so that the next piece can write the document line ending first.
     *
     *  @param original   The document before the changes.
     *  @param startLine  The zero-based index of the first line of the run.
     *  @param endLine    The zero-based index that ends the run, exclusive.
     *  @param lastEnding The line ending of the last piece that was appended, or nullopt before the first piece.
     *
     *  @return The line ending of the last line of the run, or lastEnding when the run is empty.
     */
    std::optional<std::string_view> appendOriginalRun (const TextDocument& original,
                                                       int startLine,
                                                       int endLine,
                                                       std::optional<std::string_view> lastEnding);

    /** @brief Appends a piece of text and its line ending to the planned bytes.
     *
     *  When the previous piece had an empty line ending, the document line ending is written first.
     *  That case is an incomplete last line, which needs a line ending before more text can follow it.
     *
     *  @param original   The document before the changes. Its line ending is the default.
     *  @param bytes      The text of the piece, without its line ending.
     *  @param ending     The line ending of the piece. It may be empty.
     *  @param lastEnding The line ending of the previous piece, or nullopt before the first piece.
     */
    void appendWithEnding (const TextDocument& original,
                           std::string_view bytes,
                           std::string_view ending,
                           std::optional<std::string_view> lastEnding);

    /** @brief Sets the end of the planned bytes after the last change, and completes an incomplete last line.
     *
     *  The original lines after the last change are appended. The planned bytes end with a line ending only when
     *  the original ends with one, when the last change deletes the final line, or when the document is empty.
     *  Otherwise the line ending that was appended last is removed. An insertion after an incomplete last line
     *  takes that line into the change as a replaced line, because the line gets a line ending in the planned bytes.
     *
     *  @param original   The document before the changes.
     *  @param lastEnding The line ending of the last piece that was appended.
     *  @param isVerbatim The mode that the changes were built in, as given to the callable constructor.
     */
    void setPlannedEnd (const TextDocument& original, std::optional<std::string_view> lastEnding, bool isVerbatim);

    /** @brief Appends the body of one hunk: the context lines between the changes, and the changes themselves.
     *
     *  @param output               The text that receives the body.
     *  @param original             The document before the changes.
     *  @param plannedBytes         The bytes that the inserted lines index.
     *  @param incompletePlannedLine The one-based planned line that has no line ending, or 0 when the planned bytes end with one.
     *  @param hunkRange            Half-open range of indexes into changes that the hunk holds.
     *  @param oldStart             The zero-based index of the first original line of the hunk.
     *  @param oldEnd               The zero-based index that ends the original lines of the hunk, exclusive.
     *  @param newStart             The zero-based index of the first planned line of the hunk.
     */
    void appendHunkBody (std::string& output,
                         const TextDocument& original,
                         std::string_view plannedBytes,
                         int incompletePlannedLine,
                         juce::Range<int> hunkRange,
                         int oldStart,
                         int oldEnd,
                         int newStart) const;

    /** @brief Appends the removed lines and the inserted lines of one change.
     *
     *  @param output                The text that receives the change.
     *  @param original              The document before the changes.
     *  @param plannedBytes          The bytes that the inserted lines index.
     *  @param incompletePlannedLine The one-based planned line that has no line ending, or 0 when the planned bytes end with one.
     *  @param change                The change to append.
     *  @param originalLine          The zero-based index of the first original line of the change.
     *  @param plannedLine           The zero-based index of the first planned line of the change.
     */
    void appendHunkChange (std::string& output,
                           const TextDocument& original,
                           std::string_view plannedBytes,
                           int incompletePlannedLine,
                           const Change& change,
                           int originalLine,
                           int plannedLine) const;

    /** @brief The planned bytes. The callable constructor builds them, and the Myers constructor leaves them empty. */
    std::string plannedText;

    /** @brief Ordered list of line-level changes.
     */
    jam::Array<Change> changes;

    /** @brief The offset and the size of each inserted line, in the bytes that appendUnifiedDiff receives.
     *
     *  A change refers to its lines through its insertedRange. The offset indexes plannedText for the callable
     *  constructor, and the bytes of the target document for the Myers constructor.
     */
    jam::Array<std::pair<size_t, size_t>> insertedLines;

    /** @brief Appends the line range of a unified diff hunk header.
     *
     *  A count of 1 is written without the count. A count of 0 writes the start as given, with `,0`.
     *
     *  @param output         The text that receives the range.
     *  @param zeroBasedStart The zero-based index of the first line of the range.
     *  @param lineCount      The number of lines in the range.
     */
    static void appendHunkRange (std::string& output, int zeroBasedStart, int lineCount);

    /** @brief Marks the planned line above a removal, or the first line when no line is above it.
     *
     *  @param lineChanges   The line marks that receive the removal.
     *  @param plannedBefore The planned index of the line that follows the removal.
     */
    static void setRemovedMark (jam::Array<int>& lineChanges, int plannedBefore);

    /** @brief Marks the planned lines of an insertion or a replacement, as added or as modified.
     *
     *  @param lineChanges    The line marks that receive the insertion.
     *  @param plannedBefore  The planned index of the first inserted line.
     *  @param length         The number of original lines that the change replaces.
     *  @param insertedLength The number of inserted lines.
     */
    static void setInsertedMarks (jam::Array<int>& lineChanges, int plannedBefore, int length, int insertedLength);

    /** @brief Marks the planned lines of one change: a removal when no line is inserted, otherwise an insertion or a replacement.
     *
     *  @param lineChanges   The line marks that receive the change.
     *  @param plannedBefore The planned index of the first line of the change.
     *  @param length        The number of original lines that the change replaces.
     *  @param insertedRange The range of inserted lines of the change.
     */
    static void setChangeMarks (jam::Array<int>& lineChanges, int plannedBefore, int length, juce::Range<int> insertedRange);
};
