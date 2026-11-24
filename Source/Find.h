#pragma once
#include <JuceHeader.h>
#include "Arguments.h"
#include "Stdio.h"

/** @brief Implements the `saw find` subcommand for searching files and standard input.
 */
struct Find
{
    /** @brief Parses the argument list of find, with its flags and value options.
     *
     *  @param argumentList The argument list, with the verb name at index 0.
     *
     *  @return The parsed arguments. Their status holds any parse failure.
     */
    static Arguments getArguments (const jam::Strings& argumentList);

    /** @brief Runs find on parsed arguments.
     *
     *  A failed parse is returned as a failure. With `--files`, the command lists the files
     *  instead of searching them. Without paths, standard input is searched when it is not a terminal.
     *
     *  @param arguments The parsed arguments from getArguments.
     *  @param input     The supplied input text, or nullopt for the process standard input.
     *
     *  @return The exit code, the output bytes, and the diagnostics. The exit code is 0 when a
     *          match is found, 1 when none is found, and 2 on a failure.
     */
    static Stdio::Output run (const Arguments& arguments, const std::optional<std::string>& input);
};
