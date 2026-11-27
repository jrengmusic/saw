#pragma once
#include <JuceHeader.h>
#include "Arguments.h"
#include "Stdio.h"

/** @brief Prints verbatim lines of many targets in one call.
 *
 *  Each target is read in parallel, so one call replaces one command per target.
 */
struct Excerpt
{
    /** @brief Parses the argument list of excerpt, with its text flag and its context value option.
     *
     *  @param argumentList The argument list, with the verb name at index 0.
     *
     *  @return The parsed arguments. Their status holds any parse failure.
     */
    static Arguments getArguments (juce::Span<const std::string_view> argumentList);

    /** @brief Runs excerpt on parsed arguments.
     *
     *  A failed parse, or an invalid context count, is returned as a failure. An input with no target is returned as a failure.
     *  Each target is reported in its own block, so one missing target does not hide the others.
     *
     *  @param arguments The parsed arguments from getArguments.
     *  @param input     The standard input, the null device, or a supplied text. It holds one target per line when the arguments name none.
     *
     *  @return The exit code, the output documents, and the failures. The exit code is Stdio::exitSuccess when every target gives lines,
     *          Stdio::exitMismatch when one does not, and the failure code otherwise.
     */
    static Stdio::Output run (const Arguments& arguments, const Input::Source& input);
};
