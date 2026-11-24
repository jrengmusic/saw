#pragma once
#include <JuceHeader.h>
#include "Arguments.h"
#include "Stdio.h"

/** @brief Implements the `saw fields` subcommand for extracting delimited columns from lines.
 */
struct Fields
{
    /** @brief Parses the argument list of fields, with its flags and value options.
     *
     *  @param argumentList The argument list, with the verb name at index 0.
     *
     *  @return The parsed arguments. Their status holds any parse failure.
     */
    static Arguments getArguments (const jam::Strings& argumentList);

    /** @brief Runs fields on parsed arguments.
     *
     *  The field settings and the filter settings are checked first. A failed check is returned
     *  as a failure. Without paths, standard input is read.
     *
     *  @param arguments The parsed arguments from getArguments.
     *  @param input     The supplied input text, or nullopt for the process standard input.
     *
     *  @return The exit code, the output bytes, and the diagnostics.
     */
    static Stdio::Output run (const Arguments& arguments, const std::optional<std::string>& input);
};
