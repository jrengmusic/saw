#pragma once
#include <JuceHeader.h>
#include "Arguments.h"
#include "Stdio.h"

/** @brief Implements the `saw undo` subcommand for listing, previewing, and restoring transactions.
 */
struct Undo
{
    /** @brief Parses the argument list of undo, with its flags.
     *
     *  @param argumentList The argument list, with the verb name at index 0.
     *
     *  @return The parsed arguments. Their status holds any parse failure.
     */
    static Arguments getArguments (juce::Span<const std::string_view> argumentList);

    /** @brief Runs undo on parsed arguments.
     *
     *  A failed parse is returned as a failure. With `--list`, the transactions are listed.
     *  Otherwise one transaction is previewed, or restored with `--apply`. The input is not read.
     *
     *  @param arguments The parsed arguments from getArguments.
     *  @param input     The supplied input text. Undo does not read it.
     *
     *  @return The exit code, the output text, and the diagnostics.
     */
    static Stdio::Output run (const Arguments& arguments, const std::optional<std::string>& input);
};
