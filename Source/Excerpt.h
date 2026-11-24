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
    /** @brief Parses the argument list of excerpt, with its value option.
     *
     *  @param argumentList The argument list, with the verb name at index 0.
     *
     *  @return The parsed arguments. Their status holds any parse failure.
     */
    static Arguments getArguments (const jam::Strings& argumentList);

    /** @brief Runs excerpt on parsed arguments.
     *
     *  A failed parse, an invalid context count, or no target is returned as a failure. A target
     *  that gives no lines prints its state in its block, so one bad target does not hide the others.
     *
     *  @param arguments The parsed arguments from getArguments.
     *  @param input     The supplied input text, or nullopt for the process standard input. It
     *                   holds one target per line when the arguments name none.
     *
     *  @return The exit code, the output bytes, and the diagnostics. The exit code is 0 when every
     *          target gives lines, 1 when one does not, and 2 on a failure.
     */
    static Stdio::Output run (const Arguments& arguments, const std::optional<std::string>& input);
};
