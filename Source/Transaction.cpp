#include "Transaction.h"
#include "Stdio.h"
#include "UndoCache.h"
#include "Writer.h"
#include "generated/Generated.h"

static std::pair<juce::Result, int> getValidatedRunState (const Arguments& arguments, bool isStandardInput)
{
    if (arguments.contains (Id::applyOption) and isStandardInput)
        return { juce::Result::fail (text::Diagnostics::failApplyStandardInput), 0 };

    return Transaction::getExpectStatus (arguments);
}

static juce::Result getRestoreStatus (const juce::File& targetFile,
                                      const juce::File& blobFile,
                                      const TextDocument& originalDocument,
                                      std::string_view plannedBytes)
{
    const auto& displayPath { originalDocument.getDisplayPath() };
    const auto blobStatus { TextDocument::getBytesStatus (blobFile, displayPath, originalDocument.getBytes(), Id::preHash) };

    if (blobStatus.failed())
        return blobStatus;

    if (jam::File::swapFiles (targetFile, blobFile))
    {
        const auto swapStatus { TextDocument::getBytesStatus (blobFile, displayPath, plannedBytes, Id::postHash) };

        if (swapStatus.failed())
        {
            jam::File::swapFiles (targetFile, blobFile);
            return swapStatus;
        }
    }
    else
    {
        const auto plannedStatus { TextDocument::getBytesStatus (targetFile, displayPath, plannedBytes, Id::postHash) };

        if (plannedStatus.failed())
            return plannedStatus;

        const auto writeStatus { Writer::toFile (targetFile, originalDocument.getBytes()) };

        if (writeStatus.failed())
            return writeStatus;
    }

    return TextDocument::getBytesStatus (targetFile, displayPath, originalDocument.getBytes(), Id::preHash);
}

static jam::Strings rollbackFiles (const jam::Array<juce::File>& targetFiles,
                                   const jam::Array<std::unique_ptr<TextDocument>>& originalDocuments,
                                   const std::vector<std::string>& plannedTexts,
                                   const juce::String& txId,
                                   int rollbackCount)
{
    jam::Strings failedPaths;

    for (int index { 0 }; index < rollbackCount; ++index)
    {
        const auto restoreStatus { getRestoreStatus (targetFiles.at (index),
                                                     UndoCache::getBlobFile (txId, index),
                                                     *originalDocuments.at (index),
                                                     plannedTexts.at (static_cast<size_t> (index))) };

        if (restoreStatus.failed())
            failedPaths.add (originalDocuments.at (index)->getDisplayPath());
    }

    return failedPaths;
}

static std::pair<juce::Result, int> getSwappedFileStatus (const juce::File& targetFile,
                                                          const juce::File& blobFile,
                                                          const TextDocument& originalDocument,
                                                          std::string_view plannedBytes,
                                                          int fileIndex)
{
    const auto& displayPath { originalDocument.getDisplayPath() };
    const auto preStatus { TextDocument::getBytesStatus (blobFile, displayPath, originalDocument.getBytes(), Id::preHash) };

    if (preStatus.failed())
    {
        const bool isSwappedBack { jam::File::swapFiles (targetFile, blobFile) };

        if (isSwappedBack and TextDocument::getBytesStatus (blobFile, displayPath, plannedBytes, Id::postHash).wasOk())
            return { preStatus, fileIndex };

        if (isSwappedBack)
            jam::File::swapFiles (targetFile, blobFile);

        return { preStatus, fileIndex + 1 };
    }

    const auto postStatus { TextDocument::getBytesStatus (targetFile, displayPath, plannedBytes, Id::postHash) };

    if (postStatus.failed())
        return { postStatus, fileIndex + 1 };

    return { juce::Result::ok(), 0 };
}

static std::pair<juce::Result, int> getWrittenFileStatus (const juce::File& targetFile,
                                                          const TextDocument& originalDocument,
                                                          std::string_view plannedBytes,
                                                          int fileIndex)
{
    const auto& displayPath { originalDocument.getDisplayPath() };
    const auto preStatus { TextDocument::getBytesStatus (targetFile, displayPath, originalDocument.getBytes(), Id::preHash) };

    if (preStatus.failed())
        return { preStatus, fileIndex };

    const auto writeStatus { Writer::toFile (targetFile, plannedBytes) };

    if (writeStatus.failed())
    {
        const bool isUnchanged { TextDocument::getBytesStatus (targetFile, displayPath, originalDocument.getBytes(), Id::preHash).wasOk() };
        return { writeStatus, isUnchanged ? fileIndex : fileIndex + 1 };
    }

    const auto postStatus { TextDocument::getBytesStatus (targetFile, displayPath, plannedBytes, Id::postHash) };

    if (postStatus.failed())
        return { postStatus, fileIndex + 1 };

    return { juce::Result::ok(), 0 };
}

static std::pair<juce::Result, int> getCommitFileStatus (const juce::File& targetFile,
                                                         const juce::File& blobFile,
                                                         const TextDocument& originalDocument,
                                                         std::string_view plannedBytes,
                                                         int fileIndex)
{
    if (jam::File::isSwappable (targetFile, blobFile))
    {
        if (jam::File::swapFiles (targetFile, blobFile))
            return getSwappedFileStatus (targetFile, blobFile, originalDocument, plannedBytes, fileIndex);

        const auto backupStatus { Writer::toFile (blobFile, originalDocument.getBytes()) };

        if (backupStatus.failed())
            return { backupStatus, fileIndex };
    }

    return getWrittenFileStatus (targetFile, originalDocument, plannedBytes, fileIndex);
}

static juce::Result commitPlannedFiles (const jam::Array<juce::File>& targetFiles,
                                        const jam::Array<std::unique_ptr<TextDocument>>& originalDocuments,
                                        const std::vector<std::string>& plannedTexts,
                                        const juce::String& txId)
{
    for (int index { 0 }; index < targetFiles.size(); ++index)
    {
        const auto displayPath { originalDocuments.at (index)->getDisplayPath() };
        const auto [fileStatus, rollbackCount] { getCommitFileStatus (targetFiles.at (index),
                                                                      UndoCache::getBlobFile (txId, index),
                                                                      *originalDocuments.at (index),
                                                                      plannedTexts.at (static_cast<size_t> (index)),
                                                                      index) };

        if (fileStatus.failed())
        {
            const auto failedPaths { rollbackFiles (targetFiles, originalDocuments, plannedTexts, txId, rollbackCount) };

            if (failedPaths.size() == 0)
            {
                UndoCache::removeTransaction (txId);
                return juce::Result::fail (
                    Stdio::getFormatted (text::Diagnostics::failWrite,
                                         displayPath.toRawUTF8(),
                                         fileStatus.getErrorMessage().toRawUTF8()));
            }

            return juce::Result::fail (
                Stdio::getFormatted (text::Diagnostics::failRollback,
                                     displayPath.toRawUTF8(),
                                     fileStatus.getErrorMessage().toRawUTF8(),
                                     failedPaths.joinIntoString (", ").toRawUTF8(),
                                     UndoCache::getBlobDirectory().getFullPathName().toRawUTF8()));
        }
    }

    return juce::Result::ok();
}

Transaction::Transaction (std::string_view newCommandName)
    : commandName (jam::Strings::fromView (newCommandName))
{
}

juce::Result Transaction::addFile (const juce::File& targetFile,
                                   std::unique_ptr<TextDocument> originalDocument,
                                   LineDiff diff,
                                   int fileEditCount)
{
    assert (originalDocument != nullptr);

    if (fileEditCount > 0)
    {
        auto plannedText { diff.getPlannedText (*originalDocument) };
        totalEditCount += fileEditCount;

        if (originalDocument->getBytes().compare (plannedText) != 0)
        {
            const auto writeStatus { Writer::getWriteStatus (targetFile, originalDocument->getDisplayPath()) };

            if (writeStatus.failed())
                return writeStatus;

            targetFiles.add (targetFile);
            originalDocuments.add (std::move (originalDocument));
            plannedTexts.push_back (std::move (plannedText));
            diffs.add (std::move (diff));
            executableFlags.add (Writer::isExecutable (targetFile));
        }
    }

    return juce::Result::ok();
}

Stdio::Output Transaction::run (const Arguments& arguments, bool isStandardInput)
{
    const bool isApply { arguments.contains (Id::applyOption) };
    const bool hasExpect { arguments.contains (Id::expectOption) };
    const auto [runStatus, expectedCount] { getValidatedRunState (arguments, isStandardInput) };

    if (runStatus.failed())
        return Stdio::getFailure (runStatus.getErrorMessage());

    if (hasExpect and expectedCount != totalEditCount)
        return getExpectMismatch (getDryRunDiffText(), expectedCount, totalEditCount);

    if (isApply)
        return runCommit();

    return runDryRun();
}

std::pair<juce::Result, int> Transaction::getExpectStatus (const Arguments& arguments)
{
    if (arguments.contains (Id::applyOption) and not arguments.contains (Id::expectOption))
        return { juce::Result::fail (text::Diagnostics::failApplyRequiresExpect), 0 };

    return arguments.getInteger (Id::expectOption, 0, 0);
}

Stdio::Output Transaction::getExpectMismatch (std::string diffText, int expectedCount, int actualCount)
{
    return { Stdio::exitMismatch,
             std::move (diffText),
             Stdio::getDiagnostic (Stdio::getFormatted (text::Diagnostics::failExpectMismatch,
                                                        expectedCount,
                                                        actualCount)) };
}

std::string Transaction::getDryRunDiffText() const
{
    std::string diffText;

    for (int index { 0 }; index < diffs.size(); ++index)
        diffText.append (diffs.at (index).getUnifiedDiff (*originalDocuments.at (index), plannedTexts.at (static_cast<size_t> (index))));

    return diffText;
}

Stdio::Output Transaction::runDryRun() const
{
    std::string printedOutput { getDryRunDiffText() };

    if (totalEditCount == 0)
    {
        printedOutput += Stdio::getDiagnostic (juce::String::fromUTF8 (text::Diagnostics::summaryDryRunZero));
        return { Stdio::exitSuccess, std::move (printedOutput), {} };
    }

    printedOutput += Stdio::getDiagnostic (Stdio::getFormatted (text::Diagnostics::summaryDryRun,
                                                                totalEditCount,
                                                                originalDocuments.size(),
                                                                totalEditCount));
    return { Stdio::exitSuccess, std::move (printedOutput), {} };
}

Stdio::Output Transaction::runCommit()
{
    if (totalEditCount == 0)
        return { Stdio::exitSuccess, Stdio::getDiagnostic (text::Diagnostics::summaryAppliedZero), {} };

    if (targetFiles.isEmpty())
        return { Stdio::exitSuccess, Stdio::getDiagnostic (text::Diagnostics::summaryNoContentChanged), {} };

    const auto [commitStatus, txId] { commit() };

    if (commitStatus.failed() and txId.isEmpty())
        return Stdio::getFailure (commitStatus.getErrorMessage());

    const auto appliedSummary { Stdio::getDiagnostic (Stdio::getFormatted (text::Diagnostics::summaryApplied,
                                                                           totalEditCount,
                                                                           targetFiles.size(),
                                                                           txId.toRawUTF8())) };

    if (commitStatus.failed())
        return { Stdio::exitError, appliedSummary, Stdio::getDiagnostic (commitStatus.getErrorMessage()) };

    return { Stdio::exitSuccess, appliedSummary, {} };
}

std::pair<juce::Result, juce::String> Transaction::commit()
{
    const auto [stageStatus, txId] { UndoCache::addTransaction (commandName, targetFiles, executableFlags, originalDocuments, plannedTexts) };

    if (stageStatus.failed())
        return { stageStatus, {} };

    const auto commitStatus { commitPlannedFiles (targetFiles, originalDocuments, plannedTexts, txId) };

    if (commitStatus.failed())
        return { commitStatus, {} };

    const auto publishStatus { UndoCache::publishTransaction (txId) };
    UndoCache::removeOldestTransactions();
    return { publishStatus, txId };
}
