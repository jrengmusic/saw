#include "Undo.h"
#include "Arguments.h"
#include "LineDiff.h"
#include "Stdio.h"
#include "TextDocument.h"
#include "UndoCache.h"
#include "Writer.h"
#include "generated/Generated.h"

using VerifiedEntries = std::tuple<juce::Result, jam::Array<juce::File>, jam::Array<juce::File>,
                                   jam::Array<std::unique_ptr<TextDocument>>, jam::Array<std::unique_ptr<TextDocument>>,
                                   jam::Array<bool>, jam::Array<bool>>;
using VerifiedEntry = std::tuple<juce::Result, juce::File, juce::File, std::unique_ptr<TextDocument>,
                                 std::unique_ptr<TextDocument>, bool, bool>;

static const jam::Document::Element& getRootObject (const jam::JsonDocument& document)
{
    const auto* rootElement { document.getChildByToken (*document.getRoot(), document.getValue()) };
    assert (rootElement != nullptr);
    return *rootElement;
}

static const jam::Document::Element& getFilesArray (const jam::JsonDocument& document, const jam::Document::Element& rootElement)
{
    const auto& filesToken { document.get (rootElement, Id::files) };
    const auto* filesElement { document.getChildByToken (rootElement, filesToken) };
    assert (filesElement != nullptr);
    return *filesElement;
}

static juce::String getMemberString (const jam::JsonDocument& document, const jam::Document::Element& element, std::string_view key)
{
    return jam::Strings::fromView (document.getString (document.get (element, key)));
}

static bool getMemberBoolean (const jam::JsonDocument& document, const jam::Document::Element& element, std::string_view key)
{
    return document.getBoolean (document.get (element, key));
}

static juce::Result getListArgumentStatus (const Arguments& arguments)
{
    const auto& positionals { arguments.getPositionals() };

    if (positionals.size() > 0)
        return juce::Result::fail (Stdio::getFormatted (text::Diagnostics::failUnexpectedArgument, positionals.at (0).toRawUTF8()));

    const std::vector<std::string_view> disallowedOptions { Id::applyOption, Id::forceOption };

    for (const auto option : disallowedOptions)
    {
        if (arguments.contains (option))
            return juce::Result::fail (Stdio::getFormatted (text::Diagnostics::failFlagUnknown, Arguments::getOptionName (option).toRawUTF8()));
    }

    return juce::Result::ok();
}

static std::string getTransactionSummary (const juce::String& txId, const jam::JsonDocument& metaDoc)
{
    const auto& rootObject { getRootObject (metaDoc) };
    const auto& filesArray { getFilesArray (metaDoc, rootObject) };
    const auto fileCount { filesArray.get<jam::Document::Tokens> (Id::tokens)->size() };
    const auto timestamp { getMemberString (metaDoc, rootObject, Id::timestamp) };
    const auto command { getMemberString (metaDoc, rootObject, Id::transactionCommand) };
    return Stdio::getLine (Stdio::getFormatted (text::Diagnostics::undoListFormat,
                                                txId.toRawUTF8(), timestamp.toRawUTF8(), fileCount, command.toRawUTF8()));
}

static Stdio::Output runList (const Arguments& arguments)
{
    const auto listStatus { getListArgumentStatus (arguments) };

    if (listStatus.failed())
        return Stdio::getFailure (listStatus.getErrorMessage());

    const auto transactionIds { UndoCache::getTransactionIds() };
    int failureCount { 0 };
    std::string printedOutput;
    std::string diagnostics;

    for (int index { 0 }; index < transactionIds.size(); ++index)
    {
        const auto [docStatus, metaDoc] { UndoCache::getTransactionDocument (transactionIds.at (index)) };

        if (docStatus.failed())
        {
            diagnostics += Stdio::getDiagnostic (docStatus.getErrorMessage());
            ++failureCount;
        }
        else
        {
            printedOutput += getTransactionSummary (transactionIds.at (index), *metaDoc);
        }
    }

    return { failureCount == 0 ? Stdio::exitSuccess : Stdio::exitError, std::move (printedOutput), std::move (diagnostics) };
}

static std::pair<juce::Result, juce::String> getSelectedTransactionId (const jam::Strings& positionals)
{
    if (positionals.size() > 1)
        return { juce::Result::fail (Stdio::getFormatted (text::Diagnostics::failUnexpectedArgument, positionals.at (1).toRawUTF8())), {} };

    if (positionals.size() == 1)
    {
        const auto transactionId { positionals.at (0) };

        if (transactionId.isEmpty() or juce::File::createLegalFileName (transactionId).compare (transactionId) != 0)
            return { juce::Result::fail (Stdio::getFormatted (text::Diagnostics::failUndoUnknown, transactionId.toRawUTF8())), {} };

        return { juce::Result::ok(), transactionId };
    }

    return { juce::Result::fail (text::Diagnostics::failUndoRequiresId), {} };
}

static std::pair<juce::Result, std::unique_ptr<TextDocument>> getVerifiedOriginalDocument (
    const juce::File& blobFile, const juce::File& targetFile, const juce::String& txId,
    const juce::String& canonicalPath, const juce::String& preHash, const juce::String& postHash)
{
    auto [originalStatus, originalDoc] { TextDocument::fromOwnedFile (blobFile, canonicalPath) };

    if (originalStatus.wasOk())
    {
        const auto blobHash { originalDoc->getContentHash() };

        if (blobHash.compare (preHash) == 0)
            return { juce::Result::ok(), std::move (originalDoc) };

        const auto [targetStatus, targetDoc] { TextDocument::fromFile (targetFile, canonicalPath) };

        if (blobHash.compare (postHash) == 0 or (targetStatus.wasOk() and targetDoc->getContentHash().compare (preHash) == 0))
            return { juce::Result::ok(), nullptr };
    }

    return { juce::Result::fail (Stdio::getFormatted (text::Diagnostics::failUndoCorrupt, txId.toRawUTF8(), blobFile.getFileName().toRawUTF8())), nullptr };
}

static std::pair<juce::Result, std::unique_ptr<TextDocument>> getExistingCurrentDocument (
    const juce::File& targetFile, const juce::String& txId, const juce::String& canonicalPath,
    const juce::String& postHash, bool isForce)
{
    auto [currentStatus, currentDoc] { TextDocument::fromOwnedFile (targetFile, canonicalPath) };

    if (currentStatus.failed())
        return { currentStatus, nullptr };

    if (not isForce and currentDoc->getContentHash().compare (postHash) != 0)
        return { juce::Result::fail (Stdio::getFormatted (text::Diagnostics::failUndoDrift, txId.toRawUTF8(), canonicalPath.toRawUTF8())), nullptr };

    return { juce::Result::ok(), std::move (currentDoc) };
}

static std::tuple<juce::Result, std::unique_ptr<TextDocument>, bool> getCurrentState (
    const juce::File& targetFile, const juce::String& txId, const juce::String& canonicalPath,
    const juce::String& postHash, bool isForce)
{
    if (targetFile.existsAsFile())
    {
        auto [status, currentDoc] { getExistingCurrentDocument (targetFile, txId, canonicalPath, postHash, isForce) };

        if (status.failed())
            return { status, nullptr, false };

        return { juce::Result::ok(), std::move (currentDoc), true };
    }

    if (not isForce)
        return { juce::Result::fail (Stdio::getFormatted (text::Diagnostics::failUndoMissing, txId.toRawUTF8(), canonicalPath.toRawUTF8())), nullptr, false };

    return { juce::Result::ok(), TextDocument::fromString ({}, canonicalPath), false };
}

static VerifiedEntry getVerifiedEntry (const jam::JsonDocument& metaDoc, const jam::Document::Element& entryElement,
                                       const juce::String& txId, int fileIndex, bool isForce)
{
    const auto canonicalPath { getMemberString (metaDoc, entryElement, jam::Strings::getView (Id::path.toString())) };
    const auto preHash { getMemberString (metaDoc, entryElement, Id::preHash) };
    const auto postHash { getMemberString (metaDoc, entryElement, Id::postHash) };
    const auto isExecutable { getMemberBoolean (metaDoc, entryElement, Id::executable) };
    const auto targetFile { juce::File::getCurrentWorkingDirectory().getChildFile (canonicalPath) };
    const auto blobFile { UndoCache::getBlobFile (txId, fileIndex) };
    VerifiedEntry entry { juce::Result::ok(), juce::File {}, juce::File {}, std::unique_ptr<TextDocument> {}, std::unique_ptr<TextDocument> {}, false, false };
    auto [originalStatus, originalDoc] { getVerifiedOriginalDocument (blobFile, targetFile, txId, canonicalPath, preHash, postHash) };

    if (originalStatus.failed())
    {
        std::get<0> (entry) = originalStatus;
        return entry;
    }

    if (originalDoc == nullptr)
        return entry;

    const auto writeStatus { Writer::getWriteStatus (targetFile, canonicalPath) };

    if (writeStatus.failed())
    {
        std::get<0> (entry) = writeStatus;
        return entry;
    }

    auto [currentStatus, currentDoc, existed] { getCurrentState (targetFile, txId, canonicalPath, postHash, isForce) };

    if (currentStatus.failed())
    {
        std::get<0> (entry) = currentStatus;
        return entry;
    }

    return { juce::Result::ok(), targetFile, blobFile, std::move (currentDoc), std::move (originalDoc), isExecutable, existed };
}

static VerifiedEntries getVerifiedEntries (const jam::JsonDocument& metaDoc, const juce::String& txId, bool isForce)
{
    jam::Array<juce::File> targetFiles;
    jam::Array<juce::File> blobFiles;
    jam::Array<std::unique_ptr<TextDocument>> currentDocuments;
    jam::Array<std::unique_ptr<TextDocument>> originalDocuments;
    jam::Array<bool> executableFlags;
    jam::Array<bool> existedBeforeUndo;
    const auto& filesArray { getFilesArray (metaDoc, getRootObject (metaDoc)) };
    const auto& entryTokens { *filesArray.get<jam::Document::Tokens> (Id::tokens) };

    for (int index { 0 }; index < entryTokens.size(); ++index)
    {
        const auto* entryElement { metaDoc.getChildByToken (filesArray, entryTokens.at (index)) };
        assert (entryElement != nullptr);
        auto [entryStatus, targetFile, blobFile, currentDoc, originalDoc, isExecutable, existed] {
            getVerifiedEntry (metaDoc, *entryElement, txId, index, isForce) };

        if (entryStatus.failed())
            return { entryStatus, jam::Array<juce::File> {}, jam::Array<juce::File> {}, jam::Array<std::unique_ptr<TextDocument>> {},
                     jam::Array<std::unique_ptr<TextDocument>> {}, jam::Array<bool> {}, jam::Array<bool> {} };

        if (originalDoc != nullptr)
        {
            targetFiles.add (targetFile);
            blobFiles.add (blobFile);
            currentDocuments.add (std::move (currentDoc));
            originalDocuments.add (std::move (originalDoc));
            executableFlags.add (isExecutable);
            existedBeforeUndo.add (existed);
        }
    }

    return { juce::Result::ok(), std::move (targetFiles), std::move (blobFiles), std::move (currentDocuments),
             std::move (originalDocuments), std::move (executableFlags), std::move (existedBeforeUndo) };
}

static Stdio::Output runDryRun (const juce::String& txId, const jam::Array<juce::File>& targetFiles,
                                const jam::Array<std::unique_ptr<TextDocument>>& currentDocuments,
                                const jam::Array<std::unique_ptr<TextDocument>>& originalDocuments)
{
    std::string combinedDiff;

    for (int index { 0 }; index < targetFiles.size(); ++index)
    {
        const auto& currentDoc { *currentDocuments.at (index) };
        const auto& originalDoc { *originalDocuments.at (index) };
        combinedDiff.append (LineDiff (currentDoc, originalDoc).getUnifiedDiff (currentDoc, originalDoc.getBytes()));
    }

    combinedDiff += Stdio::getDiagnostic (Stdio::getFormatted (text::Diagnostics::summaryDryRunUndo, txId.toRawUTF8(), targetFiles.size()));
    return { Stdio::exitSuccess, std::move (combinedDiff), {} };
}

static bool restoreFile (const juce::File& targetFile, const juce::File& blobFile,
                         const TextDocument& currentDoc, const TextDocument& originalDoc, bool isExisting)
{
    if (not isExisting)
        return targetFile.deleteFile();

    const bool isWritten { TextDocument::getBytesStatus (blobFile, originalDoc.getDisplayPath(), originalDoc.getBytes(), Id::preHash).wasOk() };

    if (isWritten)
        return TextDocument::getBytesStatus (targetFile, originalDoc.getDisplayPath(), originalDoc.getBytes(), Id::preHash).wasOk()
               and Writer::toFile (targetFile, currentDoc.getBytes()).wasOk();

    if (jam::File::swapFiles (targetFile, blobFile))
    {
        if (TextDocument::getBytesStatus (blobFile, originalDoc.getDisplayPath(), originalDoc.getBytes(), Id::preHash).wasOk())
            return true;

        jam::File::swapFiles (targetFile, blobFile);
    }

    return false;
}

static jam::Strings rollbackFiles (int rollbackCount, const jam::Array<juce::File>& targetFiles,
                                   const jam::Array<juce::File>& blobFiles,
                                   const jam::Array<std::unique_ptr<TextDocument>>& currentDocuments,
                                   const jam::Array<std::unique_ptr<TextDocument>>& originalDocuments,
                                   const jam::Array<bool>& existedBeforeUndo)
{
    jam::Strings failedPaths;

    for (int index { 0 }; index < rollbackCount; ++index)
    {
        const auto& currentDoc { *currentDocuments.at (index) };
        const bool isRestored { restoreFile (targetFiles.at (index), blobFiles.at (index), currentDoc,
                                             *originalDocuments.at (index), existedBeforeUndo.at (index)) };

        if (not isRestored)
            failedPaths.add (currentDoc.getDisplayPath());
    }

    return failedPaths;
}

static std::pair<juce::Result, int> restoreAndVerifyFile (const juce::File& targetFile, const juce::File& blobFile,
                                                          const TextDocument& currentDoc, const TextDocument& originalDoc,
                                                          bool isExecutable, bool isExisting)
{
    const bool isSwapped { isExisting
                           and jam::File::isSwappable (targetFile, blobFile)
                           and jam::File::setMetadata (blobFile, targetFile)
                           and blobFile.setLastModificationTime (juce::Time::getCurrentTime())
                           and jam::File::swapFiles (targetFile, blobFile) };

    if (isSwapped)
    {
        const auto swapStatus { TextDocument::getBytesStatus (blobFile, currentDoc.getDisplayPath(), currentDoc.getBytes(), text::Diagnostics::failChecksumMismatch) };

        if (swapStatus.failed())
            return { swapStatus, 1 };
    }
    else
    {
        const auto writeResult { Writer::toFile (targetFile, originalDoc.getBytes()) };

        if (writeResult.failed())
        {
            const bool isUnchanged { isExisting
                                     ? TextDocument::getBytesStatus (targetFile, currentDoc.getDisplayPath(), currentDoc.getBytes(), text::Diagnostics::failChecksumMismatch).wasOk()
                                     : not targetFile.existsAsFile() };
            return { writeResult, isUnchanged ? 0 : 1 };
        }
    }

    if (not isExisting and Writer::isExecutable (targetFile) != isExecutable)
    {
        if (not targetFile.setExecutePermission (isExecutable))
            return { juce::Result::fail (text::Diagnostics::failOverwriteFailed), 1 };
    }

    const auto restoreStatus { TextDocument::getBytesStatus (targetFile, originalDoc.getDisplayPath(), originalDoc.getBytes(), text::Diagnostics::failChecksumMismatch) };

    if (restoreStatus.failed())
        return { restoreStatus, 1 };

    return { juce::Result::ok(), 0 };
}

static Stdio::Output runApply (const juce::String& txId, bool isMovedFromUndo,
                               const jam::Array<juce::File>& targetFiles,
                               const jam::Array<juce::File>& blobFiles,
                               const jam::Array<std::unique_ptr<TextDocument>>& currentDocuments,
                               const jam::Array<std::unique_ptr<TextDocument>>& originalDocuments,
                               const jam::Array<bool>& executableFlags, const jam::Array<bool>& existedBeforeUndo)
{
    for (int index { 0 }; index < targetFiles.size(); ++index)
    {
        const auto& originalDoc { *originalDocuments.at (index) };
        const auto [restoreResult, rollbackOffset] { restoreAndVerifyFile (
            targetFiles.at (index), blobFiles.at (index), *currentDocuments.at (index), originalDoc,
            executableFlags.at (index), existedBeforeUndo.at (index)) };

        if (restoreResult.failed())
        {
            const auto failedPaths { rollbackFiles (index + rollbackOffset, targetFiles, blobFiles,
                                                    currentDocuments, originalDocuments, existedBeforeUndo) };

            if (isMovedFromUndo)
                juce::ignoreUnused (UndoCache::publishTransaction (txId));

            const auto backupDirectory { UndoCache::getBlobDirectory().getFullPathName() };

            if (failedPaths.size() == 0)
                return Stdio::getFailure (Stdio::getFormatted (
                    text::Diagnostics::failUndoWrite, originalDoc.getDisplayPath().toRawUTF8(), restoreResult.getErrorMessage().toRawUTF8()));

            return Stdio::getFailure (Stdio::getFormatted (
                text::Diagnostics::failUndoRollback, originalDoc.getDisplayPath().toRawUTF8(), restoreResult.getErrorMessage().toRawUTF8(),
                failedPaths.joinIntoString (", ").toRawUTF8(), backupDirectory.toRawUTF8()));
        }
    }

    UndoCache::removeTransaction (txId);
    return { Stdio::exitSuccess, Stdio::getDiagnostic (Stdio::getFormatted (text::Diagnostics::summaryUndone, txId.toRawUTF8(), targetFiles.size())), {} };
}

static Stdio::Output runTransaction (const Arguments& arguments)
{
    const auto [selectStatus, txId] { getSelectedTransactionId (arguments.getPositionals()) };

    if (selectStatus.failed())
        return Stdio::getFailure (selectStatus.getErrorMessage());

    const auto isApply { arguments.contains (Id::applyOption) };
    const auto isForce { arguments.contains (Id::forceOption) };
    const auto [claimStatus, isMovedFromUndo] { isApply ? UndoCache::claimTransaction (txId, isForce)
                                                        : std::pair<juce::Result, bool> { juce::Result::ok(), false } };

    if (claimStatus.failed())
        return Stdio::getFailure (claimStatus.getErrorMessage());

    const auto rejectTransaction { [] (const juce::Result& status, const juce::String& id, bool isPublishedBack) -> Stdio::Output
    {
        if (isPublishedBack)
            juce::ignoreUnused (UndoCache::publishTransaction (id));

        return Stdio::getFailure (status.getErrorMessage());
    } };

    const auto [loadStatus, metaDoc] { UndoCache::getTransactionDocument (txId) };

    if (loadStatus.failed())
        return rejectTransaction (loadStatus, txId, isMovedFromUndo);

    auto [verifyStatus, targetFiles, blobFiles, currentDocuments, originalDocuments, executableFlags, existedBeforeUndo] {
        getVerifiedEntries (*metaDoc, txId, isForce) };

    if (verifyStatus.failed())
        return rejectTransaction (verifyStatus, txId, isMovedFromUndo);

    if (isApply)
        return runApply (txId, isMovedFromUndo, targetFiles, blobFiles, currentDocuments, originalDocuments, executableFlags, existedBeforeUndo);

    return runDryRun (txId, targetFiles, currentDocuments, originalDocuments);
}

Arguments Undo::getArguments (const jam::Strings& argumentList)
{
    const std::vector<std::string_view> flagOptions { Id::listOption, Id::applyOption, Id::forceOption };

    return Arguments { argumentList, flagOptions, {}, {} };
}

Stdio::Output Undo::run (const Arguments& arguments, const std::optional<std::string>&)
{
    if (arguments.getStatus().failed())
        return Stdio::getFailure (arguments.getStatus().getErrorMessage());

    if (arguments.contains (Id::listOption))
        return runList (arguments);

    return runTransaction (arguments);
}
