#pragma once
#include <JuceHeader.h>
#include "Stdio.h"

/** @brief Entry point that runs one saw command line for every caller.
 *
 *  The program and the tool server both call this entry point, so that
 *  every caller gets the same verbs, the same help, and the same exit codes.
 */
struct CommandLine
{
    /** @brief Runs one command line and returns its result without writing to a process stream.
     *
     *  The first argument is the verb. An empty list prints the banner and the help.
     *  The global options `--help` and `--version` are handled before a verb is matched.
     *
     *  @param arguments The argument list, with the verb at index 0.
     *  @param input     The standard input, the null device, or a supplied text.
     *
     *  @return The exit code, the output documents, and the failures.
     */
    static Stdio::Output run (juce::Span<const std::string_view> arguments, const Input::Source& input);
};
