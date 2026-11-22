#pragma once
#include <JuceHeader.h>

/** @brief Implements the `saw lines` subcommand for reading and mutating line ranges.
 */
struct Lines
{
    /** @brief Executes `saw lines` in read mode or single-operation transaction mode.
     *
     *  @param argumentList Command arguments starting with `lines` at index 0.
     *
     *  @return `Stdio::exitSuccess`, `Stdio::exitMismatch` on `--expect` mismatch, or `Stdio::exitError`.
     */
    static int run (const jam::Strings& argumentList);
};
