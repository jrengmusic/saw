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
     *  @return The output bytes.
     */
    std::string getOutput();

    /** @brief Returns the end of the last printed line, with its context lines.
     *
     *  @return Byte offset just after the last printed line. Zero when nothing is printed.
     */
    size_t getPrintedSize() const noexcept;

private:
    void appendLinePrefix (int lineNumber, std::string_view line, char separator);
    void appendFormattedLine (int lineNumber, std::string_view line, char separator);
    juce::Result appendOnlyMatches (const Pattern& pattern, int lineNumber, std::string_view line);
    size_t appendContextLine (size_t lineStart, int lineNumber, char separator);
    void appendTrailingContext (size_t limit);
    bool isNumbered() const noexcept;

    std::string_view bytes;
    std::string_view displayPath;
    int beforeContext;
    int afterContext;
    bool isOnlyMatching;
    bool isShowingPathPrefix;
    bool isShowingLineNumber;
    bool isShowingHash;
    std::string output;
    size_t printedUpTo { 0 };
    int printedLineNumber;
    size_t countedUpTo { 0 };
    int countedLineNumber;
};
