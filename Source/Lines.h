#pragma once
#include <JuceHeader.h>
#include "Arguments.h"
#include "Stdio.h"
#include "TextDocument.h"
#include "Transaction.h"

/** @brief Implements the `saw lines` subcommand for reading and mutating line ranges.
 */
class Lines
{
public:
    /** @brief Parses the argument list of lines, with its flags, value options, and optional-value options.
     *
     *  @param argumentList The argument list, with the verb name at index 0.
     *
     *  @return The parsed arguments. Their status holds any parse failure.
     */
    static Arguments getArguments (juce::Span<const std::string_view> argumentList);

    /** @brief Runs lines on parsed arguments.
     *
     *  A line range that fails its check is returned as a failure. Without a mutating option, the
     *  lines are read. Otherwise the lines are changed.
     *
     *  @param arguments The parsed arguments from getArguments.
     *  @param input     The standard input, the null device, or a supplied text.
     *
     *  @return The exit code, the output documents, and the failures.
     */
    static Stdio::Output run (const Arguments& arguments, const Input::Source& input);

private:
    /** @brief Binds the parsed arguments of one lines command to its line range and its payload.
     *
     *  @param arguments         The parsed arguments.
     *  @param newLineRange      One-based half-open range of lines that the command addresses.
     *  @param newPayloadInput   The owner of the payload bytes when the payload comes from standard input, otherwise nullptr.
     *  @param newPayloadText    The payload text: the bytes of newPayloadInput, or the value of the mutation option.
     */
    Lines (const Arguments& arguments, juce::Range<int> newLineRange, std::unique_ptr<Input> newPayloadInput, std::string_view newPayloadText);

    /** @brief Reads standard input as one text, and refuses a terminal.
     *
     *  Only the process standard input can be a terminal. A terminal is refused, so the command does not wait for typed text.
     *
     *  @param input The standard input, the null device, or a supplied text.
     *
     *  @return The exit code, the output documents, and the failures.
     */
    Stdio::Output runStandardInput (const Input::Source& input) const;

    /** @brief Reads each file in its own job, and writes the output of each file into its own slot.
     *
     *  The slots keep the order of the sorted file entries. No lock is needed, because each job writes one slot.
     *
     *  @param rawPaths The paths that the user named.
     *  @param source   The source of the input, passed to TextDocument::getInput for each file.
     *
     *  @return The exit code, and one document group for each file in file order. The first failure in job order is returned as the failure, with no documents.
     */
    Stdio::Output runFiles (juce::Span<const std::string_view> rawPaths, const Input::Source& source) const;

    /** @brief Stages the mutation of every target file in one transaction, and runs the transaction.
     *
     *  @param arguments The parsed arguments of the mutation.
     *
     *  @return The output of the transaction, or the failure of the first file that could not be staged.
     */
    Stdio::Output runMutation (const Arguments& arguments) const;

    /** @brief Reads one file and returns the documents of its selected lines.
     *
     *  @param file        The file to read.
     *  @param displayPath The display path of the file.
     *  @param isExplicit  True when the file was named on the command line.
     *  @param source    The source of the input, passed to TextDocument::getInput.
     *
     *  @return The read status, and the documents of the selected lines.
     */
    std::pair<juce::Result, Stdio::Documents> getFileOutput (const juce::File& file, std::string_view displayPath, bool isExplicit, const Input::Source& source) const;

    /** @brief Builds the documents of the selected lines of an input.
     *
     *  Without a line number or a hash prefix, the selected bytes are borrowed from the input as one document, so the document is a view.
     *  With a prefix, each selected line is formatted into one document.
     *
     *  @param input       The input that holds the bytes. Ownership passes to a borrowed document.
     *  @param displayPath The display path of the input.
     *  @param isExplicit  True when the input was named on the command line.
     *
     *  @return The read status, and the documents of the selected lines.
     */
    std::pair<juce::Result, Stdio::Documents> getOutputDocuments (std::unique_ptr<Input> input, std::string_view displayPath, bool isExplicit) const;

    /** @brief Appends one selected line, with its line number or hash prefix, and its line ending.
     *
     *  @param output     The text that receives the line.
     *  @param lineNumber The one-based number of the line.
     *  @param line       The line bytes, without the line ending.
     *  @param ending    The line ending of the line, as TextDocument::getLineEnding gives it.
     */
    void appendFormattedLine (std::string& output, int lineNumber, std::string_view line, std::string_view ending) const;

    /** @brief Stages the mutation of one file as one edit of the transaction.
     *
     *  The file is read with Input::allBytes, so the document holds an owned copy of the bytes. A later write
     *  to the file cannot change those bytes.
     *
     *  @param transaction The transaction that receives the edit.
     *  @param targetFile  The file to mutate.
     *  @param displayPath The display path of the file.
     *
     *  @return Ok when the edit is staged, otherwise the failure.
     */
    juce::Result addFileMutation (Transaction& transaction, const juce::File& targetFile, std::string_view displayPath) const;

    /** @brief Resolves the mutation range against the line count, and checks its bounds.
     *
     *  An open end becomes numLines + 1. An insertAfter may start at any line from 0 to numLines, so an insertion can follow the last line.
     *  Every other mutation needs a range inside the lines.
     *
     *  @param document    The document to mutate.
     *  @param displayPath The display path of the document, used in the failure text.
     *
     *  @return The result, and the half-open range of lines to mutate.
     */
    std::pair<juce::Result, juce::Range<int>> getEffectiveMutationRange (const TextDocument& document, std::string_view displayPath) const;

    /** @brief Checks the expected hash of the first line of the range, when one was given.
     *
     *  A start line below 1 has no line to hash, so it fails whenever a hash is expected.
     *
     *  @param document    The document to check.
     *
     *  @return Ok when no hash is expected or the hash matches, otherwise the failure.
     */
    juce::Result getVerifiedHashStatus (const TextDocument& document) const;

    /** @brief One-based half-open range of lines that the command addresses, as given on the command line. */
    juce::Range<int> lineRange;

    /** @brief The range text as the user typed it, used in the bounds diagnostic. */
    std::string_view rangeText;

    /** @brief Name of the mutation option that is active, or an empty identifier when the command reads. */
    juce::Identifier activeMutation;

    /** @brief The hash that the first line must have, when --verify-hash is given. */
    std::optional<std::string_view> expectedHash;

    /** @brief Owns the payload bytes when the payload comes from standard input, and is null otherwise. */
    std::unique_ptr<Input> payloadInput;

    /** @brief The payload text: the bytes of payloadInput, or a view of the mutation option value. */
    std::string_view payloadText;

    /** @brief True when each selected line is printed with its hash. */
    bool isShowingHash { false };

    /** @brief True when each selected line is printed with its line number. */
    bool isShowingLineNumber { false };

    /** @brief True when binary input is formatted rather than reported. */
    bool isText { false };
};
