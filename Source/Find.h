#pragma once
#include <JuceHeader.h>

/** @brief Implements the `saw find` subcommand for searching files and standard input.
 */
struct Find
{
    /** @brief Executes `saw find` with the given command arguments.
     *
     *  @param argumentList Command arguments starting with `find` at index 0.
     *
     *  @return `Stdio::exitSuccess` on match, `Stdio::exitMismatch` on zero matches, or `Stdio::exitError`.
     */
    static int run (const jam::Strings& argumentList);
};
