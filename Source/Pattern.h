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
     *  @param newLiteral       The case-folded literal of a single literal pattern. Empty otherwise.
     *  @param newLiteralSet    The prefilter literals, or null when none applies.
     *  @param newPatternText   The pattern texts joined with `|`, kept for diagnostics.
     *  @param newIsLiteral     True when the pattern matches its text as a literal.
     *  @param newIsIgnoreCase  True when case is ignored.
     *  @param newIsExactLine   True when the pattern must match a whole line.
     *  @param newIsWord        True when the pattern must match whole words.
     */
    Pattern (Access,
             std::unique_ptr<jam::RegExp> newRegExp,
             std::unique_ptr<jam::RegExpProgram> newProgram,
             std::string newLiteral,
             std::unique_ptr<jam::LiteralSet> newLiteralSet,
             std::string newPatternText,
             bool newIsLiteral,
             bool newIsIgnoreCase,
             bool newIsExactLine,
             bool newIsWord);

    /** @brief Compiles one or more pattern texts into a Pattern.
     *
     *  A pattern is a literal text, or a regex when isRegex is true. Several texts give an alternation.
     *  A pattern that is not a regex, and has one text, is kept as a literal, so no regex is built for it.
     *
     *  @param patternTexts The pattern texts. Each must be non-empty, and must not hold a newline.
     *  @param isRegex      True when the texts are regular expressions.
     *  @param isIgnoreCase True to ignore case.
     *  @param isExactLine  True to match only a whole line.
     *  @param isWord       True to match only whole words.
     *
     *  @return Status result paired with the Pattern, or failure with null for an empty text,
     *          a text with a newline, or an invalid regex.
     */
    static std::pair<juce::Result, std::unique_ptr<Pattern>> create (const jam::Strings& patternTexts,
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
    std::pair<juce::Result, Captures> findSpan (std::string_view line, size_t offset) const;
    std::pair<juce::Result, Captures> findBacktracked (std::string_view line, size_t offset) const;

    std::unique_ptr<jam::RegExp> regExp;
    std::unique_ptr<jam::RegExpProgram> program;
    std::string literal;
    std::unique_ptr<jam::LiteralSet> literalSet;
    std::string patternText;
    bool isLiteral { false };
    bool isIgnoreCase { false };
    bool isExactLine { false };
    bool isWord { false };
};
