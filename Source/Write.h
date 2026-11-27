#pragma once
#include <JuceHeader.h>
#include "Arguments.h"
#include "Input.h"
#include "Stdio.h"

/** @brief Implements the `saw write` subcommand, which writes the input to one file through a transaction. */
struct Write
{
    /** @brief Parses the argument list of write, with its apply flag and its expect value option.
     *
     *  @param argumentList The argument list, with the verb name at index 0.
     *
     *  @return The parsed arguments. Their status holds any parse failure.
     */
    static Arguments getArguments (juce::Span<const std::string_view> argumentList);

    /** @brief Runs write on parsed arguments: one path names the target file, and the input supplies the text.
     *
     *  A missing or extra path is returned as a failure, and binary input is refused. The write is previewed, or applied with apply.
     *
     *  @param arguments The parsed arguments from getArguments.
     *  @param input     The source of the text to write: the standard input, the null device, or a supplied text.
     *
     *  @return The exit code, the output documents, and the failures.
     */
    static Stdio::Output run (const Arguments& arguments, const Input::Source& input);
};
