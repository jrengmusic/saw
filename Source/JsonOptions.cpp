#include "JsonOptions.h"
#include "Stdio.h"
#include "generated/Generated.h"

static std::pair<juce::Result, int> getIndentValue (const Arguments& arguments)
{
    const auto [integerStatus, indentValue] { arguments.getInteger (Id::jsonIndentOption, 0, JsonOptions::defaultIndent) };

    if (integerStatus.failed())
        return { integerStatus, JsonOptions::defaultIndent };

    if (indentValue > JsonOptions::maximumIndent)
        return { Stdio::fail (text::Diagnostics::failJsonIndentRange, JsonOptions::maximumIndent), JsonOptions::defaultIndent };

    return { juce::Result::ok(), indentValue };
}

static std::pair<juce::Result, int> getCountValue (const Arguments& arguments, std::string_view option, int defaultValue)
{
    return arguments.getInteger (option, 0, defaultValue);
}

static int getIndent (const Arguments& arguments)
{
    const auto [indentStatus, indentValue] { getIndentValue (arguments) };
    juce::ignoreUnused (indentStatus);
    return indentValue;
}

static int getSkipCount (const Arguments& arguments)
{
    const auto [skipStatus, skipValue] { getCountValue (arguments, Id::jsonSkipOption, 0) };
    juce::ignoreUnused (skipStatus);
    return skipValue;
}

static int getTakeCount (const Arguments& arguments)
{
    const auto [takeStatus, takeValue] { getCountValue (arguments, Id::jsonTakeOption, Arguments::openRangeEnd) };
    juce::ignoreUnused (takeStatus);
    return takeValue;
}

static juce::Result getParseStatus (const Arguments& arguments)
{
    if (arguments.getStatus().failed())
        return arguments.getStatus();

    const auto [indentStatus, indentValue] { getIndentValue (arguments) };
    juce::ignoreUnused (indentValue);

    if (indentStatus.failed())
        return indentStatus;

    const auto [skipStatus, skipValue] { getCountValue (arguments, Id::jsonSkipOption, 0) };
    juce::ignoreUnused (skipValue);

    if (skipStatus.failed())
        return skipStatus;

    const auto [takeStatus, takeValue] { getCountValue (arguments, Id::jsonTakeOption, Arguments::openRangeEnd) };
    juce::ignoreUnused (takeValue);
    return takeStatus;
}

static jam::Array<std::string_view> getModePositionals (const Arguments& arguments, std::string_view modeOption)
{
    jam::Array<std::string_view> values;
    const auto& positionals { arguments.getPositionals() };

    for (int index { 1 }; index < positionals.size(); ++index)
        if (Arguments::matchesOption (arguments.getPositionalMode (index), modeOption))
            values.add (positionals.at (index));

    return values;
}

static jam::Array<std::string_view> getInputFiles (const Arguments& arguments)
{
    jam::Array<std::string_view> files;
    const auto& positionals { arguments.getPositionals() };

    for (int index { 1 }; index < positionals.size(); ++index)
        if (arguments.getPositionalMode (index).empty())
            files.add (positionals.at (index));

    return files;
}

static std::string_view getFilterArgument (const Arguments& arguments)
{
    if (not arguments.getPositionals().isEmpty())
        return arguments.getPositionals().at (0);

    return {};
}

JsonOptions::JsonOptions (const Arguments& arguments)
    : isNullInput { arguments.contains (Id::jsonNullInputOption) }
    , isRawInput { arguments.contains (Id::jsonRawInputOption) }
    , isSlurp { arguments.contains (Id::jsonSlurpOption) }
    , isCompact { arguments.contains (Id::jsonCompactOption) }
    , isRawOutput { arguments.contains (Id::jsonRawOutputOption) or arguments.contains (Id::jsonRawOutput0Option) or arguments.contains (Id::jsonJoinOutputOption) }
    , isRawOutput0 { arguments.contains (Id::jsonRawOutput0Option) }
    , isJoinOutput { arguments.contains (Id::jsonJoinOutputOption) }
    , isAscii { arguments.contains (Id::jsonAsciiOption) }
    , isSortedKeys { arguments.contains (Id::jsonSortKeysOption) }
    , isColourRequested { arguments.contains (Id::jsonColourOption) }
    , isMonochrome { arguments.contains (Id::jsonMonochromeOption) }
    , isTab { arguments.contains (Id::jsonTabOption) }
    , indent { getIndent (arguments) }
    , isStream { arguments.contains (Id::jsonStreamOption) or arguments.contains (Id::jsonStreamErrorsOption) }
    , isStreamErrors { arguments.contains (Id::jsonStreamErrorsOption) }
    , isSeq { arguments.contains (Id::jsonSeqOption) }
    , isFromFile { arguments.contains (Id::jsonFromFileOption) }
    , isExitStatus { arguments.contains (Id::jsonExitStatusOption) }
    , isVersion { arguments.contains (Id::versionOption) }
    , isBuildConfiguration { arguments.contains (Id::jsonBuildConfigurationOption) }
    , isRunTests { arguments.contains (Id::jsonRunTestsOption) }
    , skipCount { getSkipCount (arguments) }
    , takeCount { getTakeCount (arguments) }
    , filterArgument { getFilterArgument (arguments) }
    , inputFiles { getInputFiles (arguments) }
    , stringArguments { getModePositionals (arguments, Id::jsonArgsOption) }
    , jsonArguments { getModePositionals (arguments, Id::jsonJsonArgsOption) }
    , libraryPaths { arguments.getValues (Id::jsonLibraryPathOption) }
    , argNamedValues { arguments.getValues (Id::jsonArgNamedOption) }
    , argJsonNamedValues { arguments.getValues (Id::jsonArgJsonOption) }
    , slurpFileNamedValues { arguments.getValues (Id::jsonSlurpFileOption) }
    , rawFileNamedValues { arguments.getValues (Id::jsonRawFileOption) }
    , status { getParseStatus (arguments) }
{
}

const juce::Result& JsonOptions::getStatus() const noexcept
{
    return status;
}
