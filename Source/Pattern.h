#pragma once
#include <JuceHeader.h>

/** @brief Compiles literal or regex patterns and extracts SIMD prefilter literals.
 */
class Pattern
{
    struct Access {};

public:
    /** @brief Stores a compiled pattern via passkey construction from create().
     *
     *  @param newRegExp           Compiled regex matcher, or null in literal mode.
     *  @param newLiteral          Literal search bytes (ASCII-folded when @p newIsIgnoreCase is true).
     *  @param newPrefilterLiteral Required case-sensitive substring for SIMD prefiltering, or empty.
     *  @param newNumCaptureGroups Count of explicit capture groups (zero in literal mode).
     *  @param newIsIgnoreCase     True when matching ignores character case.
     *  @param newIsExactLine      True when the pattern must match the full line.
     */
    Pattern (Access,
             std::unique_ptr<jam::RegExp> newRegExp,
             std::string newLiteral,
             std::string newPrefilterLiteral,
             int newNumCaptureGroups,
             bool newIsIgnoreCase,
             bool newIsExactLine);

    /** @brief Validates and compiles a non-empty literal or regular-expression pattern.
     *
     *  @param patternText  Non-empty search pattern text.
     *  @param isRegex      True to compile with jam::RegExp, false for literal byte search.
     *  @param isIgnoreCase True to enable case-insensitive matching.
     *  @param isExactLine  True to anchor matching to the entire line.
     *
     *  @return Status result paired with the compiled Pattern, or failure with null.
     */
    static std::pair<juce::Result, std::unique_ptr<Pattern>> create (const juce::String& patternText,
                                                                     bool isRegex,
                                                                     bool isIgnoreCase,
                                                                     bool isExactLine = false);

    /** @brief Searches for the next match at or after @p offset within @p line.
     *
     *  @param line   Subject line view without trailing newline bytes.
     *  @param offset Byte offset in @p line where matching begins.
     *
     *  @return Capture views with the full match at index 0, or empty when unmatched.
     */
    jam::Array<std::optional<std::string_view>> findNext (std::string_view line, size_t offset) const;

    /** @brief Reports whether the pattern matches anywhere on @p line (or full line if exact).
     *
     *  @param line Subject line view without trailing newline bytes.
     *
     *  @return True when @p line satisfies the pattern.
     */
    bool hasMatch (std::string_view line) const;

    /** @brief Returns the number of explicit capture groups in the pattern.
     *
     *  @return Capture group count; zero for literal patterns.
     */
    int getNumCaptureGroups() const noexcept;

    /** @brief Returns the required case-sensitive literal substring for SIMD prefiltering.
     *
     *  @return Prefilter substring view, or empty when case-insensitive or unavailable.
     */
    std::string_view getPrefilterLiteral() const noexcept;

private:
    static int getCaptureGroupCount (std::string_view patternText) noexcept;
    size_t findLiteralOffset (std::string_view line, size_t offset) const noexcept;

    std::unique_ptr<jam::RegExp> regExp;
    std::string literal;
    std::string prefilterLiteral;
    int numCaptureGroups { 0 };
    bool isIgnoreCase { false };
    bool isExactLine { false };
};
