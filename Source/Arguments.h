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

    /** @brief Selects how an option takes its value from one argument token.
     *
     *  The separated grammar takes the value from the next argument. The attached grammar also takes the value from the text after an `=`, as in `--option=value`, and makes an optional-value option a flag when no `=` value is given.
     */
    enum class Grammar { separated, attached };

    /** @brief Classifies an option by how many argument tokens it consumes as its value.
     *
     *  A flag consumes none. A value option consumes one token. An optional-value option consumes one token unless it is recorded as a flag. A named-value option consumes two tokens. A mode option consumes none and sets the mode of the positionals that follow it.
     */
    enum class OptionType { flag, value, optionalValue, namedValue, mode };

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

    /** @brief Parses the argument list from firstOptionIndex with one option table, where each entry carries its option type.
     *
     *  The grammar selects separated values, or attached values written as `--option=value`.
     *  The argument list must outlive this object.
     *
     *  @param arguments The argument list, with the verb name at index 0.
     *  @param options   The option table: each option text with its option type.
     *  @param grammar   The grammar of the option values.
     */
    Arguments (juce::Span<const std::string_view> arguments,
               std::initializer_list<std::pair<std::string_view, OptionType>> options,
               Grammar grammar);

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

    /** @brief Returns how many times a flag option was matched.
     *
     *  The option text must equal the option table entry, aliases included.
     *
     *  @param option The option table entry of the flag.
     *
     *  @return The number of matches. Zero when the flag was not matched.
     */
    int getCount (std::string_view option) const;
    /** @brief Returns the flag that was matched last among the listed flags.
     *
     *  The search runs from the last matched flag to the first, so the flag that was matched last wins.
     *
     *  @param options The option table entries to search for, each equal to its entry text.
     *
     *  @return The matched flag text, or an empty view when no listed flag was matched.
     */
    std::string_view getLastFlag (std::initializer_list<std::string_view> options) const;

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

    /** @brief Returns the positional mode that was in effect when a positional was added.
     *
     *  Each positional records the mode set by the last mode option before it. An empty view means that no mode option preceded it.
     *
     *  @param positionalIndex The index of the positional. It must be in [0, size).
     *
     *  @return The option text of the mode, or an empty view.
     */
    std::string_view getPositionalMode (int positionalIndex) const;

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

    /** @brief Counts the entries of a list of matched option texts that equal the option text.
     *
     *  @param matchedOptions The option texts that were matched.
     *  @param option         The option text to count.
     *
     *  @return The number of entries that equal the option text.
     */
    static int getOptionCount (const jam::Array<std::string_view>& matchedOptions, std::string_view option);

    /** @brief Finds the option table entry that an argument names. The help option is the fallback.
     *
     *  The table is searched in order. When no entry matches, the argument matches the help option as a flag, so the help option is always recognised.
     *
     *  @param argument The argument text to test.
     *  @param options  The option table.
     *
     *  @return The matched entry, or an empty optional when neither an entry nor the help option matches.
     */
    static std::optional<std::pair<std::string_view, OptionType>> getMatchingOption (std::string_view argument,
                                                                                     juce::Span<const std::pair<std::string_view, OptionType>> options);

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

    /** @brief Splits an attached `option=value` token at its first `=`, under the attached grammar only.
     *
     *  Under the separated grammar, or when the token holds no `=`, the whole token is the option text.
     *
     *  @param token   The argument text to split.
     *  @param grammar The grammar of the option values.
     *
     *  @return The option text, and the attached value when one was split off.
     */
    static std::pair<std::string_view, std::optional<std::string_view>> getSplitToken (std::string_view token, Grammar grammar);

    /** @brief Returns how many argument tokens an option type consumes as its value.
     *
     *  @param optionType The option type.
     *
     *  @return Zero for a flag and a mode, one for a value and an optional value, two for a named value.
     */
    static int getValueCount (OptionType optionType);

    /** @brief Walks the argument list from firstOptionIndex to the first `--`, then records the rest as positionals.
     *
     *  Each token before the `--` is handled by addToken, which returns the index of the next token. The `--` itself is not recorded.
     *
     *  @param arguments The argument list, with the verb name at index 0.
     *  @param options   The option table.
     *  @param grammar   The grammar of the option values.
     */
    void setParsedArguments (juce::Span<const std::string_view> arguments,
                             juce::Span<const std::pair<std::string_view, OptionType>> options,
                             Grammar grammar);

    /** @brief Handles one argument token: an option table match, or a token that matches no entry.
     *
     *  A token that matches an option table entry is handled by addOption. Any other token is handled by addUnmatchedToken.
     *
     *  @param arguments The argument list.
     *  @param index     The index of the token in the argument list.
     *  @param token     The argument text.
     *  @param options   The option table.
     *  @param grammar   The grammar of the option values.
     *
     *  @return The index of the next token to parse.
     */
    int addToken (juce::Span<const std::string_view> arguments,
                  int index,
                  std::string_view token,
                  juce::Span<const std::pair<std::string_view, OptionType>> options,
                  Grammar grammar);

    /** @brief Handles a token that matches no option table entry: a short flag group, an unknown flag, or a positional.
     *
     *  A token of more than two characters that has one leading dash, and not two, is a short flag group, handled by addShortLetter. Any other token that starts with a dash, and is not a number, is an unknown flag, and it fails the parse. Any other token is a positional.
     *
     *  @param arguments The argument list.
     *  @param index     The index of the token in the argument list.
     *  @param token     The argument text.
     *  @param options   The option table.
     *  @param grammar   The grammar of the option values.
     *
     *  @return The index of the next token to parse.
     */
    int addUnmatchedToken (juce::Span<const std::string_view> arguments,
                           int index,
                           std::string_view token,
                           juce::Span<const std::pair<std::string_view, OptionType>> options,
                           Grammar grammar);

    /** @brief Handles one letter of a short flag group, such as `-abc`, at the given letter index.
     *
     *  A letter that takes no value, and is not the last letter, is recorded as a flag, and the next letter is handled by a recursive call. The last letter takes the next argument as its value. A value letter that is not last takes the rest of the token as its value under the attached grammar only. Under the separated grammar that rest is an unknown flag. An unknown letter fails the parse.
     *
     *  @param arguments   The argument list.
     *  @param index       The index of the token in the argument list.
     *  @param argument    The short flag group text.
     *  @param letterIndex The index of the letter in the group text.
     *  @param options     The option table.
     *  @param grammar     The grammar of the option values.
     *
     *  @return The index of the next token to parse.
     */
    int addShortLetter (juce::Span<const std::string_view> arguments,
                        int index,
                        std::string_view argument,
                        size_t letterIndex,
                        juce::Span<const std::pair<std::string_view, OptionType>> options,
                        Grammar grammar);

    /** @brief Records one option match, and returns the index of the next token.
     *
     *  An attached value goes to addAttachedValue. An option that takes no value, under the current grammar, is recorded as a flag. Any other option takes its values from the following tokens through addValues.
     *
     *  @param arguments     The argument list.
     *  @param index         The index of the token in the argument list.
     *  @param option        The matched option table entry.
     *  @param token         The argument text that named the option.
     *  @param attachedValue The value attached to the token, when one was split off.
     *  @param grammar       The grammar of the option values.
     *
     *  @return The index of the next token to parse.
     */
    int addOption (juce::Span<const std::string_view> arguments,
                   int index,
                   std::pair<std::string_view, OptionType> option,
                   std::string_view token,
                   std::optional<std::string_view> attachedValue,
                   Grammar grammar);

    /** @brief Tests whether an option takes no value tokens at this position.
     *
     *  An option with a zero value count takes none. An optional-value option takes none under the attached grammar, when the next token is `--stdin`, or when `--stdin` was already matched.
     *
     *  @param option    The option table entry.
     *  @param arguments The argument list.
     *  @param index     The index of the option token in the argument list.
     *  @param grammar   The grammar of the option values.
     *
     *  @return True when the option is recorded as a flag.
     */
    bool isWithoutValue (std::pair<std::string_view, OptionType> option,
                         juce::Span<const std::string_view> arguments,
                         int index,
                         Grammar grammar) const;

    /** @brief Records a value that is attached to an option token, under the attached grammar only.
     *
     *  A value option or an optional-value option records the attached value. Any other option, and any option under the separated grammar, fails the parse as an unknown flag.
     *
     *  @param option        The option table entry.
     *  @param token         The argument text that named the option.
     *  @param attachedValue The attached value text.
     *  @param grammar       The grammar of the option values.
     *  @param nextIndex     The index of the next token to parse.
     *
     *  @return The index of the next token to parse.
     */
    int addAttachedValue (std::pair<std::string_view, OptionType> option,
                          std::string_view token,
                          std::string_view attachedValue,
                          Grammar grammar,
                          int nextIndex);

    /** @brief Takes the next valueCount argument tokens as the values of an option.
     *
     *  When those tokens exist, each one is recorded as a value of the option. When they do not, the parse fails with a missing value.
     *
     *  @param arguments  The argument list.
     *  @param index      The index of the option token in the argument list.
     *  @param option     The option table text, which keys the recorded values.
     *  @param token      The argument text that named the option.
     *  @param valueCount The number of value tokens to take.
     *
     *  @return The index of the next token to parse.
     */
    int addValues (juce::Span<const std::string_view> arguments,
                   int index,
                   std::string_view option,
                   std::string_view token,
                   int valueCount);

    /** @brief Appends one value to the list of an option, which is created at the first value.
     *
     *  @param option The option table text, which keys the list.
     *  @param value  The value text, stored as a view.
     */
    void addMatchedValue (std::string_view option, std::string_view value);

    /** @brief Records a flag match, and sets the positional mode when the option is a mode option.
     *
     *  @param option The option table entry.
     */
    void addFlag (std::pair<std::string_view, OptionType> option);

    /** @brief Records an argument that is neither an option nor an option value, with the positional mode in effect.
     *
     *  @param token The argument text, stored as a view.
     */
    void addPositional (std::string_view token);

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
    /** @brief The positional mode in effect for each positional, in the order of the positionals. */
    jam::Array<std::string_view> positionalModes;

    /** @brief The mode of the last mode option that matched. Empty until one matches. */
    std::string_view positionalMode {};
};
