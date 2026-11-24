#pragma once
#include <JuceHeader.h>
#include "Arguments.h"
#include "Pattern.h"

/** @brief Searches the bytes of one input for a pattern, with the settings of one command.
 *
 *  The settings are read one time from the arguments, when the Search is built. Every run then
 *  uses the same settings, so no run reads the arguments again.
 */
class Search
{
public:
    /** @brief Builds a search with the settings of one command.
     *
     *  @param arguments       The parsed arguments. Flags such as quiet, count, and invert are read here.
     *  @param newBeforeContext Number of lines to print before each match.
     *  @param newAfterContext  Number of lines to print after each match.
     *  @param newAfterLine     Matches are taken only on lines that are numbered after this line.
     *  @param newLineRange     One-based half-open range of lines that the search covers.
     */
    Search (const Arguments& arguments, int newBeforeContext, int newAfterContext, int newAfterLine, juce::Range<int> newLineRange);

    /** @brief Searches bytes that are already in memory.
     *
     *  A large input with no context and no early stop is searched in parallel chunks.
     *
     *  @param bytes                The bytes to search.
     *  @param displayPath          The path shown in the output.
     *  @param pattern              The compiled pattern.
     *  @param automaton            Matcher state that the caller owns.
     *  @param isExplicit           True when the input was named as a file on the command line.
     *  @param isShowingPathPrefix True to prefix the output lines with the display path.
     *
     *  @return Status result, the number of matching lines, and the output bytes.
     */
    std::tuple<juce::Result, int, std::string> run (std::string_view bytes,
                                                    std::string_view displayPath,
                                                    const Pattern& pattern,
                                                    jam::RegExpAutomaton& automaton,
                                                    bool isExplicit,
                                                    bool isShowingPathPrefix) const;

    /** @brief Opens a file and searches its bytes.
     *
     *  The Input of the file owns the bytes for the search. A file of up to Input::maxBufferedReadSize
     *  bytes is read into a buffer, and a larger file is mapped. A file that cannot be read gives the
     *  read failure as the status result. An empty file gives the empty-file outcome.
     *
     *  @param file                 The file to read.
     *  @param displayPath          The path shown in the output.
     *  @param pattern              The compiled pattern.
     *  @param automaton            Matcher state that the caller owns.
     *  @param isExplicit           True when the input was named as a file on the command line.
     *  @param isShowingPathPrefix  True to prefix the output lines with the display path.
     *
     *  @return Status result, the number of matching lines, and the output bytes.
     */
    std::tuple<juce::Result, int, std::string> runFile (const juce::File& file,
                                                        std::string_view displayPath,
                                                        const Pattern& pattern,
                                                        jam::RegExpAutomaton& automaton,
                                                        bool isExplicit,
                                                        bool isShowingPathPrefix) const;

    /** @brief Searches the process standard input in blocks of complete lines, as they arrive.
     *
     *  A match that ends the search early ends the read too, so an endless input can be searched.
     *
     *  @param pattern The compiled pattern.
     *
     *  @return Status result, the number of matching lines, the output bytes, and the number of
     *          bytes read from standard input.
     */
    std::tuple<juce::Result, int, std::string, size_t> runStream (const Pattern& pattern) const;

    /** @brief Reports whether the search needs no context lines.
     *
     *  @return True when neither before nor after context is requested.
     */
    bool isStreamSearch() const noexcept;

private:
    /** @brief Searches the lines of a byte range, and returns the bytes the result depends on.
     *
     *  @param bytes               The bytes to search.
     *  @param displayPath         The path shown in the output.
     *  @param pattern             The compiled pattern.
     *  @param automaton           Matcher state that the caller owns.
     *  @param isExplicit          True when the input was named as a file on the command line.
     *  @param isShowingPathPrefix True to prefix the output lines with the display path.
     *  @param firstLineNumber     The line number of the first byte of bytes.
     *
     *  @return Status result, the number of matching lines, the output bytes, and the number of
     *          bytes of the input that the result depends on. A NUL byte after that point does not
     *          make the input binary.
     */
    std::tuple<juce::Result, int, std::string, size_t> runLines (std::string_view bytes,
                                                                 std::string_view displayPath,
                                                                 const Pattern& pattern,
                                                                 jam::RegExpAutomaton& automaton,
                                                                 bool isExplicit,
                                                                 bool isShowingPathPrefix,
                                                                 int firstLineNumber) const;

    /** @brief Searches a large byte range in parallel chunks, and joins their output in order.
     *
     *  A NUL byte in any chunk makes the outcome binary, as getBinaryOutcome defines it, when the
     *  input is explicit or a count is requested, and also when the search fails or finds a match.
     *
     *  @param bytes               The bytes to search.
     *  @param displayPath         The path shown in the output.
     *  @param pattern             The compiled pattern.
     *  @param isExplicit          True when the input was named as a file on the command line.
     *  @param isShowingPathPrefix True to prefix the output lines with the display path.
     *
     *  @return Status result, the number of matching lines, and the output bytes.
     */
    std::tuple<juce::Result, int, std::string> runChunks (std::string_view bytes,
                                                          std::string_view displayPath,
                                                          const Pattern& pattern,
                                                          bool isExplicit,
                                                          bool isShowingPathPrefix) const;

    /** @brief Returns the outcome of an input with no bytes.
     *
     *  An explicit input with a count option prints a zero count, unless quiet or files-with-matches
     *  is set. Any other empty input prints nothing.
     *
     *  @param displayPath         The path shown in the output.
     *  @param isExplicit          True when the input was named as a file on the command line.
     *  @param isShowingPathPrefix True to prefix the count with the display path.
     *
     *  @return Status result, zero matches, and the output bytes.
     */
    std::tuple<juce::Result, int, std::string> getEmptyFileOutcome (std::string_view displayPath,
                                                                    bool isExplicit,
                                                                    bool isShowingPathPrefix) const;

    /** @brief Reports whether a byte range is large enough, and needs no context, to be searched in parallel.
     *
     *  @param size The size of the bytes in question.
     *
     *  @return True when the size reaches the chunked threshold and no option stops the search early
     *          or limits the lines or the context.
     */
    bool isChunkedSearch (size_t size) const noexcept;

    bool isQuiet { false };
    bool isFilesWithMatches { false };
    bool isCount { false };
    bool isOnlyMatching { false };
    bool isInvert { false };
    bool isFirst { false };
    bool isShowingLineNumber { false };
    bool isShowingHash { false };
    int beforeContext { 0 };
    int afterContext { 0 };
    int afterLine { 0 };
    juce::Range<int> lineRange;
};
