#include "Transaction.h"
#include "Stdio.h"
#include "UndoCache.h"
#include "Writer.h"
#include "generated/Generated.h"

static jam::Strings getCanonicalPaths (const jam::Array<juce::File>& targetFiles)
{
    jam::Strings canonicalPaths;

    for (int index { 0 }; index < targetFiles.size(); ++index)
        canonicalPaths.add (targetFiles.at (index).getLinkedTarget().getFullPathName());

    return canonicalPaths;
}

static jam::Strings getContentHashes (const jam::Array<std::unique_ptr<TextDocument>>& documents)
{
    jam::Strings hashes;

    for (int index { 0 }; index < documents.size(); ++index)
        hashes.add (documents.at (index)->getContentHash());

    return hashes;
}

static jam::Array<std::string_view> getDocumentBytes (const jam::Array<std::unique_ptr<TextDocument>>& documents)
{
    jam::Array<std::string_view> blobs;

    for (int index { 0 }; index < documents.size(); ++index)
        blobs.add (documents.at (index)->getBytes());

    return blobs;
}

static void writeExpectMismatch (const std::string& diffText, int expectedCount, int actualCount)
{
    if (not diffText.empty())
        Stdio::write (diffText);

    Stdio::printError (juce::String::formatted (text::Diagnostics::failExpectMismatch,
                                                expectedCount,
                                                actualCount));
}

static std::pair<juce::Result, int> getValidatedRunState (const Arguments& arguments,
                                                          bool isStandardInput,
                                                          bool isApply,
                                                          bool hasExpect)
{
    if (isApply and isStandardInput)
        return { juce::Result::fail (text::Diagnostics::failApplyStandardInput), 0 };

    if (isApply and not hasExpect)
        return { juce::Result::fail (text::Diagnostics::failApplyRequiresExpect), 0 };

    if (hasExpect)
        return arguments.getInteger (Id::expectOption, 0);

    return { juce::Result::ok(), 0 };
}

static void rollbackFiles (const jam::Array<juce::File>& targetFiles,
                           const jam::Array<std::unique_ptr<TextDocument>>& originalDocuments,
                           const jam::Array<bool>& executableFlags,
                           int rollbackCount)
{
    for (int index { 0 }; index < rollbackCount; ++index)
        static_cast<void> (Writer::toFile (targetFiles.at (index),
                                            originalDocuments.at (index)->getBytes(),
                                            executableFlags.at (index)));
}

static juce::Result getFileHashStatus (const juce::File& targetFile,
                                       const juce::String& displayPath,
                                       const juce::String& expectedHash,
                                       const juce::Identifier& hashStage)
{
    const auto [readStatus, document] { TextDocument::fromFile (targetFile, displayPath) };

    if (readStatus.failed())
        return readStatus;

    if (document->getContentHash().compare (expectedHash) != 0)
        return juce::Result::fail (hashStage.toString());

    return juce::Result::ok();
}

static std::pair<juce::Result, int> getCommitFileStatus (const juce::File& targetFile,
                                                         const juce::String& displayPath,
                                                         std::string_view plannedBytes,
                                                         bool isExecutable,
                                                         const juce::String& preHash,
                                                         const juce::String& postHash,
                                                         int fileIndex)
{
    const auto preStatus { getFileHashStatus (targetFile, displayPath, preHash, Id::preHash) };

    if (preStatus.failed())
        return { preStatus, fileIndex };

    const auto writeStatus { Writer::toFile (targetFile, plannedBytes, isExecutable) };

    if (writeStatus.failed())
        return { writeStatus, fileIndex };

    const auto postStatus { getFileHashStatus (targetFile, displayPath, postHash, Id::postHash) };

    if (postStatus.failed())
        return { postStatus, fileIndex + 1 };

    return { juce::Result::ok(), 0 };
}

static juce::Result commitPlannedFiles (const jam::Array<juce::File>& targetFiles,
                                        const jam::Array<std::unique_ptr<TextDocument>>& originalDocuments,
                                        const jam::Array<std::unique_ptr<TextDocument>>& plannedDocuments,
                                        const jam::Array<bool>& executableFlags,
                                        const jam::Strings& preHashes,
                                        const jam::Strings& postHashes,
                                        const juce::File& txDir)
{
    for (int index { 0 }; index < targetFiles.size(); ++index)
    {
        const auto& displayPath { originalDocuments.at (index)->getDisplayPath() };
        const auto [fileStatus, rollbackCount] { getCommitFileStatus (targetFiles.at (index),
                                                                      displayPath,
                                                                      plannedDocuments.at (index)->getBytes(),
                                                                      executableFlags.at (index),
                                                                      preHashes.at (index),
                                                                      postHashes.at (index),
                                                                      index) };

        if (fileStatus.failed())
        {
            rollbackFiles (targetFiles, originalDocuments, executableFlags, rollbackCount);
            txDir.deleteRecursively (false);
            return juce::Result::fail (
                juce::String::formatted (text::Diagnostics::failWrite,
                                         displayPath.toRawUTF8(),
                                         fileStatus.getErrorMessage().toRawUTF8()));
        }
    }

    return juce::Result::ok();
}

Transaction::Transaction (const juce::String& newCommandName)
    : commandName (newCommandName)
{
}

void Transaction::addFile (const juce::File& targetFile,
                           std::unique_ptr<TextDocument> originalDocument,
                           LineDiff diff,
                           int fileEditCount)
{
    assert (originalDocument != nullptr);

    if (fileEditCount > 0)
    {
        auto plannedDoc { TextDocument::fromString (diff.getPlannedText (*originalDocument),
                                                    originalDocument->getDisplayPath()) };
        totalEditCount += fileEditCount;

        if (originalDocument->getBytes().compare (plannedDoc->getBytes()) != 0)
        {
            targetFiles.add (targetFile);
            originalDocuments.add (std::move (originalDocument));
            plannedDocuments.add (std::move (plannedDoc));
            diffs.add (std::move (diff));
            executableFlags.add (Writer::isExecutable (targetFile));
        }
    }
}

int Transaction::run (const Arguments& arguments, bool isStandardInput) const
{
    const bool isApply { arguments.contains (Id::applyOption) };
    const bool hasExpect { arguments.contains (Id::expectOption) };
    const auto [runStatus, expectedCount] { getValidatedRunState (arguments, isStandardInput, isApply, hasExpect) };

    if (runStatus.failed())
    {
        Stdio::printError (runStatus.getErrorMessage());
        return Stdio::exitError;
    }

    if (hasExpect and expectedCount != totalEditCount)
    {
        writeExpectMismatch (getDryRunDiffText(), expectedCount, totalEditCount);
        return Stdio::exitMismatch;
    }

    if (isApply)
        return runCommit();

    return runDryRun();
}

std::string Transaction::getDryRunDiffText() const
{
    std::string diffText;

    for (int index { 0 }; index < diffs.size(); ++index)
        diffText.append (diffs.at (index).getUnifiedDiff (*originalDocuments.at (index),
                                                          *plannedDocuments.at (index)));

    return diffText;
}

int Transaction::runDryRun() const
{
    const auto diffText { getDryRunDiffText() };

    if (not diffText.empty())
        Stdio::write (diffText);

    if (totalEditCount == 0)
    {
        Stdio::printSummary (juce::String::fromUTF8 (text::Diagnostics::summaryDryRunZero));
        return Stdio::exitSuccess;
    }

    Stdio::printSummary (Stdio::getFormatted (text::Diagnostics::summaryDryRun,
                                              totalEditCount,
                                              originalDocuments.size(),
                                              totalEditCount));
    return Stdio::exitSuccess;
}

int Transaction::runCommit() const
{
    if (totalEditCount == 0)
    {
        Stdio::printSummary (text::Diagnostics::summaryAppliedZero);
        return Stdio::exitSuccess;
    }

    if (originalDocuments.isEmpty())
    {
        Stdio::printSummary (text::Diagnostics::summaryNoContentChanged);
        return Stdio::exitSuccess;
    }

    const auto [commitStatus, txId] { commit() };

    if (commitStatus.failed())
    {
        Stdio::printError (commitStatus.getErrorMessage());
        return Stdio::exitError;
    }

    Stdio::printSummary (juce::String::formatted (text::Diagnostics::summaryApplied,
                                                  totalEditCount,
                                                  originalDocuments.size(),
                                                  txId.toRawUTF8()));
    return Stdio::exitSuccess;
}

std::pair<juce::Result, juce::String> Transaction::commit() const
{
    const auto preHashes (getContentHashes (originalDocuments));
    const auto postHashes (getContentHashes (plannedDocuments));
    const auto [stageStatus, txId] { UndoCache::addTransaction (commandName,
                                                                getCanonicalPaths (targetFiles),
                                                                preHashes,
                                                                postHashes,
                                                                executableFlags,
                                                                getDocumentBytes (originalDocuments)) };

    if (stageStatus.failed())
        return { stageStatus, {} };

    const auto txDir { UndoCache::getDirectory().getChildFile (txId) };
    const auto commitStatus { commitPlannedFiles (targetFiles,
                                                    originalDocuments,
                                                    plannedDocuments,
                                                    executableFlags,
                                                    preHashes,
                                                    postHashes,
                                                    txDir) };

    if (commitStatus.failed())
        return { commitStatus, {} };

    return { juce::Result::ok(), txId };
}
