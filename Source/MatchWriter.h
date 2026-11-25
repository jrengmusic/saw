#pragma once
#include "Pattern.h"

/** @brief Formats the output of one search: matching lines, context lines, and group separators.
 *
 *  The writer tracks the end of the last printed line, so each line is printed one time, even
 *  when the context of two matches overlaps.
 */
class MatchWriter
{
public:
    /** @brief Creates a writer over the bytes of one input.
     *
     *  @param newBytes              The bytes of the input. The writer holds a view on them.
     *  @param newDisplayPath       The path shown before each line when path prefixes are on.
     *  @param newBeforeContext     Number of lines printed before each match.
     *  @param newAfterContext      Number of lines printed after each match.
     *  @param newIsOnlyMatching    True to print only the matched text of each match.
     *  @param newIsShowingPathPrefix True to prefix each line with the display path.
     *  @param newIsShowingLineNumber True to prefix each line with its line number.
     *  @param newIsShowingHash     True to prefix each line with its line number and its hash.
     *  @param newFirstLineNumber   The line number of the first line of newBytes.
     */
    MatchWriter (std::string_view newBytes,
                 std::string_view newDisplayPath,
                 int newBeforeContext,
                 int newAfterContext,
                 bool newIsOnlyMatching,
                 bool newIsShowingPathPrefix,
                 bool newIsShowingLineNumber,
                 bool newIsShowingHash,
                 int newFirstLineNumber);

    /** @brief Moves the line count forward to a line start.
     *
     *  The count adds the newlines between the last counted position and lineStart. The count
     *  never moves back, so lineStart must not be before the last counted position.
     *
     *  @param lineStart Byte offset of a line start, at or after the last counted position.
     */
    void setLineNumber (size_t lineStart);

    /** @brief Returns the line number of the last counted position.
     *
     *  @return One-based line number, as counted by setLineNumber.
     */
    int getLineNumber() const noexcept;

    /** @brief Appends one matching line, with its context lines.
     *
     *  Context lines before the match are printed when they follow the last printed line. A
     *  separator line is printed when the context does not join the previous group.
     *
     *  @param pattern     The pattern that matched. Used to print the matched text with only-matching.
     *  @param lineStart   Byte offset of the start of the matching line.
     *  @param newlinePos  Byte offset of the newline that ends the matching line, or bytes.size().
     *
     *  @return Status result. It fails when the pattern fails on a line.
     */
    juce::Result addMatch (const Pattern& pattern, size_t lineStart, size_t newlinePos);

    /** @brief Appends the trailing context and returns the output.
     *
     *  The output buffer is moved out of the writer, so the call takes the output in one step.
     *
     *  @return The output text.
     */
    std::string removeOutput();

    /** @brief Returns the end of the last printed line, with its context lines.
     *
     *  @return Byte offset just after the last printed line. Zero when nothing is printed.
     */
    size_t getPrintedSize() const noexcept;

private:
    /** @brief Appends the display path, when shown, and the line prefix: the line number, the hash, or both.
     *
     *  @param lineNumber The one-based number of the line.
     *  @param line       The line bytes, without the line ending. The hash is computed from them.
     *  @param separator  The character written after the path and the prefix.
     */
    void appendLinePrefix (int lineNumber, std::string_view line, char separator);

    /** @brief Appends one line with its prefix, and its newline.
     *
     *  @param lineNumber The one-based number of the line.
     *  @param line       The line bytes, without the line ending.
     *  @param separator  The character that separates the prefix from the line: a colon for a match, a dash for context.
     */
    void appendFormattedLine (int lineNumber, std::string_view line, char separator);

    /** @brief Appends each non-empty match of the line on its own line, for only-matching output.
     *
     *  @param pattern    The pattern that matched the line.
     *  @param lineNumber The one-based number of the line.
     *  @param line       The line bytes, without the line ending.
     *
     *  @return Status result. It fails when the pattern fails on the line.
     */
    juce::Result appendOnlyMatches (const Pattern& pattern, int lineNumber, std::string_view line);

    /** @brief Appends the context line that starts at a byte offset.
     *
     *  @param lineStart  Byte offset of the first byte of the context line.
     *  @param lineNumber The one-based number of the context line.
     *  @param separator  The character that separates the prefix from the line.
     *
     *  @return Byte offset just after the context line, or the size of the bytes at the end.
     */
    size_t appendContextLine (size_t lineStart, int lineNumber, char separator);

    /** @brief Appends the context lines that follow the last printed line, up to afterContext lines and limit.
     *
     *  @param limit Byte offset that the context lines do not pass.
     */
    void appendTrailingContext (size_t limit);

    /** @brief Reports whether the output carries line numbers, which the context counting needs.
     *
     *  @return True when a line number or a hash is shown, or when context lines are requested.
     */
    bool isNumbered() const noexcept;

    /** @brief View of the bytes of the input that the writer formats. */
    std::string_view bytes;

    /** @brief The display path that prefixes each line when path prefixes are on. */
    std::string_view displayPath;

    /** @brief Number of lines printed before each match. */
    int beforeContext;

    /** @brief Number of lines printed after each match. */
    int afterContext;

    /** @brief True when only the matched text of each match is printed. */
    bool isOnlyMatching;

    /** @brief True when each line is prefixed with the display path. */
    bool isShowingPathPrefix;

    /** @brief True when each line is prefixed with its line number. */
    bool isShowingLineNumber;

    /** @brief True when each line is prefixed with its hash. */
    bool isShowingHash;

    /** @brief The formatted output, moved out by removeOutput. */
    std::string output;

    /** @brief Byte offset just after the last printed line. Lines before it are never printed again. */
    size_t printedUpTo { 0 };

    /** @brief One-based number of the line that follows the last printed line. */
    int printedLineNumber;

    /** @brief Byte offset up to which the newlines are counted by setLineNumber. */
    size_t countedUpTo { 0 };

    /** @brief One-based line number at countedUpTo. */
    int countedLineNumber;
};
