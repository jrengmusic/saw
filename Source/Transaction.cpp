#include "Transaction.h"
#include "Stdio.h"
#include "UndoCache.h"
#include "Writer.h"
#include "generated/Generated.h"

Transaction::Transaction (std::string_view newCommandName)
    : commandName (newCommandName)
{
}

bool Transaction::addTarget (const juce::File& targetFile)
{
    const auto [targetEntry, isInserted] { targetFiles.insert (targetFile) };
    juce::ignoreUnused (targetEntry);
    return isInserted;
}

juce::Result Transaction::addFile (const juce::File& targetFile,
                                   std::unique_ptr<TextDocument> originalDocument,
                                   LineDiff diff,
                                   int fileEditCount)
{
    assert (originalDocument != nullptr);

    if (fileEditCount > 0)
    {
        totalEditCount += fileEditCount;

        if (originalDocument->getBytes().compare (diff.getPlannedBytes()) != 0)
        {
            const auto writeStatus { Writer::getWriteStatus (targetFile, originalDocument->getDisplayPath()) };

            if (writeStatus.failed())
                return writeStatus;

            stagedFiles.add (StagedFile (targetFile, std::move (originalDocument), std::move (diff), stagedFiles.size()));
        }
    }

    return juce::Result::ok();
}

Stdio::Output Transaction::run (const Arguments& arguments)
{
    const bool isApply { arguments.contains (Id::applyOption) };
    const bool hasExpect { arguments.contains (Id::expectOption) };
    const auto [runStatus, expectedCount] { getExpectStatus (arguments) };

    if (runStatus.failed())
        return Stdio::getFailure (runStatus);

    if (hasExpect and expectedCount != totalEditCount)
        return getExpectMismatch (getDiffDocuments (stagedFiles), expectedCount, totalEditCount);

    if (isApply)
        return runCommit();

    return runDryRun();
}

std::pair<juce::Result, int> Transaction::getExpectStatus (const Arguments& arguments)
{
    if (arguments.contains (Id::applyOption) and not arguments.contains (Id::expectOption))
        return { Stdio::fail (text::Diagnostics::failApplyRequiresExpect), 0 };

    return arguments.getInteger (Id::expectOption, 0, 0);
}

Stdio::Output Transaction::getExpectMismatch (Stdio::Documents diff, int expectedCount, int actualCount)
{
    jam::Array<juce::Result> failures;
    failures.add (Stdio::fail (text::Diagnostics::failExpectMismatch,
                               expectedCount,
                               text::Diagnostics::foundEdits,
                               actualCount,
                               text::Diagnostics::nothingWritten));
    return Stdio::getOutput (Stdio::exitMismatch, std::move (diff), std::move (failures));
}

juce::Result Transaction::getRollbackFailure (std::string_view failure, std::string_view failedPath, const juce::Result& status, const jam::Strings& unrestoredPaths)
{
    static constexpr const char* pathSeparator { ", " };
    return Stdio::fail (failure,
                        failedPath,
                        jam::Strings::getView (Id::diagnosticSeparator),
                        status,
                        text::Diagnostics::rollbackFailed,
                        jam::Strings::getView (unrestoredPaths.joinIntoString (pathSeparator)),
                        text::Diagnostics::backupKept,
                        jam::Strings::getView (UndoCache::getBlobDirectory().getFullPathName()));
}

std::tuple<juce::Result, std::string_view, jam::Strings> Transaction::applyToStagedFiles (const jam::Array<StagedFile>& stagedFiles,
                                                                                         std::string_view transactionId,
                                                                                         const std::function<std::pair<juce::Result, bool> (const StagedFile&)>& function)
{
    for (int index { 0 }; index < stagedFiles.size(); ++index)
    {
        const auto& stagedFile { stagedFiles.at (index) };
        const auto [fileStatus, isChanged] { function (stagedFile) };

        if (fileStatus.failed())
        {
            const int restoreCount { isChanged ? index + 1 : index };
            jam::Strings unrestoredPaths;

            for (int restoreIndex { 0 }; restoreIndex < restoreCount; ++restoreIndex)
            {
                const auto& restoredFile { stagedFiles.at (restoreIndex) };

                if (restoredFile.restore (transactionId).failed())
                    unrestoredPaths.add (restoredFile.getDisplayPath());
            }

            return { fileStatus, stagedFile.getDisplayPath(), std::move (unrestoredPaths) };
        }
    }

    return { juce::Result::ok(), std::string_view {}, jam::Strings {} };
}

Stdio::Documents Transaction::getDiffDocuments (const jam::Array<StagedFile>& stagedFiles)
{
    Stdio::Documents documents;

    for (int index { 0 }; index < stagedFiles.size(); ++index)
    {
        const auto& stagedFile { stagedFiles.at (index) };
        std::string diffText;
        stagedFile.appendUnifiedDiff (diffText);

        if (not diffText.empty())
            documents.add (TextDocument::fromText (std::move (diffText), stagedFile.getDisplayPath()));
    }

    return documents;
}

Stdio::Output Transaction::runDryRun() const
{
    auto documents (getDiffDocuments (stagedFiles));

    if (totalEditCount == 0)
    {
        documents.add (TextDocument::fromText (Stdio::getDiagnostic (text::Diagnostics::summaryDryRunZero), {}));
        return Stdio::getOutput (Stdio::exitSuccess, std::move (documents));
    }

    documents.add (TextDocument::fromText (Stdio::getDiagnostic (text::Diagnostics::summaryDryRun,
                                                                 totalEditCount,
                                                                 text::Diagnostics::editsIn,
                                                                 stagedFiles.size(),
                                                                 text::Diagnostics::expectToWrite,
                                                                 totalEditCount,
                                                                 text::Diagnostics::toWrite), {}));
    return Stdio::getOutput (Stdio::exitSuccess, std::move (documents));
}

Stdio::Output Transaction::runCommit()
{
    if (totalEditCount == 0)
        return Stdio::getOutput (Stdio::exitSuccess, Stdio::getDocuments (Stdio::getDiagnostic (text::Diagnostics::summaryAppliedZero)));

    if (stagedFiles.isEmpty())
        return Stdio::getOutput (Stdio::exitSuccess, Stdio::getDocuments (Stdio::getDiagnostic (text::Diagnostics::summaryNoContentChanged)));

    const auto [commitStatus, txId] { commit() };

    if (commitStatus.failed() and txId.empty())
        return Stdio::getFailure (commitStatus);

    auto appliedSummary (Stdio::getDocuments (Stdio::getDiagnostic (text::Diagnostics::summaryApplied,
                                                                    totalEditCount,
                                                                    text::Diagnostics::editsIn,
                                                                    stagedFiles.size(),
                                                                    text::Diagnostics::transactionOpen,
                                                                    txId,
                                                                    Chars::closeParen)));

    if (commitStatus.failed())
    {
        jam::Array<juce::Result> failures;
        failures.add (commitStatus);
        return Stdio::getOutput (Stdio::exitError, std::move (appliedSummary), std::move (failures));
    }

    return Stdio::getOutput (Stdio::exitSuccess, std::move (appliedSummary));
}

std::pair<juce::Result, std::string> Transaction::commit()
{
    const auto [stageStatus, txId] { UndoCache::addTransaction (commandName, stagedFiles) };

    if (stageStatus.failed())
        return { stageStatus, {} };

    const auto [commitStatus, failedPath, unrestoredPaths] { applyToStagedFiles (stagedFiles, txId, [&transactionId = txId] (const StagedFile& stagedFile)
    {
        return stagedFile.commit (transactionId);
    }) };

    if (commitStatus.failed())
    {
        if (unrestoredPaths.size() == 0)
        {
            UndoCache::removeTransaction (txId);
            return { Writer::getWriteFailure (failedPath, jam::Strings::getView (commitStatus.getErrorMessage())), {} };
        }

        return { getRollbackFailure (text::Diagnostics::failWrite, failedPath, commitStatus, unrestoredPaths), {} };
    }

    const auto publishStatus { UndoCache::publishTransaction (txId) };
    UndoCache::removeOldestTransactions();
    return { publishStatus, txId };
}
