#include "Undo.h"
#include "Arguments.h"
#include "LineDiff.h"
#include "Stdio.h"
#include "TextDocument.h"
#include "UndoCache.h"
#include "Writer.h"
#include "generated/Generated.h"

using VerifiedEntries = std::tuple<juce::Result, jam::Array<juce::File>, jam::Array<std::unique_ptr<TextDocument>>,
                                   jam::Array<std::unique_ptr<TextDocument>>, jam::Strings, jam::Array<bool>,
                                   jam::Array<bool>, jam::Array<bool>>;
using VerifiedEntry = std::tuple<juce::Result, juce::File, std::unique_ptr<TextDocument>,
                                 std::unique_ptr<TextDocument>, juce::String, bool, bool, bool>;

static const jam::Document::Element& getRootObject (const jam::JsonDocument& document)
{
    const auto* rootElement { document.getChildByToken (*document.getRoot(), document.getValue()) };
    assert (rootElement != nullptr);
    return *rootElement;
}

static const jam::Document::Element& getFilesArray (const jam::JsonDocument& document, const jam::Document::Element& rootElement)
{
    const auto& filesToken { document.get (rootElement, jam::Strings::getView (Id::files.toString())) };
    const auto* filesElement { document.getChildByToken (rootElement, filesToken) };
    assert (filesElement != nullptr);
    return *filesElement;
}

static juce::String getMemberString (const jam::JsonDocument& document, const jam::Document::Element& element, const juce::Identifier& key)
{
    return jam::Strings::fromView (document.getString (document.get (element, jam::Strings::getView (key.toString()))));
}

static bool getMemberBoolean (const jam::JsonDocument& document, const jam::Document::Element& element, const juce::Identifier& key)
{
    return document.getBoolean (document.get (element, jam::Strings::getView (key.toString())));
}

static juce::Result getListArgumentStatus (const Arguments& arguments)
{
    const auto& positionals { arguments.getPositionals() };

    if (positionals.size() > 0)
        return juce::Result::fail (juce::String::formatted (text::Diagnostics::failUnexpectedArgument, positionals.at (0).toRawUTF8()));

    const jam::Strings disallowedOptions { Id::applyOption, Id::forceOption };

    for (int index { 0 }; index < disallowedOptions.size(); ++index)
    {
        const auto option { disallowedOptions.at (index) };

        if (arguments.contains (option))
            return juce::Result::fail (juce::String::formatted (text::Diagnostics::failFlagUnknown, option.toRawUTF8()));
    }

    return juce::Result::ok();
}

static void printTransactionSummary (const jam::JsonDocument& metaDoc)
{
    const auto& rootObject { getRootObject (metaDoc) };
    const auto& filesArray { getFilesArray (metaDoc, rootObject) };
    const auto fileCount { filesArray.get<jam::Document::Tokens> (Id::tokens)->size() };
    const auto id { getMemberString (metaDoc, rootObject, Id::id) };
    const auto timestamp { getMemberString (metaDoc, rootObject, Id::timestamp) };
    const auto command { getMemberString (metaDoc, rootObject, Id::command) };
    Stdio::print (juce::String::formatted (text::Diagnostics::undoListFormat,
                                           id.toRawUTF8(), timestamp.toRawUTF8(), fileCount, command.toRawUTF8()));
}

static int runList (const Arguments& arguments)
{
    const auto listStatus { getListArgumentStatus (arguments) };

    if (listStatus.failed())
    {
        Stdio::printError (listStatus.getErrorMessage());
        return Stdio::exitError;
    }

    const auto transactionIds { UndoCache::getTransactionIds() };

    for (int index { 0 }; index < transactionIds.size(); ++index)
    {
        const auto [docStatus, metaDoc] { UndoCache::getTransactionDocument (transactionIds.at (index)) };

        if (docStatus.failed())
        {
            Stdio::printError (docStatus.getErrorMessage());
            return Stdio::exitError;
        }

        printTransactionSummary (*metaDoc);
    }

    return Stdio::exitSuccess;
}

static std::pair<juce::Result, juce::String> getSelectedTransactionId (const jam::Strings& positionals)
{
    if (positionals.size() > 1)
        return { juce::Result::fail (juce::String::formatted (text::Diagnostics::failUnexpectedArgument, positionals.at (1).toRawUTF8())), {} };

    if (positionals.size() == 1)
        return { juce::Result::ok(), positionals.at (0) };

    const auto ids { UndoCache::getTransactionIds() };

    if (ids.size() == 0)
        return { juce::Result::fail (text::Diagnostics::failUndoEmpty), {} };

    return { juce::Result::ok(), ids.at (0) };
}

static std::tuple<juce::Result, juce::String, std::unique_ptr<jam::JsonDocument>> getLoadedTransaction (const jam::Strings& positionals)
{
    const auto [selectStatus, txId] { getSelectedTransactionId (positionals) };

    if (selectStatus.failed())
        return { selectStatus, juce::String {}, std::unique_ptr<jam::JsonDocument> {} };

    auto [docStatus, metaDoc] { UndoCache::getTransactionDocument (txId) };

    if (docStatus.failed())
        return { docStatus, juce::String {}, std::unique_ptr<jam::JsonDocument> {} };

    return { juce::Result::ok(), txId, std::move (metaDoc) };
}

static std::pair<juce::Result, std::unique_ptr<TextDocument>> getVerifiedOriginalDocument (
    const juce::File& txDir, const juce::String& txId, const juce::String& blobName,
    const juce::String& canonicalPath, const juce::String& preHash)
{
    auto [originalStatus, originalDoc] { TextDocument::fromFile (txDir.getChildFile (blobName), canonicalPath) };

    if (originalStatus.failed() or originalDoc->getContentHash().compare (preHash) != 0)
        return { juce::Result::fail (juce::String::formatted (text::Diagnostics::failUndoCorrupt, txId.toRawUTF8(), blobName.toRawUTF8())), nullptr };

    return { juce::Result::ok(), std::move (originalDoc) };
}

static std::pair<juce::Result, std::unique_ptr<TextDocument>> getExistingCurrentDocument (
    const juce::File& targetFile, const juce::String& txId, const juce::String& canonicalPath,
    const juce::String& postHash, bool isForce)
{
    auto [currentStatus, currentDoc] { TextDocument::fromFile (targetFile, canonicalPath) };

    if (currentStatus.failed())
        return { currentStatus, nullptr };

    if (not isForce and currentDoc->getContentHash().compare (postHash) != 0)
        return { juce::Result::fail (juce::String::formatted (text::Diagnostics::failUndoDrift, txId.toRawUTF8(), canonicalPath.toRawUTF8())), nullptr };

    return { juce::Result::ok(), std::move (currentDoc) };
}

static std::tuple<juce::Result, std::unique_ptr<TextDocument>, bool, bool> getCurrentState (
    const juce::File& targetFile, const juce::String& txId, const juce::String& canonicalPath,
    const juce::String& postHash, bool isForce)
{
    if (targetFile.existsAsFile())
    {
        auto [status, currentDoc] { getExistingCurrentDocument (targetFile, txId, canonicalPath, postHash, isForce) };

        if (status.failed())
            return { status, nullptr, false, false };

        return { juce::Result::ok(), std::move (currentDoc), Writer::isExecutable (targetFile), true };
    }

    if (not isForce)
        return { juce::Result::fail (juce::String::formatted (text::Diagnostics::failUndoMissing, txId.toRawUTF8(), canonicalPath.toRawUTF8())), nullptr, false, false };

    return { juce::Result::ok(), TextDocument::fromString ({}, canonicalPath), false, false };
}

static VerifiedEntry getVerifiedEntry (const jam::JsonDocument& metaDoc, const jam::Document::Element& entryElement,
                                       const juce::String& txId, const juce::File& txDir, bool isForce)
{
    const auto canonicalPath { getMemberString (metaDoc, entryElement, Id::path) };
    const auto blobName { getMemberString (metaDoc, entryElement, Id::blob) };
    const auto preHash { getMemberString (metaDoc, entryElement, Id::preHash) };
    const auto postHash { getMemberString (metaDoc, entryElement, Id::postHash) };
    const auto isExecutable { getMemberBoolean (metaDoc, entryElement, Id::executable) };
    auto [originalStatus, originalDoc] { getVerifiedOriginalDocument (txDir, txId, blobName, canonicalPath, preHash) };

    if (originalStatus.failed())
        return { originalStatus, juce::File {}, std::unique_ptr<TextDocument> {}, std::unique_ptr<TextDocument> {},
                 juce::String {}, false, false, false };

    const auto targetFile { juce::File::getCurrentWorkingDirectory().getChildFile (canonicalPath) };
    auto [currentStatus, currentDoc, wasExecutable, existed] { getCurrentState (targetFile, txId, canonicalPath, postHash, isForce) };

    if (currentStatus.failed())
        return { currentStatus, juce::File {}, std::unique_ptr<TextDocument> {}, std::unique_ptr<TextDocument> {},
                 juce::String {}, false, false, false };

    return { juce::Result::ok(), targetFile, std::move (currentDoc), std::move (originalDoc), preHash, isExecutable, wasExecutable, existed };
}

static VerifiedEntries getVerifiedEntries (const jam::JsonDocument& metaDoc, const juce::String& txId,
                                           const juce::File& txDir, bool isForce)
{
    jam::Array<juce::File> targetFiles;
    jam::Array<std::unique_ptr<TextDocument>> currentDocuments;
    jam::Array<std::unique_ptr<TextDocument>> originalDocuments;
    jam::Strings preHashes;
    jam::Array<bool> executableFlags;
    jam::Array<bool> preUndoExecutableFlags;
    jam::Array<bool> existedBeforeUndo;
    const auto& filesArray { getFilesArray (metaDoc, getRootObject (metaDoc)) };
    const auto& entryTokens { *filesArray.get<jam::Document::Tokens> (Id::tokens) };

    for (int index { 0 }; index < entryTokens.size(); ++index)
    {
        const auto* entryElement { metaDoc.getChildByToken (filesArray, entryTokens.at (index)) };
        assert (entryElement != nullptr);
        auto [entryStatus, targetFile, currentDoc, originalDoc, preHash, isExecutable, wasExecutable, existed] {
            getVerifiedEntry (metaDoc, *entryElement, txId, txDir, isForce) };

        if (entryStatus.failed())
            return { entryStatus, jam::Array<juce::File> {}, jam::Array<std::unique_ptr<TextDocument>> {},
                     jam::Array<std::unique_ptr<TextDocument>> {}, jam::Strings {}, jam::Array<bool> {},
                     jam::Array<bool> {}, jam::Array<bool> {} };

        targetFiles.add (targetFile);
        currentDocuments.add (std::move (currentDoc));
        originalDocuments.add (std::move (originalDoc));
        preHashes.add (preHash);
        executableFlags.add (isExecutable);
        preUndoExecutableFlags.add (wasExecutable);
        existedBeforeUndo.add (existed);
    }

    return { juce::Result::ok(), std::move (targetFiles), std::move (currentDocuments), std::move (originalDocuments),
             std::move (preHashes), std::move (executableFlags), std::move (preUndoExecutableFlags), std::move (existedBeforeUndo) };
}

static int runDryRun (const juce::String& txId, const jam::Array<juce::File>& targetFiles,
                      const jam::Array<std::unique_ptr<TextDocument>>& currentDocuments,
                      const jam::Array<std::unique_ptr<TextDocument>>& originalDocuments)
{
    std::string combinedDiff;

    for (int index { 0 }; index < targetFiles.size(); ++index)
    {
        const auto& currentDoc { *currentDocuments.at (index) };
        const auto& originalDoc { *originalDocuments.at (index) };
        combinedDiff.append (LineDiff (currentDoc, originalDoc).getUnifiedDiff (currentDoc, originalDoc));
    }

    if (not combinedDiff.empty())
        Stdio::write (combinedDiff);

    Stdio::printSummary (Stdio::getFormatted (text::Diagnostics::summaryDryRunUndo, txId.toRawUTF8(), targetFiles.size()));
    return Stdio::exitSuccess;
}

static void rollbackFiles (int rollbackCount, const jam::Array<juce::File>& targetFiles,
                           const jam::Array<std::unique_ptr<TextDocument>>& currentDocuments,
                           const jam::Array<bool>& preUndoExecutableFlags, const jam::Array<bool>& existedBeforeUndo)
{
    for (int index { 0 }; index < rollbackCount; ++index)
    {
        if (existedBeforeUndo.at (index))
            Writer::toFile (targetFiles.at (index), currentDocuments.at (index)->getBytes(), preUndoExecutableFlags.at (index));
        else
            targetFiles.at (index).deleteFile();
    }
}

static std::pair<juce::Result, int> restoreAndVerifyFile (const juce::File& targetFile, const TextDocument& originalDoc,
                                                          bool isExecutable, const juce::String& preHash)
{
    const auto writeResult { Writer::toFile (targetFile, originalDoc.getBytes(), isExecutable) };

    if (writeResult.failed())
        return { writeResult, 0 };

    const auto [readStatus, writtenDoc] { TextDocument::fromFile (targetFile, originalDoc.getDisplayPath()) };

    if (readStatus.failed())
        return { readStatus, 1 };

    if (writtenDoc->getContentHash().compare (preHash) != 0)
        return { juce::Result::fail (text::Diagnostics::failChecksumMismatch), 1 };

    return { juce::Result::ok(), 0 };
}

static int runApply (const juce::String& txId, const juce::File& txDir, const jam::Array<juce::File>& targetFiles,
                     const jam::Array<std::unique_ptr<TextDocument>>& currentDocuments,
                     const jam::Array<std::unique_ptr<TextDocument>>& originalDocuments, const jam::Strings& preHashes,
                     const jam::Array<bool>& executableFlags, const jam::Array<bool>& preUndoExecutableFlags,
                     const jam::Array<bool>& existedBeforeUndo)
{
    for (int index { 0 }; index < targetFiles.size(); ++index)
    {
        const auto& originalDoc { *originalDocuments.at (index) };
        const auto [restoreResult, rollbackOffset] { restoreAndVerifyFile (
            targetFiles.at (index), originalDoc, executableFlags.at (index), preHashes.at (index)) };

        if (restoreResult.failed())
        {
            rollbackFiles (index + rollbackOffset, targetFiles, currentDocuments, preUndoExecutableFlags, existedBeforeUndo);
            Stdio::printError (juce::String::formatted (
                text::Diagnostics::failUndoWrite, originalDoc.getDisplayPath().toRawUTF8(), restoreResult.getErrorMessage().toRawUTF8()));
            return Stdio::exitError;
        }
    }

    txDir.deleteRecursively (false);
    Stdio::printSummary (Stdio::getFormatted (text::Diagnostics::summaryUndone, txId.toRawUTF8(), targetFiles.size()));
    return Stdio::exitSuccess;
}

static int runTransaction (const Arguments& arguments)
{
    const auto [loadStatus, txId, metaDoc] { getLoadedTransaction (arguments.getPositionals()) };

    if (loadStatus.failed())
    {
        Stdio::printError (loadStatus.getErrorMessage());
        return Stdio::exitError;
    }

    const auto txDir { UndoCache::getDirectory().getChildFile (txId) };
    const auto isForce { arguments.contains (Id::forceOption) };
    auto [verifyStatus, targetFiles, currentDocuments, originalDocuments, preHashes, executableFlags,
          preUndoExecutableFlags, existedBeforeUndo] { getVerifiedEntries (*metaDoc, txId, txDir, isForce) };

    if (verifyStatus.failed())
    {
        Stdio::printError (verifyStatus.getErrorMessage());
        return Stdio::exitError;
    }

    if (arguments.contains (Id::applyOption))
        return runApply (txId, txDir, targetFiles, currentDocuments, originalDocuments, preHashes, executableFlags, preUndoExecutableFlags, existedBeforeUndo);

    return runDryRun (txId, targetFiles, currentDocuments, originalDocuments);
}

int Undo::run (const jam::Strings& argumentList)
{
    const Arguments arguments { argumentList, 1, jam::Strings { Id::listOption, Id::applyOption, Id::forceOption }, jam::Strings {} };

    if (arguments.getStatus().failed())
    {
        Stdio::printError (arguments.getStatus().getErrorMessage());
        return Stdio::exitError;
    }

    if (arguments.contains (Id::listOption))
        return runList (arguments);

    return runTransaction (arguments);
}
