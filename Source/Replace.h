#pragma once
#include <JuceHeader.h>

/** @brief Implements the `saw replace` subcommand for literal and regex search-and-replace.
 */
struct Replace
{
    /** @brief Executes `saw replace` with the given command arguments.
     *
     *  @param argumentList Command arguments starting with `replace` at index 0.
     *
     *  @return `Stdio::exitSuccess`, `Stdio::exitMismatch` on `--expect` mismatch, or `Stdio::exitError`.
     */
    static int run (const jam::Strings& argumentList);
};
