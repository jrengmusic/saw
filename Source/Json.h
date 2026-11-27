#pragma once
#include <JuceHeader.h>
#include "Arguments.h"
#include "Input.h"
#include "Stdio.h"

/** @brief The json verb: takes the arguments and the filter language of jq 1.8.2, and its messages are saw's own text rows.
 *
 *  readFilter compiles the filter and the tostream filter, and runCompiled runs the compiled filter.
 */
struct Json
{
    /** @brief Returns the parsed arguments of the verb, with the option table of the jq 1.8.2 options.
     *
     *  The table also holds the verb options --run-tests and --build-configuration.
     *
     *  @param argumentList The argument list, with the verb name at index 0.
     *
     *  @return The parsed arguments.
     */
    static Arguments getArguments (juce::Span<const std::string_view> argumentList);

    /** @brief Runs the verb: the test runner for --run-tests, the version or build information, or the filter.
     *
     *  The dispatch order is fixed: --run-tests first, then --version or --build-configuration, then the filter.
     *
     *  @param arguments The parsed arguments of the verb.
     *  @param input     The source of the input values. It is read when no file argument names the input.
     *
     *  @return The exit code, the standard output, and the errors of the command.
     */
    static Stdio::Output run (const Arguments& arguments, const Input::Source& input);
};
