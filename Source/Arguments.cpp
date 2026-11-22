#include "Arguments.h"
#include "generated/Generated.h"

Arguments::Arguments (const jam::Strings& arguments,
                      int startIndex,
                      const jam::Strings& flagOptions,
                      const jam::Strings& valueOptions)
{
    const auto terminatorIndex { getTerminatorIndex (arguments, startIndex) };

    for (int index { startIndex }; index < terminatorIndex and status.wasOk();)
    {
        const auto argument { arguments.at (index) };
        const auto flagIndex { getMatchingOptionIndex (argument, flagOptions) };
        const auto valueIndex { getMatchingOptionIndex (argument, valueOptions) };

        if (flagIndex >= 0)
        {
            matchedFlags.addIfNotAlreadyThere (flagOptions.at (flagIndex), false);
            ++index;
        }
        else if (valueIndex >= 0)
        {
            if (index + 1 < terminatorIndex)
            {
                matchedValues.add ({ valueOptions.at (valueIndex), arguments.at (index + 1) });
                index += 2;
            }
            else
            {
                status = juce::Result::fail (
                    juce::String::formatted (text::Diagnostics::failFlagValueMissing,
                                             argument.toRawUTF8()));
            }
        }
        else
        {
            if (isOptionArgument (argument))
            {
                status = juce::Result::fail (
                    juce::String::formatted (text::Diagnostics::failFlagUnknown,
                                             argument.toRawUTF8()));
            }
            else
            {
                positionals.add (argument);
                ++index;
            }
        }
    }

    for (int index { terminatorIndex + 1 }; status.wasOk() and index < arguments.size(); ++index)
        positionals.add (arguments.at (index));
}

bool Arguments::matchesOption (const juce::String& argument, const juce::String& option)
{
    const auto tokens (jam::Strings::fromTokens (option, juce::String::charToString (Chars::pipe), {}));

    for (int index { 0 }; index < tokens.size(); ++index)
    {
        if (argument.compare (tokens.at (index)) == 0)
            return true;
    }

    return false;
}

bool Arguments::containsOptionBeforeTerminator (const jam::Strings& arguments,
                                                int startIndex,
                                                const juce::String& option)
{
    const auto terminatorIndex { getTerminatorIndex (arguments, startIndex) };

    for (int index { startIndex }; index < terminatorIndex; ++index)
    {
        if (matchesOption (arguments.at (index), option))
            return true;
    }

    return false;
}

std::pair<juce::Result, juce::Range<int>> Arguments::getLineRange (const juce::String& rangeText,
                                                                   bool allowZeroStart)
{
    if (not rangeText.containsChar (Chars::colon))
    {
        const auto [isValid, lineNumber] { getParsedLineNumber (rangeText, allowZeroStart ? 0 : 1) };

        if (isValid)
            return { juce::Result::ok(), juce::Range<int> (lineNumber, lineNumber + 1) };

        return { juce::Result::fail (
                     juce::String::formatted (text::Diagnostics::failLineRangeInvalid,
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
        const auto endExclusive { postColon.isEmpty() ? openRangeEnd : endLine + 1 };
        return { juce::Result::ok(), juce::Range<int> (startLine, endExclusive) };
    }

    return { juce::Result::fail (
                 juce::String::formatted (text::Diagnostics::failLineRangeInvalid,
                                          rangeText.toRawUTF8())),
             {} };
}

const juce::Result& Arguments::getStatus() const noexcept
{
    return status;
}

bool Arguments::contains (const juce::String& option) const
{
    if (matchedFlags.contains (option, false))
        return true;

    for (int index { 0 }; index < matchedValues.size(); ++index)
    {
        const auto& [matchedOption, matchedValue] { matchedValues.at (index) };

        if (matchedOption.compare (option) == 0)
            return true;
    }

    return false;
}

juce::String Arguments::getValue (const juce::String& option) const
{
    for (int index { matchedValues.size() - 1 }; index >= 0; --index)
    {
        const auto& [matchedOption, matchedValue] { matchedValues.at (index) };

        if (matchedOption.compare (option) == 0)
            return matchedValue;
    }

    return {};
}

jam::Strings Arguments::getValues (const juce::String& option) const
{
    jam::Strings values;

    for (int index { 0 }; index < matchedValues.size(); ++index)
    {
        const auto& [matchedOption, matchedValue] { matchedValues.at (index) };

        if (matchedOption.compare (option) == 0)
            values.add (matchedValue);
    }

    return values;
}

const jam::Strings& Arguments::getPositionals() const noexcept
{
    return positionals;
}

std::pair<juce::Result, int> Arguments::getInteger (const juce::String& option,
                                                    int minimumValue) const
{
    const auto rawValue { getValue (option) };
    const auto [isValid, parsedValue] { getParsedLineNumber (rawValue, minimumValue) };

    if (isValid)
        return { juce::Result::ok(), parsedValue };

    return { juce::Result::fail (
                 juce::String::formatted (text::Diagnostics::failIntegerInvalid,
                                          option.toRawUTF8(),
                                          rawValue.toRawUTF8())),
             0 };
}

std::pair<juce::Result, juce::Range<int>> Arguments::getLineRangeFilter() const
{
    if (contains (Id::linesOption))
        return getLineRange (getValue (Id::linesOption), false);

    return { juce::Result::ok(), juce::Range<int> (1, openRangeEnd) };
}

int Arguments::getMatchingOptionIndex (const juce::String& argument,
                                       const jam::Strings& optionList)
{
    for (int index { 0 }; index < optionList.size(); ++index)
    {
        if (matchesOption (argument, optionList.at (index)))
            return index;
    }

    return -1;
}

int Arguments::getTerminatorIndex (const jam::Strings& arguments, int startIndex)
{
    for (int index { startIndex }; index < arguments.size(); ++index)
    {
        if (arguments.at (index).compare (Chars::doubleDash) == 0)
            return index;
    }

    return arguments.size();
}

bool Arguments::isOptionArgument (const juce::String& argument) noexcept
{
    return argument.startsWithChar (Chars::dash)
           and argument.length() > 1
           and not (argument[1] >= Chars::zero and argument[1] <= Chars::nine);
}

std::pair<bool, int> Arguments::getParsedLineNumber (const juce::String& token, int minimumValue)
{
    const auto value { token.getIntValue() };
    const auto isValid { token.isNotEmpty()
                         and juce::String (value).compare (token) == 0
                         and value >= minimumValue };
    return { isValid, isValid ? value : 0 };
}
