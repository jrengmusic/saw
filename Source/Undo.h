#pragma once
#include <JuceHeader.h>

/** @brief Implements the `saw undo` subcommand for listing, previewing, and restoring transactions.
 */
struct Undo
{
    /** @brief Executes `saw undo` to list transactions, preview a revert diff, or restore backup blobs.
     *
     *  @param argumentList Command arguments starting with `undo` at index 0.
     *
     *  @return `Stdio::exitSuccess` on success, or `Stdio::exitError` on drift or I/O failure.
     */
    static int run (const jam::Strings& argumentList);
};
