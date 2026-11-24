#pragma once
#include <JuceHeader.h>
#include "Arguments.h"
#include "Stdio.h"

/** @brief Implements the `saw lines` subcommand for reading and mutating line ranges.
 */
struct Lines
{
    /** @brief Parses the argument list of lines, with its flags, value options, and optional-value options.
     *
     *  @param argumentList The argument list, with the verb name at index 0.
     *
     *  @return The parsed arguments. Their status holds any parse failure.
     */
    static Arguments getArguments (const jam::Strings& argumentList);

    /** @brief Runs lines on parsed arguments.
     *
     *  A line range that fails its check is returned as a failure. Without a mutating option, the
     *  lines are read. Otherwise the lines are changed.
     *
     *  @param arguments The parsed arguments from getArguments.
     *  @param input     The supplied input text, or nullopt for the process standard input.
     *
     *  @return The exit code, the output bytes, and the diagnostics.
     */
    static Stdio::Output run (const Arguments& arguments, const std::optional<std::string>& input);
};
