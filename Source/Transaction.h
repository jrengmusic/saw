#pragma once
#include <JuceHeader.h>
#include "Arguments.h"
#include "LineDiff.h"
#include "Stdio.h"
#include "TextDocument.h"

/** @brief Stages multi-file edits for unified dry-run preview, `--expect` checks, and atomic commit.
 */
class Transaction
{
public:
    /** @brief Creates a transaction for one mutating subcommand.
     *
     *  @param newCommandName Mutating subcommand name, recorded with the undo record of the transaction on commit.
     */
    explicit Transaction (std::string_view newCommandName);

    /** @brief Records edit counts, and stages the file when its planned bytes differ from its original bytes.
     *
     *  The edit count is added to the total even when the bytes do not change. The file is staged
     *  only when the planned bytes differ from the original bytes.
     *
     *  @param targetFile       Destination file on disk.
     *  @param originalDocument Pre-edit document snapshot; must not be null.
     *  @param diff             Line-level changes to apply onto @p originalDocument.
     *  @param fileEditCount    Logical edit count added to the `--expect` total.
     *
     *  @return Ok when the file is staged or needs no change. Otherwise the failure that
     *          Writer::getWriteStatus reports, such as a hard-linked or read-only file.
     */
    juce::Result addFile (const juce::File& targetFile,
                          std::unique_ptr<TextDocument> originalDocument,
                          LineDiff diff,
                          int fileEditCount);

    /** @brief Runs the transaction: a checked dry run, or a commit with `--apply`.
     *
     *  Without `--apply`, the command prints the unified diff of each staged file and writes nothing.
     *  When `--expect` is given, it must match the total edit count first. A mismatch prints the
     *  diff, reports both counts, and exits with exitMismatch. Nothing is written, and no commit runs.
     *
     *  @param arguments       The parsed arguments of the command.
     *  @param isStandardInput True when the input is standard input. `--apply` is refused then.
     *
     *  @return The exit code, the output bytes, and the diagnostics.
     */
    Stdio::Output run (const Arguments& arguments, bool isStandardInput);

    /** @brief Reads the `--expect` count from the arguments, and checks it against `--apply`.
     *
     *  `--apply` requires `--expect`. The count is 0 when `--expect` is absent.
     *
     *  @param arguments The parsed arguments of the command.
     *
     *  @return Status result paired with the expected edit count.
     */
    static std::pair<juce::Result, int> getExpectStatus (const Arguments& arguments);

    /** @brief Builds the output of a run whose edit count differs from `--expect`.
     *
     *  @param diffText      The unified diff to print on standard output.
     *  @param expectedCount The count that `--expect` gave.
     *  @param actualCount   The count that the edits made.
     *
     *  @return Exit code exitMismatch, the diff text, and a diagnostic with both counts.
     */
    static Stdio::Output getExpectMismatch (std::string diffText, int expectedCount, int actualCount);

private:
    /** @brief Renders the unified diff of every staged file.
     *
     *  @return The concatenated diff text. Empty when no file is staged.
     */
    std::string getDryRunDiffText() const;

    /** @brief Builds the output of a dry run: the diff and the summary.
     *
     *  @return Exit code exitSuccess, the diff text with the summary, and no diagnostic.
     */
    Stdio::Output runDryRun() const;

    /** @brief Commits the staged files and builds the output of the commit.
     *
     *  @return Exit code exitSuccess when the commit and the publish succeed. Exit code exitError
     *          when the files were written but the publish failed, with the id still reported.
     */
    Stdio::Output runCommit();

    /** @brief Stages the blobs and the record, writes the planned files, and publishes the record.
     *
     *  The id is returned as soon as the files are written, even when the publish fails. This
     *  lets the caller report the id, so the user can undo the written files.
     *
     *  @return Status result paired with the transaction id. The id is empty when staging or writing failed.
     */
    std::pair<juce::Result, juce::String> commit();

    juce::String commandName;
    jam::Array<juce::File> targetFiles;
    jam::Array<std::unique_ptr<TextDocument>> originalDocuments;
    std::vector<std::string> plannedTexts;
    jam::Array<LineDiff> diffs;
    jam::Array<bool> executableFlags;
    int totalEditCount { 0 };
};
