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

    /** @brief Result of a command: exit code, standard output bytes, and standard error bytes.
     *
     *  A verb returns an Output, so that no verb writes to a process stream. The caller
     *  decides where the bytes go.
     */
    using Output = std::tuple<int, std::string, std::string>;

    /** @brief Checks whether standard output is connected to an interactive terminal.
     *
     *  @return True when stdout is a TTY.
     */
    static bool isTerminalOutput() noexcept;

    /** @brief Checks whether the input of a command is an interactive terminal.
     *
     *  Supplied input is never a terminal. Only the process standard input can be one.
     *
     *  @param input The supplied input, or nullopt when the process standard input is read.
     *
     *  @return True when no input is supplied and standard input is a TTY.
     */
    static bool isTerminalInput (const std::optional<std::string>& input);

    /** @brief Appends a newline to a message.
     *
     *  @param message The text of the line, without a newline.
     *
     *  @return The message bytes followed by one newline.
     */
    static std::string getLine (const juce::String& message);

    /** @brief Formats a diagnostic line with the program name as its prefix.
     *
     *  @param message The text of the diagnostic, without a newline.
     *
     *  @return The program name, the diagnostic separator, the message, and one newline.
     */
    static std::string getDiagnostic (const juce::String& message);

    /** @brief Builds the output of a failed command.
     *
     *  @param message The text of the diagnostic.
     *
     *  @return Exit code exitError, empty standard output, and the diagnostic on standard error.
     */
    static Output getFailure (const juce::String& message);

    /** @brief Writes both streams of an Output and returns the exit code.
     *
     *  A failed write of the standard output gives exitError, and the failure is reported on
     *  standard error. A closed pipe is not a failure.
     *
     *  @param output The command result to write.
     *
     *  @return The exit code of the command, or exitError when the standard output write fails.
     */
    static int write (const Output& output);

    /** @brief Writes text to standard error. A failed write is ignored.
     *
     *  The diagnostic channel does not change the exit code of the command.
     *
     *  @param text The bytes to write.
     */
    static void writeError (std::string_view text);

    /** @brief Formats a printf-style UTF-8 string using two-pass sizing.
     *
     *  @param format Null-terminated printf format string; must not be null.
     *
     *  @return Formatted UTF-8 string, or empty when formatting yields zero bytes.
     */
    static juce::String getFormatted (const char* format, ...);

    /** @brief Joins several output parts into one string.
     *
     *  The result is sized once, so the join makes one allocation.
     *
     *  @param outputs The parts, in the order they are joined.
     *
     *  @return The concatenation of all parts.
     */
    static std::string getJoinedOutput (const jam::Array<std::string>& outputs);

private:
    /** @brief Writes exact raw bytes to standard output.
     *
     *  @param bytes Byte sequence to write without adding a newline.
     *
     *  @return Ok when every byte is written or the pipe is closed, otherwise a failure.
     */
    static juce::Result write (std::string_view bytes);
};
