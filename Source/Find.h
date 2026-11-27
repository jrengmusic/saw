#pragma once
#include <JuceHeader.h>
#include "Arguments.h"
#include "Input.h"
#include "Stdio.h"

/** @brief Implements the `saw find` subcommand for searching files and standard input.
 */
struct Find
{
    /** @brief Parses the argument list of find, with its flags and value options.
     *
     *  @param argumentList The argument list, with the verb name at index 0.
     *
     *  @return The parsed arguments. Their status holds any parse failure.
     */
    static Arguments getArguments (juce::Span<const std::string_view> argumentList);

    /** @brief Runs find on parsed arguments.
     *
     *  A failed parse is returned as a failure. With `--files`, the command lists the files instead of searching them.
     *  Without paths, standard input is searched when it holds data, see Input::hasData. Otherwise the current folder is searched.
     *
     *  @param arguments The parsed arguments from getArguments.
     *  @param source    The standard input, the null device, or a supplied text.
     *
     *  @return The exit code, the output documents, and the failures. For a file search, the exit code is Stdio::exitError when a failure
     *          is recorded, except that --quiet with a match gives Stdio::exitSuccess. Otherwise it is Stdio::exitSuccess when a match
     *          is found, and Stdio::exitMismatch when none is found.
     */
    static Stdio::Output run (const Arguments& arguments, const Input::Source& source);
};
