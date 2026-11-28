#include "Arguments.h"
#include "Stdio.h"
#include "generated/Generated.h"

static constexpr std::array<int, 5> valueCounts { 0, 1, 1, 2, 0 };

Arguments::Arguments (juce::Span<const std::string_view> arguments,
                      std::initializer_list<std::string_view> flagOptions,
                      std::initializer_list<std::string_view> valueOptions,
                      std::initializer_list<std::string_view> optionalValueOptions)
{
    const std::array<std::pair<std::initializer_list<std::string_view>, OptionType>, 3> optionGroups {{
        { flagOptions, OptionType::flag },
        { valueOptions, OptionType::value },
        { optionalValueOptions, OptionType::optionalValue }
    }};
    jam::Array<std::pair<std::string_view, OptionType>> optionList;

    for (const auto& [optionTexts, optionType] : optionGroups)
        for (const auto optionText : optionTexts)
            optionList.add (std::pair<std::string_view, OptionType> { optionText, optionType });

    setParsedArguments (arguments,
                        juce::Span<const std::pair<std::string_view, OptionType>> { optionList.data(), static_cast<size_t> (optionList.size()) },
                        Grammar::separated);
}

Arguments::Arguments (juce::Span<const std::string_view> arguments,
                      std::initializer_list<std::pair<std::string_view, OptionType>> options,
                      Grammar grammar)
{
    setParsedArguments (arguments,
                        juce::Span<const std::pair<std::string_view, OptionType>> { options.begin(), options.size() },
                        grammar);
}

bool Arguments::matchesOption (std::string_view argument, std::string_view option)
{
    const auto separatorIndex { option.find (static_cast<char> (Chars::pipe)) };
    const auto alias { option.substr (0, separatorIndex) };

    if (argument.compare (alias) == 0)
        return true;

    return separatorIndex != std::string_view::npos
           and matchesOption (argument, option.substr (separatorIndex + 1));
}

std::string_view Arguments::getOptionName (std::string_view option)
{
    return option.substr (0, option.find (static_cast<char> (Chars::pipe)));
}

std::pair<juce::Result, juce::Range<int>> Arguments::getLineRange (std::string_view rangeText,
                                                                   bool allowZeroStart)
{
    const auto colonIndex { rangeText.find (static_cast<char> (Chars::colon)) };

    if (colonIndex == std::string_view::npos)
    {
        const auto [isValid, lineNumber] { getParsedLineNumber (rangeText, allowZeroStart ? 0 : 1) };

        if (isValid and lineNumber < openRangeEnd)
            return { juce::Result::ok(), juce::Range<int> (lineNumber, lineNumber + 1) };

        return { Stdio::fail (text::Diagnostics::failLineRangeInvalid, rangeText), {} };
    }

    const auto preColon { jam::Format::trim (rangeText.substr (0, colonIndex)) };
    const auto postColon { jam::Format::trim (rangeText.substr (colonIndex + 1)) };
    const auto hasValidHalves { postColon.find (static_cast<char> (Chars::colon)) == std::string_view::npos
                                and (not preColon.empty() or not postColon.empty()) };
    const auto [startValid, startLine] { preColon.empty()
                                             ? std::pair<bool, int> { true, 1 }
                                             : getParsedLineNumber (preColon, 1) };
    const auto [endValid, endLine] { postColon.empty()
                                         ? std::pair<bool, int> { true, openRangeEnd }
                                         : getParsedLineNumber (postColon, startLine) };

    if (hasValidHalves and startValid and endValid)
    {
        const auto endExclusive { (postColon.empty() or endLine == openRangeEnd) ? openRangeEnd : endLine + 1 };
        return { juce::Result::ok(), juce::Range<int> (startLine, endExclusive) };
    }

    return { Stdio::fail (text::Diagnostics::failLineRangeInvalid, rangeText), {} };
}

const juce::Result& Arguments::getStatus() const noexcept
{
    return status;
}

bool Arguments::contains (std::string_view option) const
{
    return containsOption (matchedFlags, option) or not getValues (option).empty();
}

int Arguments::getCount (std::string_view option) const
{
    return getOptionCount (matchedFlags, option);
}

std::string_view Arguments::getLastFlag (std::initializer_list<std::string_view> options) const
{
    for (int flagIndex { matchedFlags.size() - 1 }; flagIndex >= 0; --flagIndex)
    {
        const auto matchedFlag { matchedFlags.at (flagIndex) };
        const bool isListed { std::any_of (options.begin(),
                                           options.end(),
                                           [matchedFlag] (std::string_view option) { return option.compare (matchedFlag) == 0; }) };

        if (isListed)
            return matchedFlag;
    }

    return {};
}

std::string_view Arguments::getValueView (std::string_view option) const
{
    const auto values { getValues (option) };
    return values.empty() ? std::string_view {} : values.back();
}

juce::Span<const std::string_view> Arguments::getValues (std::string_view option) const
{
    const auto valueEntry { matchedValues.find (option) };

    if (valueEntry != matchedValues.end())
    {
        const auto& [matchedOption, optionValues] { *valueEntry };
        juce::ignoreUnused (matchedOption);
        return juce::Span<const std::string_view> { optionValues.data(), static_cast<size_t> (optionValues.size()) };
    }

    return {};
}

const jam::Array<std::string_view>& Arguments::getPositionals() const noexcept
{
    return positionals;
}

juce::Span<const std::string_view> Arguments::getPositionals (int firstIndex) const noexcept
{
    jassert (firstIndex >= 0 and firstIndex <= positionals.size());
    return juce::Span<const std::string_view> { positionals.data() + firstIndex,
                                                static_cast<size_t> (positionals.size() - firstIndex) };
}

std::string_view Arguments::getPositionalMode (int positionalIndex) const
{
    jassert (positionalIndex >= 0 and positionalIndex < positionalModes.size());
    return positionalModes.at (positionalIndex);
}

std::optional<int> Arguments::getInteger (std::string_view text)
{
    int value { 0 };
    const auto parsed { std::from_chars (text.data(), text.data() + text.size(), value) };

    if (parsed.ec == std::errc {} and parsed.ptr == text.data() + text.size())
        return value;

    return std::nullopt;
}

std::pair<juce::Result, int> Arguments::getInteger (std::string_view option, int minimumValue, int defaultValue) const
{
    if (not contains (option))
        return { juce::Result::ok(), defaultValue };

    const auto rawValue { getValueView (option) };
    const auto [isValid, parsedValue] { getParsedLineNumber (rawValue, minimumValue) };

    if (isValid)
        return { juce::Result::ok(), parsedValue };

    return { Stdio::fail (text::Diagnostics::failIntegerInvalid,
                          getOptionName (option),
                          jam::Strings::getView (Id::diagnosticSeparator),
                          rawValue),
             0 };
}

std::pair<juce::Result, juce::Range<int>> Arguments::getLineRangeFilter() const
{
    if (contains (Id::linesOption))
        return getLineRange (getValueView (Id::linesOption), false);

    return { juce::Result::ok(), juce::Range<int> (1, openRangeEnd) };
}

bool Arguments::containsOption (const jam::Array<std::string_view>& matchedOptions, std::string_view option)
{
    return getOptionCount (matchedOptions, option) > 0;
}

int Arguments::getOptionCount (const jam::Array<std::string_view>& matchedOptions, std::string_view option)
{
    return static_cast<int> (std::count_if (matchedOptions.begin(),
                                            matchedOptions.end(),
                                            [option] (std::string_view matchedOption) { return matchedOption.compare (option) == 0; }));
}

std::optional<std::pair<std::string_view, Arguments::OptionType>> Arguments::getMatchingOption (std::string_view argument,
                                                                                     juce::Span<const std::pair<std::string_view, OptionType>> options)
{
    for (size_t optionIndex { 0 }; optionIndex < options.size(); ++optionIndex)
    {
        const auto& [optionText, optionType] { options[optionIndex] };

        if (matchesOption (argument, optionText))
            return options[optionIndex];
    }

    if (matchesOption (argument, Id::helpOption))
        return std::pair<std::string_view, OptionType> { Id::helpOption, OptionType::flag };

    if (matchesOption (argument, Id::noLimitOption))
        return std::pair<std::string_view, OptionType> { Id::noLimitOption, OptionType::flag };

    return std::nullopt;
}

bool Arguments::isOptionArgument (std::string_view argument) noexcept
{
    return argument.size() > 1
           and argument.at (0) == static_cast<char> (Chars::dash)
           and not jam::Document::isDigit (static_cast<unsigned char> (argument.at (1)));
}

std::pair<bool, int> Arguments::getParsedLineNumber (std::string_view token, int minimumValue)
{
    const auto value { getInteger (token) };
    const bool isCanonical { token.size() == 1 or (not token.empty() and token.front() != static_cast<char> (Chars::zero) and jam::Document::isDigit (static_cast<unsigned char> (token.front()))) };
    const bool isValid { value.has_value() and *value >= minimumValue and isCanonical };
    return { isValid, isValid ? *value : 0 };
}

std::pair<std::string_view, std::optional<std::string_view>> Arguments::getSplitToken (std::string_view token, Grammar grammar)
{
    const auto separatorIndex { token.find (static_cast<char> (Chars::equals)) };
    const bool isAttached { grammar == Grammar::attached and separatorIndex != std::string_view::npos };

    if (isAttached)
        return { token.substr (0, separatorIndex), token.substr (separatorIndex + 1) };

    return { token, std::nullopt };
}

int Arguments::getValueCount (OptionType optionType)
{
    return valueCounts.at (static_cast<size_t> (optionType));
}

void Arguments::setParsedArguments (juce::Span<const std::string_view> arguments,
                                    juce::Span<const std::pair<std::string_view, OptionType>> options,
                                    Grammar grammar)
{
    int index { firstOptionIndex };

    while (index < static_cast<int> (arguments.size()) and arguments[static_cast<size_t> (index)].compare (Chars::doubleDash) != 0)
        index = addToken (arguments, index, arguments[static_cast<size_t> (index)], options, grammar);

    for (int positionalIndex { index + 1 }; positionalIndex < static_cast<int> (arguments.size()); ++positionalIndex)
        addPositional (arguments[static_cast<size_t> (positionalIndex)]);
}

int Arguments::addToken (juce::Span<const std::string_view> arguments,
                         int index,
                         std::string_view token,
                         juce::Span<const std::pair<std::string_view, OptionType>> options,
                         Grammar grammar)
{
    const auto [optionText, attachedValue] { getSplitToken (token, grammar) };
    const auto option { getMatchingOption (optionText, options) };

    if (option.has_value())
        return addOption (arguments, index, *option, token, attachedValue, grammar);

    return addUnmatchedToken (arguments, index, token, options, grammar);
}

int Arguments::addUnmatchedToken (juce::Span<const std::string_view> arguments,
                                  int index,
                                  std::string_view token,
                                  juce::Span<const std::pair<std::string_view, OptionType>> options,
                                  Grammar grammar)
{
    const bool isShortFlagGroup { token.size() > 2
                                  and token.at (1) != static_cast<char> (Chars::dash)
                                  and isOptionArgument (token) };

    if (isShortFlagGroup)
        return addShortLetter (arguments, index, token, 1, options, grammar);

    if (isOptionArgument (token))
        return addFailure (text::Diagnostics::failFlagUnknown, token, index + 1);

    addPositional (token);
    return index + 1;
}

int Arguments::addShortLetter (juce::Span<const std::string_view> arguments,
                               int index,
                               std::string_view argument,
                               size_t letterIndex,
                               juce::Span<const std::pair<std::string_view, OptionType>> options,
                               Grammar grammar)
{
    const std::array<char, 2> letterToken { static_cast<char> (Chars::dash), argument.at (letterIndex) };
    const std::string_view letterView { letterToken.data(), letterToken.size() };
    const auto letterOption { getMatchingOption (letterView, options) };
    const bool isLastLetter { letterIndex + 1 == argument.size() };

    if (not letterOption.has_value())
        return addFailure (text::Diagnostics::failFlagUnknown, argument, index + 1);

    const auto& [letterText, letterType] { *letterOption };

    if (getValueCount (letterType) == 0 and not isLastLetter)
    {
        addFlag (*letterOption);
        return addShortLetter (arguments, index, argument, letterIndex + 1, options, grammar);
    }

    if (isLastLetter)
        return addOption (arguments, index, *letterOption, letterView, std::nullopt, grammar);

    return addOption (arguments, index, *letterOption, argument, argument.substr (letterIndex + 1), grammar);
}

int Arguments::addOption (juce::Span<const std::string_view> arguments,
                          int index,
                          std::pair<std::string_view, OptionType> option,
                          std::string_view token,
                          std::optional<std::string_view> attachedValue,
                          Grammar grammar)
{
    if (attachedValue.has_value())
        return addAttachedValue (option, token, *attachedValue, grammar, index + 1);

    if (isWithoutValue (option, arguments, index, grammar))
    {
        addFlag (option);
        return index + 1;
    }

    const auto& [optionText, optionType] { option };
    return addValues (arguments, index, optionText, token, getValueCount (optionType));
}

bool Arguments::isWithoutValue (std::pair<std::string_view, OptionType> option,
                                juce::Span<const std::string_view> arguments,
                                int index,
                                Grammar grammar) const
{
    const auto& [optionText, optionType] { option };
    const bool isNextStdin { index + 1 < static_cast<int> (arguments.size())
                             and matchesOption (arguments[static_cast<size_t> (index + 1)], Id::stdinOption) };
    const bool isOptionalFlag { optionType == OptionType::optionalValue
                                and (grammar == Grammar::attached or isNextStdin or contains (Id::stdinOption)) };

    return getValueCount (optionType) == 0 or isOptionalFlag;
}

int Arguments::addAttachedValue (std::pair<std::string_view, OptionType> option,
                                 std::string_view token,
                                 std::string_view attachedValue,
                                 Grammar grammar,
                                 int nextIndex)
{
    const auto& [optionText, optionType] { option };
    const bool isValueOption { optionType == OptionType::value or optionType == OptionType::optionalValue };

    if (grammar == Grammar::attached and isValueOption)
    {
        addMatchedValue (optionText, attachedValue);
        return nextIndex;
    }

    return addFailure (text::Diagnostics::failFlagUnknown, token, nextIndex);
}

int Arguments::addValues (juce::Span<const std::string_view> arguments,
                          int index,
                          std::string_view option,
                          std::string_view token,
                          int valueCount)
{
    if (index + valueCount < static_cast<int> (arguments.size()))
    {
        for (int valueOffset { 1 }; valueOffset <= valueCount; ++valueOffset)
            addMatchedValue (option, arguments[static_cast<size_t> (index + valueOffset)]);

        return index + valueCount + 1;
    }

    return addFailure (text::Diagnostics::failFlagValueMissing, token, index + 1);
}

void Arguments::addMatchedValue (std::string_view option, std::string_view value)
{
    auto [valueEntry, isInserted] { matchedValues.try_emplace (option) };
    auto& [entryOption, entryValues] { *valueEntry };
    juce::ignoreUnused (entryOption, isInserted);
    entryValues.add (value);
}

void Arguments::addFlag (std::pair<std::string_view, OptionType> option)
{
    const auto& [optionText, optionType] { option };
    matchedFlags.add (optionText);

    if (optionType == OptionType::mode)
        positionalMode = optionText;
}

void Arguments::addPositional (std::string_view token)
{
    positionals.add (token);
    positionalModes.add (positionalMode);
}

int Arguments::addFailure (std::string_view failure, std::string_view token, int nextIndex)
{
    if (status.wasOk())
        status = Stdio::fail (failure, token);

    return nextIndex;
}
