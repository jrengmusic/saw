#pragma once
#include <JuceHeader.h>
#include "Arguments.h"
#include "Input.h"
#include "Stdio.h"

/** @brief Implements the `saw sort` subcommand for ordering, deduplicating, and counting lines.
 */
struct Sort
{
    /** @brief Parses the argument list of sort, with its numeric, reverse, unique, and count flags.
     *
     *  @param argumentList The argument list, with the verb name at index 0.
     *
     *  @return The parsed arguments. Their status holds any parse failure.
     */
    static Arguments getArguments (juce::Span<const std::string_view> argumentList);

    /** @brief Runs sort on parsed arguments.
     *
     *  Lines are viewed in place from the input buffers so no line text is copied during sorting.
     *
     *  @param arguments The parsed arguments from getArguments.
     *  @param input     The standard input, the null device, or a supplied text.
     *
     *  @return The exit code, the sorted line documents, and any failures.
     */
    static Stdio::Output run (const Arguments& arguments, const Input::Source& input);
};
