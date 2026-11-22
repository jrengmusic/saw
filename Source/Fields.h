#pragma once
#include <JuceHeader.h>

/** @brief Implements the `saw fields` subcommand for extracting delimited columns from lines.
 */
struct Fields
{
    /** @brief Executes `saw fields` with the given command arguments.
     *
     *  @param argumentList Command arguments starting with `fields` at index 0.
     *
     *  @return `Stdio::exitSuccess`, `Stdio::exitMismatch` when `--match` matches nothing, or `Stdio::exitError`.
     */
    static int run (const jam::Strings& argumentList);
};
