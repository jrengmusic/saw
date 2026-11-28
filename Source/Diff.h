#pragma once
#include <JuceHeader.h>
#include "Arguments.h"
#include "Input.h"
#include "Stdio.h"

/** @brief Implements the `saw diff` subcommand for comparing two files or a file and standard input.
 */
struct Diff
{
    /** @brief Parses the argument list of diff, with its quiet flag.
     *
     *  @param argumentList The argument list, with the verb name at index 0.
     *
     *  @return The parsed arguments. Their status holds any parse failure.
     */
    static Arguments getArguments (juce::Span<const std::string_view> argumentList);

    /** @brief Runs diff on two positional targets.
     *
     *  Exits 0 when inputs are identical, 1 when they differ, and 2 on usage or read failure.
     *
     *  @param arguments The parsed arguments from getArguments.
     *  @param input     The standard input, the null device, or a supplied text.
     *
     *  @return The exit code, the unified diff document when non-quiet and differing, and any failures.
     */
    static Stdio::Output run (const Arguments& arguments, const Input::Source& input);
};
