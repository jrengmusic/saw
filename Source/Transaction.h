#pragma once
#include <JuceHeader.h>
#include "Arguments.h"
#include "LineDiff.h"
#include "TextDocument.h"

/** @brief Stages multi-file edits for unified dry-run preview, `--expect` checks, and atomic commit.
 */
class Transaction
{
public:
    /** @brief Creates a transaction labeled with @p newCommandName for undo metadata.
     *
     *  @param newCommandName Mutating subcommand name recorded in `meta.json` on commit.
     */
    explicit Transaction (const juce::String& newCommandName);

    /** @brief Records edit counts and stages @p targetFile when @p diff changes file bytes.
     *
     *  @param targetFile       Destination file on disk (empty for standard input dry runs).
     *  @param originalDocument Pre-edit document snapshot; must not be null.
     *  @param diff             Line-level changes to apply onto @p originalDocument.
     *  @param fileEditCount    Logical edit count added to the `--expect` total.
     */
    void addFile (const juce::File& targetFile,
                  std::unique_ptr<TextDocument> originalDocument,
                  LineDiff diff,
                  int fileEditCount);

    /** @brief Validates `--apply` and `--expect`, then previews diffs or commits with rollback.
     *
     *  Requires `--expect` for `--apply`, rejects `--apply` on standard input, and restores
     *  prior files while deleting the undo folder if any pre-hash, write, or post-hash fails.
     *
     *  @param arguments       Parsed command arguments carrying `--apply` and `--expect`.
     *  @param isStandardInput True when edits target standard input rather than files.
     *
     *  @return `Stdio::exitSuccess`, `Stdio::exitMismatch` on `--expect` mismatch, or `Stdio::exitError`.
     */
    int run (const Arguments& arguments, bool isStandardInput) const;

private:
    std::string getDryRunDiffText() const;
    int runDryRun() const;
    int runCommit() const;
    std::pair<juce::Result, juce::String> commit() const;

    juce::String commandName;
    jam::Array<juce::File> targetFiles;
    jam::Array<std::unique_ptr<TextDocument>> originalDocuments;
    jam::Array<std::unique_ptr<TextDocument>> plannedDocuments;
    jam::Array<LineDiff> diffs;
    jam::Array<bool> executableFlags;
    int totalEditCount { 0 };
};
