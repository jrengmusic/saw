#pragma once
#include <JuceHeader.h>

/** @brief Implements the `saw patch` subcommand for applying validated multi-file JSON patches.
 */
struct Patch
{
    /** @brief Executes `saw patch` with the given command arguments.
     *
     *  @param argumentList Command arguments starting with `patch` at index 0.
     *
     *  @return `Stdio::exitSuccess`, `Stdio::exitMismatch` on `--expect` mismatch, or `Stdio::exitError`.
     */
    static int run (const jam::Strings& argumentList);
};
