#pragma once
#include <JuceHeader.h>
#include "Arguments.h"

/** @brief The options of saw json, parsed one time from the arguments of the verb.
 *
 *  The verb and its helpers read these fields, never the Arguments. The views point into the
 *  argument list, so the argument list must outlive this object.
 */
class JsonOptions
{
public:
    /** @brief Largest indentation that --indent accepts. */
    static constexpr int maximumIndent { 7 };

    /** @brief Indentation of the output when --indent is absent. */
    static constexpr int defaultIndent { 2 };

    /** @brief Exit code of a filter whose last output is false or null, with -e. */
    static constexpr int exitFalseOrNull { 1 };

    /** @brief Exit code of a missing input file, or of a usage error. */
    static constexpr int exitInputOpen { 2 };

    /** @brief Exit code of a compile error of the filter. */
    static constexpr int exitCompile { 3 };

    /** @brief Exit code of -e when the filter gives no output. */
    static constexpr int exitNoOutput { 4 };

    /** @brief Exit code of a parse error of an input, of a runtime error, or of halt_error. */
    static constexpr int exitRuntime { 5 };

    /** @brief Reads every option from the parsed arguments, and records the first failure of the argument parse and of the integer options.
     *
     *  @param arguments The parsed arguments of the verb.
     */
    explicit JsonOptions (const Arguments& arguments);

    /** @brief Returns the first failure of the option values, or ok.
     *
     *  @return The status of the parse of the arguments and of the integer options.
     */
    const juce::Result& getStatus() const noexcept;

    /** @brief True when the null-input option is given. */
    const bool isNullInput;
    /** @brief True when the raw-input option is given. */
    const bool isRawInput;
    /** @brief True when the slurp option is given. */
    const bool isSlurp;
    /** @brief True when the compact option is given. */
    const bool isCompact;
    /** @brief True when raw output is requested by the raw-output option, the raw-output-0 option, or the join-output option. */
    const bool isRawOutput;
    /** @brief True when the raw-output-0 option is given. */
    const bool isRawOutput0;
    /** @brief True when the join-output option is given. */
    const bool isJoinOutput;
    /** @brief True when the ascii option is given. */
    const bool isAscii;
    /** @brief True when the sort-keys option is given. */
    const bool isSortedKeys;
    /** @brief True when the colour option is given. */
    const bool isColourRequested;
    /** @brief True when the monochrome option is given. */
    const bool isMonochrome;
    /** @brief True when the tab option is given, so that the output is indented with tabs. */
    const bool isTab;
    /** @brief The indentation of the output, from the indent option, or defaultIndent when the option is absent or invalid. */
    const int indent;
    /** @brief True when the stream option or the stream-errors option is given. */
    const bool isStream;
    /** @brief True when the stream-errors option is given. */
    const bool isStreamErrors;
    /** @brief True when the seq option is given. */
    const bool isSeq;
    /** @brief True when the from-file option is given, so that the filter is read from a file. */
    const bool isFromFile;
    /** @brief True when the exit-status option is given. */
    const bool isExitStatus;
    /** @brief True when the version option is given. */
    const bool isVersion;
    /** @brief True when the build-configuration option is given. */
    const bool isBuildConfiguration;
    /** @brief True when the run-tests option is given. */
    const bool isRunTests;
    /** @brief The number of results that the skip option drops first, or 0 when the option is absent or invalid. */
    const int skipCount;
    /** @brief The number of results that the take option keeps, or openRangeEnd when the option is absent or invalid. */
    const int takeCount;
    /** @brief The filter text, or the file that holds it, which is the first positional. It is empty when there is none. */
    const std::string_view filterArgument;
    /** @brief The positionals that have no mode, after the filter. They name the input files. */
    const jam::Array<std::string_view> inputFiles;
    /** @brief The positionals of the args mode, which are the string arguments of the filter. */
    const jam::Array<std::string_view> stringArguments;
    /** @brief The positionals of the jsonargs mode, which are the JSON arguments of the filter. */
    const jam::Array<std::string_view> jsonArguments;
    /** @brief The values of the library-path option, in the order given. */
    const juce::Span<const std::string_view> libraryPaths;
    /** @brief The values of the named-argument option, in the order given. */
    const juce::Span<const std::string_view> argNamedValues;
    /** @brief The values of the named JSON-argument option, in the order given. */
    const juce::Span<const std::string_view> argJsonNamedValues;
    /** @brief The values of the slurp-file option, in the order given. */
    const juce::Span<const std::string_view> slurpFileNamedValues;
    /** @brief The values of the raw-file option, in the order given. */
    const juce::Span<const std::string_view> rawFileNamedValues;

private:
    /** @brief The first failure of the argument parse, of the indent option, of the skip option, or of the take option, or ok. */
    const juce::Result status;
};
