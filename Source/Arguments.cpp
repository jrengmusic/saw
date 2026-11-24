#include "Arguments.h"
#include "Stdio.h"
#include "generated/Generated.h"

Arguments::Arguments (const jam::Strings& arguments,
                      const std::vector<std::string_view>& flagOptions,
                      const std::vector<std::string_view>& valueOptions,
                      const std::vector<std::string_view>& optionalValueOptions)
{
    int index { firstOptionIndex };

    while (index < arguments.size() and arguments.getView (index).compare (Chars::doubleDash) != 0)
        index = addToken (arguments, index, arguments.getView (index), flagOptions, valueOptions, optionalValueOptions);

    for (int positionalIndex { index + 1 }; positionalIndex < arguments.size(); ++positionalIndex)
        positionals.add (arguments.getView (positionalIndex));
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

juce::String Arguments::getOptionName (std::string_view option)
{
    return jam::Strings::fromView (option.substr (0, option.find (static_cast<char> (Chars::pipe))));
}

std::pair<juce::Result, juce::Range<int>> Arguments::getLineRange (const juce::String& rangeText,
                                                                   bool allowZeroStart)
{
    if (not rangeText.containsChar (Chars::colon))
    {
        const auto [isValid, lineNumber] { getParsedLineNumber (rangeText, allowZeroStart ? 0 : 1) };

        if (isValid and lineNumber < openRangeEnd)
            return { juce::Result::ok(), juce::Range<int> (lineNumber, lineNumber + 1) };

        return { juce::Result::fail (
                     Stdio::getFormatted (text::Diagnostics::failLineRangeInvalid,
                                          rangeText.toRawUTF8())),
                 {} };
    }

    const auto preColon { jam::Format::getPreColon (rangeText) };
    const auto postColon { jam::Format::getPostColon (rangeText) };
    const auto hasValidHalves { not postColon.containsChar (Chars::colon)
                                and (preColon.isNotEmpty() or postColon.isNotEmpty()) };
    const auto [startValid, startLine] { preColon.isEmpty()
                                             ? std::pair<bool, int> { true, 1 }
                                             : getParsedLineNumber (preColon, 1) };
    const auto [endValid, endLine] { postColon.isEmpty()
                                         ? std::pair<bool, int> { true, openRangeEnd }
                                         : getParsedLineNumber (postColon, startLine) };

    if (hasValidHalves and startValid and endValid)
    {
        const auto endExclusive { (postColon.isEmpty() or endLine == openRangeEnd) ? openRangeEnd : endLine + 1 };
        return { juce::Result::ok(), juce::Range<int> (startLine, endExclusive) };
    }

    return { juce::Result::fail (
                 Stdio::getFormatted (text::Diagnostics::failLineRangeInvalid,
                                      rangeText.toRawUTF8())),
             {} };
}

const juce::Result& Arguments::getStatus() const noexcept
{
    return status;
}

bool Arguments::contains (std::string_view option) const
{
    return containsOption (matchedFlags, option) or containsOption (matchedValueOptions, option);
}

std::string_view Arguments::getValueView (std::string_view option) const
{
    for (size_t index { matchedValueOptions.size() }; index > 0; --index)
    {
        if (matchedValueOptions.at (index - 1).compare (option) == 0)
            return matchedValues.getView (static_cast<int> (index - 1));
    }

    return {};
}

juce::String Arguments::getValue (std::string_view option) const
{
    return jam::Strings::fromView (getValueView (option));
}

jam::Strings Arguments::getValues (std::string_view option) const
{
    jam::Strings values;

    for (size_t index { 0 }; index < matchedValueOptions.size(); ++index)
    {
        if (matchedValueOptions.at (index).compare (option) == 0)
            values.add (matchedValues.getView (static_cast<int> (index)));
    }

    return values;
}

const jam::Strings& Arguments::getPositionals() const noexcept
{
    return positionals;
}

std::pair<juce::Result, int> Arguments::getInteger (std::string_view option, int minimumValue, int defaultValue) const
{
    if (not contains (option))
        return { juce::Result::ok(), defaultValue };

    const auto rawValue { getValue (option) };
    const auto [isValid, parsedValue] { getParsedLineNumber (rawValue, minimumValue) };

    if (isValid)
        return { juce::Result::ok(), parsedValue };

    return { juce::Result::fail (
                 Stdio::getFormatted (text::Diagnostics::failIntegerInvalid,
                                      getOptionName (option).toRawUTF8(),
                                      rawValue.toRawUTF8())),
             0 };
}

std::pair<juce::Result, juce::Range<int>> Arguments::getLineRangeFilter() const
{
    if (contains (Id::linesOption))
        return getLineRange (getValue (Id::linesOption), false);

    return { juce::Result::ok(), juce::Range<int> (1, openRangeEnd) };
}

bool Arguments::containsOption (const std::vector<std::string_view>& matchedOptions, std::string_view option)
{
    return std::any_of (matchedOptions.begin(),
                        matchedOptions.end(),
                        [option] (std::string_view matchedOption) { return matchedOption.compare (option) == 0; });
}

int Arguments::getMatchingOptionIndex (std::string_view argument,
                                       const std::vector<std::string_view>& optionList)
{
    for (size_t index { 0 }; index < optionList.size(); ++index)
    {
        if (matchesOption (argument, optionList.at (index)))
            return static_cast<int> (index);
    }

    return -1;
}

bool Arguments::isOptionArgument (std::string_view argument) noexcept
{
    return argument.size() > 1
           and argument.at (0) == static_cast<char> (Chars::dash)
           and not (argument.at (1) >= static_cast<char> (Chars::zero)
                    and argument.at (1) <= static_cast<char> (Chars::nine));
}

std::pair<bool, int> Arguments::getParsedLineNumber (const juce::String& token, int minimumValue)
{
    const auto value { token.getIntValue() };
    const auto isValid { token.isNotEmpty()
                         and juce::String (value).compare (token) == 0
                         and value >= minimumValue };
    return { isValid, isValid ? value : 0 };
}

int Arguments::addToken (const jam::Strings& arguments,
                         int index,
                         std::string_view token,
                         const std::vector<std::string_view>& flagOptions,
                         const std::vector<std::string_view>& valueOptions,
                         const std::vector<std::string_view>& optionalValueOptions)
{
    const auto flagIndex { getMatchingOptionIndex (token, flagOptions) };
    const auto valueIndex { getMatchingOptionIndex (token, valueOptions) };
    const auto optionalValueIndex { getMatchingOptionIndex (token, optionalValueOptions) };

    if (flagIndex >= 0 or matchesOption (token, Id::helpOption))
    {
        matchedFlags.push_back (flagIndex >= 0 ? flagOptions.at (static_cast<size_t> (flagIndex)) : Id::helpOption);
        return index + 1;
    }

    if (valueIndex >= 0)
        return addValue (arguments, index, valueOptions.at (static_cast<size_t> (valueIndex)), token);

    if (optionalValueIndex >= 0)
    {
        const bool isNextStdin { index + 1 < arguments.size()
                                 and matchesOption (arguments.getView (index + 1), Id::stdinOption) };

        if (isNextStdin or contains (Id::stdinOption))
        {
            matchedFlags.push_back (optionalValueOptions.at (static_cast<size_t> (optionalValueIndex)));
            return index + 1;
        }

        return addValue (arguments, index, optionalValueOptions.at (static_cast<size_t> (optionalValueIndex)), token);
    }

    const bool isShortFlagGroup { token.size() > 2
                                  and token.at (1) != static_cast<char> (Chars::dash)
                                  and isOptionArgument (token) };

    if (isShortFlagGroup)
        return addShortFlags (arguments, index, flagOptions, valueOptions, optionalValueOptions);

    if (isOptionArgument (token))
        return addFailure (text::Diagnostics::failFlagUnknown, token, index + 1);

    positionals.add (token);
    return index + 1;
}

int Arguments::addShortFlags (const jam::Strings& arguments,
                              int index,
                              const std::vector<std::string_view>& flagOptions,
                              const std::vector<std::string_view>& valueOptions,
                              const std::vector<std::string_view>& optionalValueOptions)
{
    const auto argument { arguments.getView (index) };

    for (size_t letterIndex { 1 }; letterIndex < argument.size(); ++letterIndex)
    {
        std::string letterToken { static_cast<char> (Chars::dash) };
        letterToken.push_back (argument.at (letterIndex));
        const std::string_view letterView { letterToken };
        const bool isFlagLetter { getMatchingOptionIndex (letterView, flagOptions) >= 0
                                  or matchesOption (letterView, Id::helpOption) };
        const bool isValueLetter { letterIndex + 1 == argument.size()
                                   and (getMatchingOptionIndex (letterView, valueOptions) >= 0
                                        or getMatchingOptionIndex (letterView, optionalValueOptions) >= 0) };

        if (isValueLetter)
            return addToken (arguments, index, letterView, flagOptions, valueOptions, optionalValueOptions);

        if (not isFlagLetter)
            return addFailure (text::Diagnostics::failFlagUnknown, argument, index + 1);

        addToken (arguments, index, letterView, flagOptions, valueOptions, optionalValueOptions);
    }

    return index + 1;
}

int Arguments::addValue (const jam::Strings& arguments,
                         int index,
                         std::string_view option,
                         std::string_view token)
{
    if (index + 1 < arguments.size())
    {
        matchedValueOptions.push_back (option);
        matchedValues.add (arguments.getView (index + 1));
        return index + 2;
    }

    return addFailure (text::Diagnostics::failFlagValueMissing, token, index + 1);
}

int Arguments::addFailure (const char* format, std::string_view token, int nextIndex)
{
    if (status.wasOk())
        status = juce::Result::fail (Stdio::getFormatted (format, jam::Strings::fromView (token).toRawUTF8()));

    return nextIndex;
}
