#pragma once
#include <JuceHeader.h>

/** @brief Compiles literal or regex patterns and extracts SIMD prefilter literals.
 */
class Pattern
{
    struct Access {};

public:
    /** @brief Capture groups of one match. Group 0 is the whole match. */
    using Captures = jam::RegExp::Captures;

    /** @brief Creates a compiled pattern. Use create(), which validates the texts first.
     *
     *  @param newRegExp        The compiled expression, or null for a literal pattern.
     *  @param newProgram       The program that runs the expression, or null when none is built.
     *  @param newLiteralSize   The byte size of the text of a single literal pattern. Zero for every other pattern.
     *  @param newLiteralSet    The prefilter literals, or null when none applies.
     *  @param newPatternText   The pattern texts joined with `|`, kept for diagnostics.
     *  @param newIsCandidateMatch     True for a pattern that is not a regex and whose text holds no carriage return.
     *  @param newIsExactLine   True when the pattern must match a whole line.
     *  @param newIsWord        True when the pattern must match whole words.
     */
    Pattern (Access,
             std::unique_ptr<jam::RegExp> newRegExp,
             std::unique_ptr<jam::RegExpProgram> newProgram,
             size_t newLiteralSize,
             std::unique_ptr<jam::LiteralSet> newLiteralSet,
             std::string newPatternText,
             bool newIsCandidateMatch,
             bool newIsExactLine,
             bool newIsWord);

    /** @brief Compiles one or more pattern texts into a Pattern.
     *
     *  A pattern is a literal text, or a regex when isRegex is true. Several texts give an alternation.
     *  A pattern that is not a regex, and has one text, is kept as a literal, so no regex is built for it.
     *
     *  @param patternTexts The pattern texts. Each must be non-empty, and must not hold a newline. The
     *                      views are read during the call only: create copies the text it keeps.
     *  @param isRegex      True when the texts are regular expressions.
     *  @param isIgnoreCase True to ignore case.
     *  @param isExactLine  True to match only a whole line.
     *  @param isWord       True to match only whole words.
     *
     *  @return Status result paired with the Pattern, or failure with null for an empty text,
     *          a text with a newline, or an invalid regex.
     */
    static std::pair<juce::Result, std::unique_ptr<Pattern>> create (juce::Span<const std::string_view> patternTexts,
                                                                     bool isRegex,
                                                                     bool isIgnoreCase,
                                                                     bool isExactLine = false,
                                                                     bool isWord = false);

    /** @brief Tests whether a line contains a match.
     *
     *  @param line       The line to test, without its line ending.
     *  @param automaton  Matcher state that the caller owns.
     *
     *  @return Status result paired with true when the line matches.
     */
    std::pair<juce::Result, bool> hasMatch (std::string_view line, jam::RegExpAutomaton& automaton) const;

    /** @brief Finds the next match in a line, with its capture groups.
     *
     *  For a literal pattern with word matching, a match that is adjacent to a word byte is skipped,
     *  and the search continues after it. A regex pattern applies its word rule in its expression.
     *
     *  @param line   The line to search.
     *  @param offset The byte offset where the search starts.
     *
     *  @return Status result paired with the captures. Group 0 is the match. The captures are
     *          empty when no match is found.
     */
    std::pair<juce::Result, Captures> findNext (std::string_view line, size_t offset) const;

    /** @brief Returns the number of explicit capture groups in the pattern.
     *
     *  @return Capture group count; zero for literal patterns.
     */
    int getNumCaptureGroups() const noexcept;

    /** @brief Finds the next line that can match, so that a search skips the lines that cannot.
     *
     *  With required literals, the call is a SIMD search for them. Without literals, the automaton
     *  of the program finds the next matching line. With neither, the call returns offset unchanged.
     *
     *  @param bytes      The bytes to scan, with line endings.
     *  @param offset     The byte offset where the scan starts. Must not exceed bytes.size().
     *  @param automaton  Matcher state that the caller owns.
     *
     *  @return Offset of a byte in a candidate line, or bytes.size() when no candidate remains.
     */
    size_t findCandidate (std::string_view bytes, size_t offset, jam::RegExpAutomaton& automaton) const;

    /** @brief Reports whether a candidate line needs no second test.
     *
     *  A candidate line from findCandidate is a match when this returns true. Otherwise the line
     *  is confirmed with hasMatch or findNext.
     *
     *  @return True when findCandidate alone decides the match.
     */
    bool isConfirmed() const noexcept;

private:
    /** @brief Finds the next match from an offset, with the literal, the compiled program, or the backtracking matcher.
     *
     *  @param line   The line to search.
     *  @param offset The byte offset where the search starts.
     *
     *  @return Status result paired with the captures. Group 0 is the match. The captures are empty when no match is found.
     */
    std::pair<juce::Result, Captures> findSpan (std::string_view line, size_t offset) const;

    /** @brief Finds the next match with the backtracking matcher, and fails when its step limit is exceeded.
     *
     *  The backtracking matcher is used when no program is built, or when the program has an empty repetition.
     *
     *  @param line   The line to search.
     *  @param offset The byte offset where the search starts.
     *
     *  @return Status result paired with the captures. The failure is the step limit failure.
     */
    std::pair<juce::Result, Captures> findBacktracked (std::string_view line, size_t offset) const;

    /** @brief The compiled expression, or null for a literal pattern. */
    std::unique_ptr<jam::RegExp> regExp;

    /** @brief The program that runs the expression with the automaton, or null when none is built. */
    std::unique_ptr<jam::RegExpProgram> program;

    /** @brief The byte size of the text of a single literal pattern, which gives the end of a match. Zero for every other pattern. */
    size_t literalSize { 0 };

    /** @brief The prefilter literals that find the candidate lines, or null when none applies. */
    std::unique_ptr<jam::LiteralSet> literalSet;

    /** @brief The pattern texts joined with a pipe, kept for the step limit diagnostic. */
    std::string patternText;

    /** @brief True when a candidate line from findCandidate is a match with no second test. */
    bool isCandidateMatch { false };

    /** @brief True when the pattern must match a whole line. */
    bool isExactLine { false };

    /** @brief True when the pattern must match whole words. */
    bool isWord { false };
};
