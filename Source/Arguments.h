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

    /** @brief Index of the first argument after the verb.
     *
     *  The verb name is at index 0 of a verb's argument list, so the options start at index 1.
     */
    static constexpr int firstOptionIndex { 1 };

    /** @brief Parses the argument list from firstOptionIndex into flags, values, and positionals.
     *
     *  A `--` argument ends the options. Each argument after it is positional.
     *  An unknown flag or a missing value is recorded in getStatus().
     *  An optional-value option takes the next argument. When `--stdin` is the next argument, or
     *  was already matched, the optional-value option acts as a flag.
     *
     *  Every stored value and positional is a view into the argument list, or into an option text.
     *  The argument list must outlive this object.
     *
     *  @param arguments            The argument list, with the verb name at index 0.
     *  @param flagOptions          Options that take no value.
     *  @param valueOptions         Options that take the next argument as their value.
     *  @param optionalValueOptions Options that take the next argument as their value, unless `--stdin` applies.
     */
    Arguments (juce::Span<const std::string_view> arguments,
               std::initializer_list<std::string_view> flagOptions,
               std::initializer_list<std::string_view> valueOptions,
               std::initializer_list<std::string_view> optionalValueOptions);

    /** @brief Tests whether an argument matches an option text.
     *
     *  An option text holds its aliases separated by `|`. A match with any alias is a match.
     *
     *  @param argument The argument text to test.
     *  @param option   The option text, with its aliases separated by `|`.
     *
     *  @return True when the argument equals one of the aliases.
     */
    static bool matchesOption (std::string_view argument, std::string_view option);

    /** @brief Returns the name of an option, which is its first alias.
     *
     *  @param option The option text, with its aliases separated by `|`.
     *
     *  @return The first alias of the option text.
     */
    static std::string_view getOptionName (std::string_view option);

    /** @brief Parses a line or `start:end` range specification into a half-open range.
     *
     *  @param rangeText      Line spec (`N`, `N:M`, `N:`, or `:M`).
     *  @param allowZeroStart True to allow line `0` for single-line specs without a colon.
     *
     *  @return Status result paired with an exclusive-end line range.
     */
    static std::pair<juce::Result, juce::Range<int>> getLineRange (std::string_view rangeText,
                                                                   bool allowZeroStart);

    /** @brief Returns the parse status recorded during construction.
     *
     *  @return Result indicating success, unknown flag, or missing option value.
     */
    const juce::Result& getStatus() const noexcept;

    /** @brief Tests whether an option was matched as a flag or as a value option.
     *
     *  @param option The option text, with its aliases separated by `|`.
     *
     *  @return True when the option was matched.
     */
    bool contains (std::string_view option) const;

    /** @brief Returns the value of an option as a view. An option may repeat, so the last value wins.
     *
     *  The view points into the argument list, so it stays valid while that list lives.
     *
     *  @param option The option text, with its aliases separated by `|`.
     *
     *  @return The last value of the option, or an empty view when the option is absent.
     */
    std::string_view getValueView (std::string_view option) const;

    /** @brief Returns every value of an option, in the order given.
     *
     *  The span is valid while this Arguments object and the argument list live.
     *
     *  @param option The option text, with its aliases separated by `|`.
     *
     *  @return All values of the option. The list is empty when the option is absent.
     */
    juce::Span<const std::string_view> getValues (std::string_view option) const;

    /** @brief Returns positional arguments in order, including all tokens after `--`.
     *
     *  The views point into the argument list.
     *
     *  @return Reference to the parsed positional argument list.
     */
    const jam::Array<std::string_view>& getPositionals() const noexcept;

    /** @brief Returns the positional arguments from an index to the end.
     *
     *  The span is valid while this Arguments object lives.
     *
     *  @param firstIndex The index of the first positional to return. It must be in [0, size].
     *
     *  @return The positionals from firstIndex to the end. The span is empty when firstIndex equals the size.
     */
    juce::Span<const std::string_view> getPositionals (int firstIndex) const noexcept;

    /** @brief Parses the whole text as a signed decimal integer.
     *
     *  The text is read by std::from_chars. A plus sign fails. Leading zeros are accepted here,
     *  so a caller that needs the canonical form checks it after the parse.
     *
     *  @param text The text to parse.
     *
     *  @return The integer, or an empty optional when the text is not a whole integer.
     */
    static std::optional<int> getInteger (std::string_view text);

    /** @brief Returns the integer value of an option, with a minimum.
     *
     *  The value must be a canonical decimal integer, so text such as `007` or `+5` fails.
     *  The status fails when the text is not such an integer, or when it is below the minimum.
     *  The value is 0 when the status fails.
     *
     *  @param option       The option text, with its aliases separated by `|`.
     *  @param minimumValue The smallest accepted value.
     *  @param defaultValue The value returned when the option is absent.
     *
     *  @return Status result paired with the parsed value, or with the default when the option is absent.
     */
    std::pair<juce::Result, int> getInteger (std::string_view option, int minimumValue, int defaultValue) const;

    /** @brief Resolves the `--lines` option or returns `[1, openRangeEnd)` when omitted.
     *
     *  @return Status result paired with the half-open line filter range.
     */
    std::pair<juce::Result, juce::Range<int>> getLineRangeFilter() const;

private:
    /** @brief Tests whether a list of matched option texts holds the option text.
     *
     *  @param matchedOptions The option texts that were matched.
     *  @param option         The option text to find.
     *
     *  @return True when the list holds the option text.
     */
    static bool containsOption (const jam::Array<std::string_view>& matchedOptions, std::string_view option);

    /** @brief Finds the first option text in a list that the argument matches, through any of its aliases.
     *
     *  @param argument   The argument text to test.
     *  @param optionList The option texts to test, in priority order.
     *
     *  @return The index of the first matching option text, or -1 when none matches.
     */
    static int getMatchingOptionIndex (std::string_view argument,
                                       std::initializer_list<std::string_view> optionList);

    /** @brief Reports whether an argument is an option: a leading dash followed by a character that is not a digit.
     *
     *  A lone dash and a negative number are positionals.
     *
     *  @param argument The argument text to test.
     *
     *  @return True when the argument is an option.
     */
    static bool isOptionArgument (std::string_view argument) noexcept;

    /** @brief Parses a line number in canonical decimal form, with a minimum.
     *
     *  Only the canonical form is accepted: a single digit, or a number without leading zeros and without a sign.
     *
     *  @param token        The text to parse.
     *  @param minimumValue The smallest accepted value.
     *
     *  @return True and the value when the text is a canonical integer at or above the minimum, otherwise false and 0.
     */
    static std::pair<bool, int> getParsedLineNumber (std::string_view token, int minimumValue);

    /** @brief Parses one argument: a flag, a value option with its value, a short-flag group, or a positional.
     *
     *  @param arguments            The whole argument list, with the verb name at index 0.
     *  @param index                The index of the argument to parse.
     *  @param token                The argument text, or the single-letter option of a short-flag group.
     *  @param flagOptions          Options that take no value.
     *  @param valueOptions         Options that take the next argument as their value.
     *  @param optionalValueOptions Options that take the next argument as their value, unless --stdin applies.
     *
     *  @return The index of the next argument to parse.
     */
    int addToken (juce::Span<const std::string_view> arguments,
                  int index,
                  std::string_view token,
                  std::initializer_list<std::string_view> flagOptions,
                  std::initializer_list<std::string_view> valueOptions,
                  std::initializer_list<std::string_view> optionalValueOptions);

    /** @brief Parses a group of single-letter flags, such as `-abc`, where a value letter may end the group.
     *
     *  An unknown letter is a failure, and the group ends at it.
     *
     *  @param arguments            The whole argument list, with the verb name at index 0.
     *  @param index                The index of the group argument.
     *  @param flagOptions          Options that take no value.
     *  @param valueOptions         Options that take the next argument as their value.
     *  @param optionalValueOptions Options that take the next argument as their value, unless --stdin applies.
     *
     *  @return The index of the next argument to parse.
     */
    int addShortFlags (juce::Span<const std::string_view> arguments,
                       int index,
                       std::initializer_list<std::string_view> flagOptions,
                       std::initializer_list<std::string_view> valueOptions,
                       std::initializer_list<std::string_view> optionalValueOptions);

    /** @brief Records the next argument as the value of an option, keyed by the option text.
     *
     *  A value option at the end of the list is a failure.
     *
     *  @param arguments The whole argument list, with the verb name at index 0.
     *  @param index     The index of the option argument.
     *  @param option    The option text, with its aliases, that keys the value.
     *  @param token     The option argument as the user typed it, named in the failure.
     *
     *  @return The index of the argument after the value.
     */
    int addValue (juce::Span<const std::string_view> arguments,
                  int index,
                  std::string_view option,
                  std::string_view token);

    /** @brief Records a parse failure when none is recorded yet, and returns the index of the next argument.
     *
     *  Only the first failure is kept in the status. A later failure leaves the status unchanged.
     *
     *  @param failure   The failure text that the status reports.
     *  @param token     The argument text that the failure names.
     *  @param nextIndex The index of the next argument to parse.
     *
     *  @return nextIndex.
     */
    int addFailure (std::string_view failure, std::string_view token, int nextIndex);

    /** @brief The first parse failure, or ok. Later failures do not replace it. */
    juce::Result status { juce::Result::ok() };

    /** @brief The option texts of the flags that were matched, in the order matched. */
    jam::Array<std::string_view> matchedFlags;

    /** @brief The values of each value option, keyed by the option text. Every value is kept, in order. */
    jam::HashMap<std::string_view, jam::Array<std::string_view>> matchedValues;

    /** @brief The arguments that are neither options nor option values, in order, including those after `--`. */
    jam::Array<std::string_view> positionals;
};
