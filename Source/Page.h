#pragma once
#include <JuceHeader.h>
#include "Arguments.h"
#include "Input.h"
#include "Stdio.h"

/** @brief Implements the `saw page` subcommand for showing the text of files, with the page options. */
struct Page
{
    /** @brief Parses the argument list of page, with the page options.
     *
     *  @param argumentList The argument list, with the verb name at index 0.
     *
     *  @return The parsed arguments. Their status holds any parse failure.
     */
    static Arguments getArguments (juce::Span<const std::string_view> argumentList);

    /** @brief Runs page: a one-shot option is answered first, then an options failure is returned, and otherwise the pages are shown.
     *
     *  The configuration is read before the options are resolved, and the command line answers first for each option.
     *
     *  @param arguments The parsed arguments from getArguments.
     *  @param input     The source of the input, read when no file argument names it.
     *
     *  @return The exit code, the output documents, and the failures.
     */
    static Stdio::Output run (const Arguments& arguments, const Input::Source& input);
};
