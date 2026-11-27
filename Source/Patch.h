#pragma once
#include <JuceHeader.h>
#include "Arguments.h"
#include "Stdio.h"

/** @brief Implements the `saw patch` subcommand for applying validated multi-file JSON patches.
 */
struct Patch
{
    /** @brief Parses the argument list of patch, with its flag and value options.
     *
     *  @param argumentList The argument list, with the verb name at index 0.
     *
     *  @return The parsed arguments. Their status holds any parse failure.
     */
    static Arguments getArguments (juce::Span<const std::string_view> argumentList);

    /** @brief Runs patch on parsed arguments.
     *
     *  A failed parse is returned as a failure. A positional argument is refused. The JSON is read
     *  from the file of `--file`, or else from standard input, which must not be a terminal.
     *
     *  @param arguments The parsed arguments from getArguments.
     *  @param input     The standard input, the null device, or a supplied text.
     *
     *  @return The exit code, the output text, and the diagnostics.
     */
    static Stdio::Output run (const Arguments& arguments, const Input::Source& input);
};
