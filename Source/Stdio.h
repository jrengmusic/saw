#pragma once
#include <JuceHeader.h>

/** @brief Process exit codes and raw standard input/output helpers.
 */
struct Stdio
{
    /** @brief Exit code when a command succeeds or finds matches.
     */
    static constexpr int exitSuccess { 0 };

    /** @brief Exit code when find matches nothing or --expect disagrees with actual edits.
     */
    static constexpr int exitMismatch { 1 };

    /** @brief Exit code when argument parsing, validation, I/O, or a transaction fails.
     */
    static constexpr int exitError { 2 };

    /** @brief Switches standard output to binary mode on Windows.
     *
     *  Prevents newline translation from altering LF or CRLF byte streams.
     */
    static void setBinaryOutput() noexcept;

    /** @brief Checks whether standard input is connected to an interactive terminal.
     *
     *  @return True when stdin is a TTY.
     */
    static bool isTerminalInput() noexcept;

    /** @brief Checks whether standard output is connected to an interactive terminal.
     *
     *  @return True when stdout is a TTY.
     */
    static bool isTerminalOutput() noexcept;

    /** @brief Prints one UTF-8 line to standard output in binary mode.
     *
     *  @param message Line text without a trailing newline.
     */
    static void print (const juce::String& message);

    /** @brief Prints a prefixed summary line to standard output.
     *
     *  @param message Summary text appended after the tool name prefix.
     */
    static void printSummary (const juce::String& message);

    /** @brief Prints a prefixed error line to standard error.
     *
     *  @param message Diagnostic text appended after the tool name prefix.
     */
    static void printError (const juce::String& message);

    /** @brief Writes exact raw bytes to standard output in binary mode.
     *
     *  @param text Byte sequence to write without adding a newline.
     */
    static void write (std::string_view text);

    /** @brief Formats a printf-style UTF-8 string using two-pass sizing.
     *
     *  @param format Null-terminated printf format string; must not be null.
     *
     *  @return Formatted UTF-8 string, or empty when formatting yields zero bytes.
     */
    static juce::String getFormatted (const char* format, ...);

    /** @brief Reads all bytes from standard input in binary mode until EOF.
     *
     *  @return Status result paired with the bytes read from stdin.
     */
    static std::pair<juce::Result, std::string> getInput();
};
