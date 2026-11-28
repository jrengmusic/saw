#include "Undo.h"
#include "Arguments.h"
#include "Stdio.h"
#include "StagedFile.h"
#include "TextDocument.h"
#include "Transaction.h"
#include "UndoCache.h"
#include "Writer.h"
#include "generated/Generated.h"

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

static std::string_view getMemberString (const jam::JsonDocument& document, const jam::Document::Element& element, std::string_view key)
{
    return document.getString (document.get (element, key));
}

static bool getMemberBoolean (const jam::JsonDocument& document, const jam::Document::Element& element, std::string_view key)
{
    return document.getBoolean (document.get (element, key));
}

static juce::Result getListArgumentStatus (const Arguments& arguments)
{
    const auto& positionals { arguments.getPositionals() };

    if (positionals.size() > 0)
        return Stdio::fail (text::Diagnostics::failUnexpectedArgument, positionals.at (0));

    static constexpr std::array disallowedOptions { Id::applyOption, Id::forceOption };

    for (const auto option : disallowedOptions)
    {
        if (arguments.contains (option))
            return Stdio::fail (text::Diagnostics::failFlagUnknown, Arguments::getOptionName (option));
    }

    return juce::Result::ok();
}

static void appendTransactionSummary (std::string& output, std::string_view txId, const jam::JsonDocument& metaDoc)
{
    const auto& rootObject { getRootObject (metaDoc) };
    const auto& filesArray { getFilesArray (metaDoc, rootObject) };
    const auto fileCount { filesArray.get<jam::Document::Tokens> (Id::tokens)->size() };
    const auto timestamp { getMemberString (metaDoc, rootObject, Id::timestamp) };
    const auto command { getMemberString (metaDoc, rootObject, Id::transactionCommand) };
    jam::Format::appendBytes (output, txId);
    jam::Format::appendUtf8 (output, Chars::tab);
    jam::Format::appendBytes (output, timestamp);
    jam::Format::appendUtf8 (output, Chars::tab);
    jam::Format::appendDecimal (output, fileCount);
    jam::Format::appendBytes (output, text::Diagnostics::files);
    jam::Format::appendUtf8 (output, Chars::tab);
    jam::Format::appendBytes (output, command);
    jam::Format::appendUtf8 (output, Chars::newline);
}

static Stdio::Output runList (const Arguments& arguments)
{
    const auto listStatus { getListArgumentStatus (arguments) };

    if (listStatus.failed())
        return Stdio::getFailure (listStatus);

    jam::Array<juce::Result> diagnostics;
    std::string output;

    UndoCache::applyToTransactions ([&diagnostics, &output] (std::string_view transactionId, const juce::Result& status, const jam::JsonDocument* document)
    {
        if (status.failed())
            diagnostics.add (status);
        else
            appendTransactionSummary (output, transactionId, *document);
    });

    const int exitCode { diagnostics.isEmpty() ? Stdio::exitSuccess : Stdio::exitError };
    return Stdio::getOutput (exitCode, Stdio::getDocuments (std::move (output)), std::move (diagnostics));
}

static std::pair<juce::Result, std::string_view> getSelectedTransactionId (const jam::Array<std::string_view>& positionals)
{
    if (positionals.size() > 1)
        return { Stdio::fail (text::Diagnostics::failUnexpectedArgument, positionals.at (1)), {} };

    if (positionals.size() == 1)
    {
        const std::string_view transactionId { positionals.at (0) };
        const auto legalFileName { juce::File::createLegalFileName (jam::Strings::fromView (transactionId)) };

        if (transactionId.empty() or jam::Strings::getView (legalFileName).compare (transactionId) != 0)
            return { Stdio::fail (text::Diagnostics::failUndoUnknown, transactionId), {} };

        return { juce::Result::ok(), transactionId };
    }

    return { Stdio::fail (text::Diagnostics::failUndoRequiresId), {} };
}

static std::pair<juce::Result, std::unique_ptr<TextDocument>> getVerifiedOriginalDocument (
    const juce::File& blobFile, const juce::File& targetFile, std::string_view txId,
    std::string_view canonicalPath, std::string_view preHash, std::string_view postHash)
{
    auto [originalStatus, originalDoc] { TextDocument::fromFile (blobFile, canonicalPath, Input::allBytes, false) };

    if (originalStatus.wasOk())
    {
        const auto blobHash { originalDoc->getContentHash() };

        if (jam::Strings::getView (blobHash).compare (preHash) == 0)
            return { juce::Result::ok(), std::move (originalDoc) };

        const auto [targetStatus, targetDoc] { TextDocument::fromFile (targetFile, canonicalPath, Input::maxBufferedReadSize, false) };

        if (jam::Strings::getView (blobHash).compare (postHash) == 0 or (targetStatus.wasOk() and jam::Strings::getView (targetDoc->getContentHash()).compare (preHash) == 0))
            return { juce::Result::ok(), nullptr };
    }

    return { Stdio::fail (text::Diagnostics::failUndoCorrupt, txId, jam::Strings::getView (Id::diagnosticSeparator), jam::Strings::getView (blobFile.getFileName())), nullptr };
}

static std::pair<juce::Result, std::unique_ptr<TextDocument>> getExistingCurrentDocument (
    const juce::File& targetFile, std::string_view txId, std::string_view canonicalPath,
    std::string_view postHash, bool isForce)
{
    auto [currentStatus, currentDoc] { TextDocument::fromFile (targetFile, canonicalPath, Input::allBytes, false) };

    if (currentStatus.failed())
        return { currentStatus, nullptr };

    if (not isForce and jam::Strings::getView (currentDoc->getContentHash()).compare (postHash) != 0)
        return { Stdio::fail (text::Diagnostics::failUndo, txId, jam::Strings::getView (Id::diagnosticSeparator), canonicalPath, text::Diagnostics::undoDrift), nullptr };

    return { juce::Result::ok(), std::move (currentDoc) };
}

static std::tuple<juce::Result, std::unique_ptr<TextDocument>, bool> getCurrentState (
    const juce::File& targetFile, std::string_view txId, std::string_view canonicalPath,
    std::string_view postHash, bool isForce)
{
    if (targetFile.existsAsFile())
    {
        auto [status, currentDoc] { getExistingCurrentDocument (targetFile, txId, canonicalPath, postHash, isForce) };

        if (status.failed())
            return { status, nullptr, false };

        return { juce::Result::ok(), std::move (currentDoc), true };
    }

    if (not isForce)
        return { Stdio::fail (text::Diagnostics::failUndo, txId, jam::Strings::getView (Id::diagnosticSeparator), canonicalPath, text::Diagnostics::undoMissing), nullptr, false };

    return { juce::Result::ok(), TextDocument::fromInput (nullptr, canonicalPath), false };
}

static jam::Array<juce::File> getRecordFolders (const jam::JsonDocument& document, const jam::Document::Element& rootElement)
{
    jam::Array<juce::File> folders;

    if (document.contains (rootElement, Id::folders))
    {
        const auto* foldersElement { document.getChildByToken (rootElement, document.get (rootElement, Id::folders)) };
        const auto& folderTokens { *foldersElement->get<jam::Document::Tokens> (Id::tokens) };

        for (int index { 0 }; index < folderTokens.size(); ++index)
            folders.add (juce::File (jam::Strings::fromView (document.getString (folderTokens.at (index)))));
    }

    return folders;
}

static std::pair<juce::Result, std::optional<StagedFile>> getVerifiedExistingEntry (const jam::JsonDocument& metaDoc, const jam::Document::Element& entryElement,
                                                                                    std::string_view txId, int fileIndex, bool isForce)
{
    const auto canonicalPath { getMemberString (metaDoc, entryElement, jam::Strings::getView (Id::path.toString())) };
    const auto preHash { getMemberString (metaDoc, entryElement, Id::preHash) };
    const auto postHash { getMemberString (metaDoc, entryElement, Id::postHash) };
    const auto isExecutable { getMemberBoolean (metaDoc, entryElement, Id::executable) };
    const auto targetFile { juce::File::getCurrentWorkingDirectory().getChildFile (jam::Strings::fromView (canonicalPath)) };
    const auto blobFile { UndoCache::getBlobFile (txId, fileIndex) };
    auto [originalStatus, originalDoc] { getVerifiedOriginalDocument (blobFile, targetFile, txId, canonicalPath, preHash, postHash) };

    if (originalStatus.failed())
        return { originalStatus, std::optional<StagedFile> {} };

    if (originalDoc == nullptr)
        return { juce::Result::ok(), std::optional<StagedFile> {} };

    const auto writeStatus { Writer::getWriteStatus (targetFile, canonicalPath) };

    if (writeStatus.failed())
        return { writeStatus, std::optional<StagedFile> {} };

    auto [currentStatus, currentDoc, existed] { getCurrentState (targetFile, txId, canonicalPath, postHash, isForce) };

    if (currentStatus.failed())
        return { currentStatus, std::optional<StagedFile> {} };

    return { juce::Result::ok(), std::optional<StagedFile> { StagedFile (targetFile, std::move (currentDoc), std::move (originalDoc), isExecutable, existed, true, fileIndex) } };
}

static std::pair<juce::Result, std::optional<StagedFile>> getVerifiedCreatedEntry (const jam::JsonDocument& metaDoc, const jam::Document::Element& entryElement,
                                                                                   std::string_view txId, int fileIndex, bool isForce)
{
    const auto canonicalPath { getMemberString (metaDoc, entryElement, jam::Strings::getView (Id::path.toString())) };
    const auto postHash { getMemberString (metaDoc, entryElement, Id::postHash) };
    const auto isExecutable { getMemberBoolean (metaDoc, entryElement, Id::executable) };
    const auto targetFile { juce::File::getCurrentWorkingDirectory().getChildFile (jam::Strings::fromView (canonicalPath)) };
    const auto writeStatus { Writer::getWriteStatus (targetFile, canonicalPath) };

    if (writeStatus.failed())
        return { writeStatus, std::optional<StagedFile> {} };

    auto [currentStatus, currentDoc, existed] { getCurrentState (targetFile, txId, canonicalPath, postHash, isForce) };

    if (currentStatus.failed())
        return { currentStatus, std::optional<StagedFile> {} };

    if (existed)
        return { juce::Result::ok(), std::optional<StagedFile> { StagedFile (targetFile, std::move (currentDoc), TextDocument::fromInput (nullptr, canonicalPath), isExecutable, true, false, fileIndex) } };

    return { juce::Result::ok(), std::optional<StagedFile> {} };
}

static std::pair<juce::Result, std::optional<StagedFile>> getVerifiedEntry (const jam::JsonDocument& metaDoc, const jam::Document::Element& entryElement,
                                                                             std::string_view txId, int fileIndex, bool isForce)
{
    const bool isExisting { not metaDoc.contains (entryElement, Id::existing) or getMemberBoolean (metaDoc, entryElement, Id::existing) };

    if (isExisting)
        return getVerifiedExistingEntry (metaDoc, entryElement, txId, fileIndex, isForce);

    return getVerifiedCreatedEntry (metaDoc, entryElement, txId, fileIndex, isForce);
}

static std::pair<juce::Result, jam::Array<StagedFile>> getVerifiedEntries (const jam::JsonDocument& metaDoc, std::string_view txId, bool isForce)
{
    jam::Array<StagedFile> stagedFiles;
    const auto& filesArray { getFilesArray (metaDoc, getRootObject (metaDoc)) };
    const auto& entryTokens { *filesArray.get<jam::Document::Tokens> (Id::tokens) };

    for (int index { 0 }; index < entryTokens.size(); ++index)
    {
        const auto* entryElement { metaDoc.getChildByToken (filesArray, entryTokens.at (index)) };
        assert (entryElement != nullptr);
        auto [entryStatus, stagedFile] { getVerifiedEntry (metaDoc, *entryElement, txId, index, isForce) };

        if (entryStatus.failed())
            return { entryStatus, jam::Array<StagedFile> {} };

        if (stagedFile.has_value())
            stagedFiles.add (std::move (*stagedFile));
    }

    return { juce::Result::ok(), std::move (stagedFiles) };
}

static Stdio::Output runDryRun (std::string_view txId, const jam::Array<StagedFile>& stagedFiles)
{
    auto documents (Transaction::getDiffDocuments (stagedFiles));
    documents.add (TextDocument::fromText (Stdio::getDiagnostic (text::Diagnostics::summaryDryRunUndo,
                                                                 txId,
                                                                 text::Diagnostics::undoFiles,
                                                                 stagedFiles.size(),
                                                                 text::Diagnostics::applyToRestore), {}));
    return Stdio::getOutput (Stdio::exitSuccess, std::move (documents));
}

static Stdio::Output runApply (std::string_view txId, bool isMovedFromUndo, const jam::Array<StagedFile>& stagedFiles, const jam::Array<juce::File>& folders)
{
    const auto [applyStatus, failedPath, unrestoredPaths] { Transaction::applyToStagedFiles (stagedFiles, txId, {}, [&txId] (const StagedFile& stagedFile)
    {
        return stagedFile.undo (txId);
    }) };

    if (applyStatus.failed())
    {
        if (isMovedFromUndo)
            juce::ignoreUnused (UndoCache::publishTransaction (txId));

        if (unrestoredPaths.size() == 0)
            return Stdio::getFailure (text::Diagnostics::failUndoWrite,
                                      failedPath,
                                      jam::Strings::getView (Id::diagnosticSeparator),
                                      applyStatus,
                                      text::Diagnostics::rolledBack);

        return Stdio::getFailure (Transaction::getRollbackFailure (text::Diagnostics::failUndoWrite, failedPath, applyStatus, unrestoredPaths));
    }

    Transaction::removeEmptyFolders (folders);
    UndoCache::removeTransaction (txId);
    return Stdio::getOutput (Stdio::exitSuccess, Stdio::getDocuments (Stdio::getDiagnostic (text::Diagnostics::summaryUndone,
                                                                                            txId,
                                                                                            text::Diagnostics::undoneFiles,
                                                                                            stagedFiles.size(),
                                                                                            text::Diagnostics::files)));
}

static Stdio::Output runTransaction (const Arguments& arguments)
{
    const auto [selectStatus, txId] { getSelectedTransactionId (arguments.getPositionals()) };

    if (selectStatus.failed())
        return Stdio::getFailure (selectStatus);

    const auto isApply { arguments.contains (Id::applyOption) };
    const auto isForce { arguments.contains (Id::forceOption) };
    const auto [claimStatus, isMovedFromUndo] { isApply ? UndoCache::claimTransaction (txId, isForce)
                                                        : std::pair<juce::Result, bool> { juce::Result::ok(), false } };

    if (claimStatus.failed())
        return Stdio::getFailure (claimStatus);

    const auto rejectTransaction { [] (const juce::Result& status, std::string_view id, bool isPublishedBack) -> Stdio::Output
    {
        if (isPublishedBack)
            juce::ignoreUnused (UndoCache::publishTransaction (id));

        return Stdio::getFailure (status);
    } };

    const auto [loadStatus, metaDoc] { UndoCache::getTransactionDocument (txId) };

    if (loadStatus.failed())
        return rejectTransaction (loadStatus, txId, isMovedFromUndo);

    auto [verifyStatus, stagedFiles] { getVerifiedEntries (*metaDoc, txId, isForce) };

    if (verifyStatus.failed())
        return rejectTransaction (verifyStatus, txId, isMovedFromUndo);

    if (isApply)
        return runApply (txId, isMovedFromUndo, stagedFiles, getRecordFolders (*metaDoc, getRootObject (*metaDoc)));

    return runDryRun (txId, stagedFiles);
}

Arguments Undo::getArguments (juce::Span<const std::string_view> argumentList)
{
    return Arguments { argumentList, { Id::listOption, Id::applyOption, Id::forceOption }, {}, {} };
}

Stdio::Output Undo::run (const Arguments& arguments, const Input::Source&)
{
    if (arguments.getStatus().failed())
        return Stdio::getFailure (arguments.getStatus());

    if (arguments.contains (Id::listOption))
        return runList (arguments);

    return runTransaction (arguments);
}
