#pragma once
#include <JuceHeader.h>
#include "TextDocument.h"
#include "generated/Generated.h"

/** @brief Process exit codes, and the output of a command as documents and failures.
 *
 *  A verb returns an Output, so that no verb writes to a process stream. write sends the
 *  Output to the process streams, once, at the end of the command.
 */
struct Stdio
{
    /** @brief Exit code when a command succeeds or finds matches. */
    static constexpr int exitSuccess { 0 };

    /** @brief Exit code when find matches nothing or --expect disagrees with actual edits. */
    static constexpr int exitMismatch { 1 };

    /** @brief Exit code when argument parsing, validation, I/O, or a transaction fails. */
    static constexpr int exitError { 2 };

    /** @brief Line feed that ends each line of a diagnostic and each summary line. */
    static constexpr std::string_view newline { "\n" };

    /** @brief Ordered documents of the standard output of one command, one for each piece of output. */
    using Documents = jam::Array<std::unique_ptr<TextDocument>>;

    /** @brief Result of a command: exit code, the documents of the standard output, and the failures.
     *
     *  The documents are written as they are, and the failures are written as diagnostic lines on
     *  standard error. No copy is made between the creation of the output and its write.
     */
    using Output = std::tuple<int, Documents, jam::Array<juce::Result>>;

    /** @brief Builds an Output from the documents and the failures of a command.
     *
     *  Both are moved into the Output.
     *
     *  @param exitCode  The exit code of the command.
     *  @param documents The documents of the standard output, in write order. It is empty when no output is written.
     *  @param failures  The failures, each written as a diagnostic line on standard error. It is empty when there is none.
     *
     *  @return The exit code, the documents, and the failures.
     */
    static Output getOutput (int exitCode, Documents documents = {}, jam::Array<juce::Result> failures = {});

    /** @brief Wraps text in one document, or returns no document when the text is empty.
     *
     *  @param text        The text, moved into the document.
     *  @param displayPath The path of the document, or empty for standard output.
     *
     *  @return One document holding the text, or no document.
     */
    static Documents getDocuments (std::string&& text, std::string_view displayPath = {});

    /** @brief Appends documents to a list, moving each of them, so that no document is copied.
     *
     *  @param documents      The list that receives the documents.
     *  @param addedDocuments The documents to append. They are moved out.
     */
    static void addDocuments (Documents& documents, Documents&& addedDocuments);

    /** @brief Builds the output of a command that failed with one status.
     *
     *  @param status The failed status.
     *
     *  @return Exit code exitError, no documents, and the status as the only failure.
     */
    static Output getFailure (const juce::Result& status);

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

    /** @brief Formats a diagnostic line with the program name as its prefix.
     *
     *  @tparam Pieces The types of the pieces.
     *  @param pieces  The pieces of the message, without a newline.
     *
     *  @return The program name, the diagnostic separator, the message, and one newline.
     */
    template <typename... Pieces>
    static std::string getDiagnostic (const Pieces&... pieces)
    {
        std::string diagnostic;
        appendPiece (diagnostic, ProjectInfo::projectName);
        appendPiece (diagnostic, jam::Strings::getView (Id::diagnosticSeparator));
        (appendPiece (diagnostic, pieces), ...);
        jam::Format::appendUtf8 (diagnostic, Chars::newline);
        return diagnostic;
    }

    /** @brief Builds a failed result whose message holds the pieces, in order.
     *
     *  The message has no program name and no newline. See getDiagnostic for the diagnostic line.
     *
     *  @tparam Pieces The types of the pieces.
     *  @param pieces  The pieces of the message.
     *
     *  @return A failed juce::Result with the message.
     */
    template <typename... Pieces>
    static juce::Result fail (const Pieces&... pieces)
    {
        juce::String message;
        (appendPiece (message, pieces), ...);
        return juce::Result::fail (message);
    }

    /** @brief Builds the output of a failed command.
     *
     *  @tparam Pieces The types of the pieces of the diagnostic message.
     *  @param pieces  The pieces of the diagnostic message.
     *
     *  @return Exit code exitError, empty standard output, and the diagnostic on standard error.
     */
    template <typename... Pieces>
    static Output getFailure (const Pieces&... pieces)
    {
        return getFailure (fail (pieces...));
    }

    /** @brief Returns the first failed status in the order given.
     *
     *  The order of the statuses decides which failure is returned, so the same inputs give the same failure.
     *
     *  @param statuses The statuses, one for each job, in job order.
     *
     *  @return The first failed status, or ok when every status succeeded.
     */
    static juce::Result getFirstFailure (const jam::Array<juce::Result>& statuses);

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

    /** @brief Passes the pieces of the diagnostic line of a failure to a function, in order, without building the line.
     *
     *  The pieces are the program name, the diagnostic separator, the message, and the line feed. The
     *  function writes each piece where it goes, so the line text is never built.
     *
     *  @tparam Function The type of the callable. It takes a std::string_view.
     *  @param failure   The failure whose message is the diagnostic.
     *  @param function  Receives each piece in order.
     */
    template <typename Function>
    static void applyToPieces (const juce::Result& failure, const Function& function)
    {
        const auto& message { failure.getErrorMessage() };
        function (std::string_view { ProjectInfo::projectName });
        function (jam::Strings::getView (Id::diagnosticSeparator));
        function (jam::Strings::getView (message));
        function (newline);
    }

private:
    /** @brief Appends one piece to a summary line of the standard output, as UTF-8 bytes.
     *
     *  @param message The line that receives the piece.
     *  @param piece   The piece to append.
     */
    static void appendPiece (std::string& message, std::string_view piece);

    /** @brief Appends one integer piece to a summary line, as decimal digits.
     *
     *  @param message The line that receives the piece.
     *  @param piece   The integer to append.
     */
    static void appendPiece (std::string& message, int piece);

    /** @brief Appends one character piece to a summary line, as UTF-8 bytes.
     *
     *  @param message The line that receives the piece.
     *  @param piece   The character to append.
     */
    static void appendPiece (std::string& message, juce::juce_wchar piece);

    /** @brief Appends the message of a result to a summary line, as UTF-8 bytes.
     *
     *  @param message The line that receives the message.
     *  @param piece   The result whose error message is appended.
     */
    static void appendPiece (std::string& message, const juce::Result& piece);

    /** @brief Appends one piece to the message of a juce::Result.
     *
     *  @param message The message that receives the piece.
     *  @param piece   The piece to append. An empty piece appends nothing.
     */
    static void appendPiece (juce::String& message, std::string_view piece);

    /** @brief Appends one integer piece to the message of a juce::Result, as decimal digits.
     *
     *  @param message The message that receives the piece.
     *  @param piece   The integer to append.
     */
    static void appendPiece (juce::String& message, int piece);

    /** @brief Appends one character piece to the message of a juce::Result.
     *
     *  @param message The message that receives the piece.
     *  @param piece   The character to append.
     */
    static void appendPiece (juce::String& message, juce::juce_wchar piece);

    /** @brief Appends the message of a result to the message of another result.
     *
     *  @param message The message that receives the piece.
     *  @param piece   The result whose error message is appended.
     */
    static void appendPiece (juce::String& message, const juce::Result& piece);
};
