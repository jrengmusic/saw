#pragma once
#include <JuceHeader.h>
#include "Arguments.h"
#include "Stdio.h"

/** @brief Implements the `saw replace` subcommand for literal and regex search-and-replace.
 */
struct Replace
{
    /** @brief Parses the argument list of replace, with its flags and value options.
     *
     *  @param argumentList The argument list, with the verb name at index 0.
     *
     *  @return The parsed arguments. Their status holds any parse failure.
     */
    static Arguments getArguments (const jam::Strings& argumentList);

    /** @brief Runs replace on parsed arguments.
     *
     *  A failed parse is returned as a failure. The replacement inputs and the paths are then
     *  validated, and the replacement is previewed, or applied with `--apply`.
     *
     *  @param arguments The parsed arguments from getArguments.
     *  @param input     The supplied input text, or nullopt for the process standard input.
     *
     *  @return The exit code, the output bytes, and the diagnostics.
     */
    static Stdio::Output run (const Arguments& arguments, const std::optional<std::string>& input);
};
