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
     *  @param arguments            The argument list, with the verb name at index 0.
     *  @param flagOptions          Options that take no value.
     *  @param valueOptions         Options that take the next argument as their value.
     *  @param optionalValueOptions Options that take the next argument as their value, unless `--stdin` applies.
     */
    Arguments (const jam::Strings& arguments,
               const std::vector<std::string_view>& flagOptions,
               const std::vector<std::string_view>& valueOptions,
               const std::vector<std::string_view>& optionalValueOptions);

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
    static juce::String getOptionName (std::string_view option);

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

    /** @brief Tests whether an option was matched as a flag or as a value option.
     *
     *  @param option The option text, with its aliases separated by `|`.
     *
     *  @return True when the option was matched.
     */
    bool contains (std::string_view option) const;

    /** @brief Returns the value of an option. An option may repeat, so the last value wins.
     *
     *  @param option The option text, with its aliases separated by `|`.
     *
     *  @return The last value of the option, or an empty string when the option is absent.
     */
    juce::String getValue (std::string_view option) const;

    /** @brief Returns the value of an option as a view. An option may repeat, so the last value wins.
     *
     *  The view stays valid while this Arguments object lives.
     *
     *  @param option The option text, with its aliases separated by `|`.
     *
     *  @return The last value of the option, or an empty view when the option is absent.
     */
    std::string_view getValueView (std::string_view option) const;

    /** @brief Returns every value of an option, in the order given.
     *
     *  @param option The option text, with its aliases separated by `|`.
     *
     *  @return All values of the option. The list is empty when the option is absent.
     */
    jam::Strings getValues (std::string_view option) const;

    /** @brief Returns positional arguments in order, including all tokens after `--`.
     *
     *  @return Reference to the parsed positional argument list.
     */
    const jam::Strings& getPositionals() const noexcept;

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
    static bool containsOption (const std::vector<std::string_view>& matchedOptions, std::string_view option);
    static int getMatchingOptionIndex (std::string_view argument,
                                       const std::vector<std::string_view>& optionList);
    static bool isOptionArgument (std::string_view argument) noexcept;
    static std::pair<bool, int> getParsedLineNumber (const juce::String& token, int minimumValue);

    int addToken (const jam::Strings& arguments,
                  int index,
                  std::string_view token,
                  const std::vector<std::string_view>& flagOptions,
                  const std::vector<std::string_view>& valueOptions,
                  const std::vector<std::string_view>& optionalValueOptions);
    int addShortFlags (const jam::Strings& arguments,
                       int index,
                       const std::vector<std::string_view>& flagOptions,
                       const std::vector<std::string_view>& valueOptions,
                       const std::vector<std::string_view>& optionalValueOptions);
    int addValue (const jam::Strings& arguments,
                  int index,
                  std::string_view option,
                  std::string_view token);
    int addFailure (const char* format, std::string_view token, int nextIndex);

    juce::Result status { juce::Result::ok() };
    std::vector<std::string_view> matchedFlags;
    std::vector<std::string_view> matchedValueOptions;
    jam::Strings matchedValues;
    jam::Strings positionals;
};
