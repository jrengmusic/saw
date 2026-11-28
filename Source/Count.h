#pragma once
#include <JuceHeader.h>
#include "Arguments.h"
#include "Input.h"
#include "Stdio.h"

/** @brief Implements the `saw count` subcommand for counting lines, words, and bytes.
 */
struct Count
{
    /** @brief Parses the argument list of count, with its line, word, and byte flags.
     *
     *  @param argumentList The argument list, with the verb name at index 0.
     *
     *  @return The parsed arguments. Their status holds any parse failure.
     */
    static Arguments getArguments (juce::Span<const std::string_view> argumentList);

    /** @brief Runs count on parsed arguments.
     *
     *  Without paths, reads standard input and refuses a terminal. With paths, counts each file
     *  and appends a total row when more than one file is counted.
     *
     *  @param arguments The parsed arguments from getArguments.
     *  @param input     The standard input, the null device, or a supplied text.
     *
     *  @return The exit code, the count output documents, and any failures.
     */
    static Stdio::Output run (const Arguments& arguments, const Input::Source& input);
};
