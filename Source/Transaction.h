#pragma once
#include <JuceHeader.h>
#include "Arguments.h"
#include "LineDiff.h"
#include "StagedFile.h"
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

    /** @brief Records a target file, and reports whether the file is new to this transaction.
     *
     *  A caller skips a file for which this returns false, so a file named twice in one command is edited once.
     *
     *  @param targetFile The file to record.
     *
     *  @return True when the file was not recorded before.
     */
    bool addTarget (const juce::File& targetFile);

    /** @brief Records edit counts, and stages the file when its planned bytes differ from its original bytes.
     *
     *  The edit count is added to the total even when the bytes do not change. The file is staged
     *  only when the planned bytes differ from the original bytes.
     *
     *  @param targetFile       Destination file on disk.
     *  @param originalDocument Pre-edit document snapshot; must not be null.
     *  @param diff             Line-level changes to apply onto @p originalDocument. The diff holds the planned
     *                          bytes, which the staged file writes at commit.
     *  @param fileEditCount    Logical edit count added to the `--expect` total.
     *
     *  @return Ok when the file is staged or needs no change. Otherwise the failure that
     *          Writer::getWriteStatus reports, such as a hard-linked or read-only file.
     */
    juce::Result addFile (const juce::File& targetFile,
                          std::unique_ptr<TextDocument> originalDocument,
                          LineDiff diff,
                          int fileEditCount);

    /** @brief Records an edit of a file from a planned document, and stages the file when it is new or its bytes change.
     *
     *  The edit count is added to the total even when the bytes do not change. The file is staged when it does not exist before the edit,
     *  or when the planned bytes differ from the original bytes.
     *
     *  @param targetFile       Destination file on disk.
     *  @param originalDocument Pre-edit document snapshot; must not be null.
     *  @param plannedDocument  The document of the planned bytes; must not be null.
     *  @param isExisting       True when the target exists before the edit.
     *  @param fileEditCount    Logical edit count added to the `--expect` total.
     *
     *  @return Ok when the file is staged or needs no change. Otherwise the failure that Writer::getWriteStatus reports.
     */
    juce::Result addFile (const juce::File& targetFile,
                          std::unique_ptr<TextDocument> originalDocument,
                          std::unique_ptr<TextDocument> plannedDocument,
                          bool isExisting,
                          int fileEditCount);

    /** @brief Runs the transaction: a checked dry run, or a commit with `--apply`.
     *
     *  Without `--apply`, the command prints the unified diff of each staged file and writes nothing.
     *  When `--expect` is given, it must match the total edit count first. A mismatch prints the
     *  diff, reports both counts, and exits with exitMismatch. Nothing is written, and no commit runs.
     *
     *  @param arguments       The parsed arguments of the command.
     *
     *  @return The exit code, the output documents, and the failures.
     */
    Stdio::Output run (const Arguments& arguments);

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
     *  @param diff          The diff documents to print on standard output.
     *  @param expectedCount The count that `--expect` gave.
     *  @param actualCount   The count that the edits made.
     *
     *  @return Exit code exitMismatch, the diff documents, and a failure that holds both counts.
     */
    static Stdio::Output getExpectMismatch (Stdio::Documents diff, int expectedCount, int actualCount);

    /** @brief Builds the failure of a write whose rollback left some files unrestored.
     *
     *  The message holds the failure text, the failed path, the write failure, the unrestored paths joined
     *  with a comma and a space, and the path of the blob folder.
     *
     *  @param failure         The failure text that starts the message.
     *  @param failedPath      The display path of the file that failed to write.
     *  @param status          The write failure. Its error message is appended.
     *  @param unrestoredPaths The display paths that could not be restored.
     *
     *  @return The failed result.
     */
    static juce::Result getRollbackFailure (std::string_view failure, std::string_view failedPath, const juce::Result& status, const jam::Strings& unrestoredPaths);

    /** @brief Applies a step to each staged file in order, and stops at the first failure.
     *
     *  The files before the failure are restored. The failing file is restored too when its step reports that it changed the target.
     *  The paths that cannot be restored are returned with the failure, and the folders are removed.
     *
     *  @param stagedFiles   The staged files, in transaction order.
     *  @param transactionId The transaction whose blob files are used by the restore.
     *  @param folders       The folders that the transaction created, removed after a failure.
     *  @param function      The step for each file. It returns the status, and true when the target changed.
     *
     *  @return The failure of the first file that failed, the display path of that file, and the paths that could not be restored.
     *          The status is ok, with an empty path and no paths, when every step succeeded.
     */
    static std::tuple<juce::Result, std::string_view, jam::Strings> applyToStagedFiles (const jam::Array<StagedFile>& stagedFiles, std::string_view transactionId, const jam::Array<juce::File>& folders, const std::function<std::pair<juce::Result, bool> (const StagedFile&)>& function);

    /** @brief Builds the unified diff of each staged file whose diff is not empty.
     *
     *  @param stagedFiles The staged files, in transaction order.
     *
     *  @return One document for each staged file whose diff is not empty, in transaction order.
     */
    static Stdio::Documents getDiffDocuments (const jam::Array<StagedFile>& stagedFiles);

    /** @brief Deletes the folders in reverse order, so that a child folder is deleted before its parent.
     *
     *  @param folders The folders to delete, sorted so that a parent comes before its child.
     */
    static void removeEmptyFolders (const jam::Array<juce::File>& folders);

private:
    /** @brief Builds the output of a dry run: the diff documents and the summary.
     *
     *  @return Exit code exitSuccess, the diff documents with the summary document, and no failure.
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
    std::pair<juce::Result, std::string> commit();

    /** @brief Returns the folders that the new files of the transaction need, and that do not exist yet.
     *
     *  @return The missing parent folders of the files that do not exist before the transaction, sorted so that a parent comes before its child.
     */
    jam::Array<juce::File> getMissingFolders() const;

    /** @brief Mutating subcommand name, recorded with the undo record on commit. */
    std::string commandName;

    /** @brief Files recorded by addTarget, so that a file named twice is edited once. */
    std::set<juce::File> targetFiles;

    /** @brief Files whose planned bytes differ from their original bytes, in transaction order. */
    jam::Array<StagedFile> stagedFiles;

    /** @brief Sum of the edit counts of every added file, compared with --expect. */
    int totalEditCount { 0 };
};
