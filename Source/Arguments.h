#pragma once
#include <JuceHeader.h>

/** @brief Parses command-line flags, value options, `--` terminator, and positional arguments.
 */
class Arguments
{
public:
    /** @brief Sentinel exclusive end value representing an open-ended line range.
     */
    static constexpr int openRangeEnd { std::numeric_limits<int>::max() };

    /** @brief Parses an argument list from @p startIndex using allowed option specifications.
     *
     *  Tokens after `--` become positionals; negative-digit tokens stay positional.
     *
     *  @param arguments    Full command argument array.
     *  @param startIndex   Zero-based index where option parsing begins.
     *  @param flagOptions  Allowed flag specs with pipe-delimited aliases.
     *  @param valueOptions Allowed value-option specs with pipe-delimited aliases.
     */
    Arguments (const jam::Strings& arguments,
               int startIndex,
               const jam::Strings& flagOptions,
               const jam::Strings& valueOptions);

    /** @brief Checks whether a single token matches any alias in an option specification.
     *
     *  @param argument Single command-line token to test.
     *  @param option   Option spec containing one or more pipe-delimited aliases.
     *
     *  @return True when @p argument equals any alias in @p option.
     */
    static bool matchesOption (const juce::String& argument, const juce::String& option);

    /** @brief Checks whether an option appears before `--` starting at @p startIndex.
     *
     *  @param arguments  Full command argument array.
     *  @param startIndex Zero-based index where scanning begins.
     *  @param option     Option spec with pipe-delimited aliases.
     *
     *  @return True if any token before `--` matches @p option.
     */
    static bool containsOptionBeforeTerminator (const jam::Strings& arguments,
                                                int startIndex,
                                                const juce::String& option);

    /** @brief Parses a line or `start:end` range specification into a half-open range.
     *
     *  @param rangeText      Line spec (`N`, `N:M`, `N:`, or `:M`).
     *  @param allowZeroStart True to allow line `0` for single-line specs without a colon.
     *
     *  @return Status result paired with an exclusive-end line range.
     */
    static std::pair<juce::Result, juce::Range<int>> getLineRange (const juce::String& rangeText,
                                                                   bool allowZeroStart);

    /** @brief Returns the parse status recorded during construction.
     *
     *  @return Result indicating success, unknown flag, or missing option value.
     */
    const juce::Result& getStatus() const noexcept;

    /** @brief Checks whether a flag or value option appeared before `--`.
     *
     *  @param option Canonical option spec string passed to the constructor.
     *
     *  @return True when @p option matched at least once.
     */
    bool contains (const juce::String& option) const;

    /** @brief Returns the last value supplied for a value option.
     *
     *  @param option Canonical value-option spec string.
     *
     *  @return Last matched value, or an empty string when absent.
     */
    juce::String getValue (const juce::String& option) const;

    /** @brief Returns all values supplied for a value option in argument order.
     *
     *  @param option Canonical value-option spec string.
     *
     *  @return Array of all matched values for @p option.
     */
    jam::Strings getValues (const juce::String& option) const;

    /** @brief Returns positional arguments in order, including all tokens after `--`.
     *
     *  @return Reference to the parsed positional argument list.
     */
    const jam::Strings& getPositionals() const noexcept;

    /** @brief Parses the last value of @p option as a decimal integer at or above @p minimumValue.
     *
     *  @param option       Canonical value-option spec string.
     *  @param minimumValue Inclusive lower bound for the integer value.
     *
     *  @return Status result paired with the parsed integer, or failure with `0`.
     */
    std::pair<juce::Result, int> getInteger (const juce::String& option, int minimumValue) const;

    /** @brief Resolves the `--lines` option or returns `[1, openRangeEnd)` when omitted.
     *
     *  @return Status result paired with the half-open line filter range.
     */
    std::pair<juce::Result, juce::Range<int>> getLineRangeFilter() const;

private:
    static int getMatchingOptionIndex (const juce::String& argument,
                                       const jam::Strings& optionList);
    static int getTerminatorIndex (const jam::Strings& arguments, int startIndex);
    static bool isOptionArgument (const juce::String& argument) noexcept;
    static std::pair<bool, int> getParsedLineNumber (const juce::String& token, int minimumValue);

    juce::Result status { juce::Result::ok() };
    jam::Strings matchedFlags;
    jam::Array<std::pair<juce::String, juce::String>> matchedValues;
    jam::Strings positionals;
};
