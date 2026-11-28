#include "UndoCache.h"
#include "Jobs.h"
#include "Paths.h"
#include "Stdio.h"
#include "Writer.h"
#include "generated/Generated.h"
#include <chrono>
#include <ctime>

static constexpr int hashesPerFile { 2 };

static juce::String getRecordName (std::string_view transactionId)
{
    return jam::Strings::fromView (transactionId) + jam::Strings::fromView (Id::recordSuffix);
}

static jam::Strings getSortedIds (const juce::File& directory)
{
    jam::Strings ids;

    Paths::applyToFileNames (directory, [&ids] (std::string_view name)
    {
        if (name.size() > Id::recordSuffix.size() and name.substr (name.size() - Id::recordSuffix.size()).compare (Id::recordSuffix) == 0)
            ids.add (name.substr (0, name.size() - Id::recordSuffix.size()));
    });

    ids.sort (false);
    return ids;
}

static juce::Result writeBlob (const juce::File& blobFile, const StagedFile& stagedFile)
{
    if (not stagedFile.isExisting())
        return juce::Result::ok();

    if (jam::File::isSwappable (stagedFile.getTargetFile(), blobFile.getParentDirectory()))
        return Writer::toStagedFile (blobFile, stagedFile.getTargetFile(), stagedFile.getPlannedBytes());

    return Writer::toNewFile (blobFile, stagedFile.getOriginalBytes());
}

static jam::Array<juce::SHA256> getContentHashes (const jam::Array<StagedFile>& stagedFiles)
{
    static constexpr size_t parallelHashSize { 256 * 1024 };
    const int fileCount { stagedFiles.size() };
    const int hashCount { fileCount * hashesPerFile };
    jam::Array<juce::SHA256> hashes;
    hashes.resize (hashCount);
    size_t byteCount { 0 };

    for (int index { 0 }; index < fileCount; ++index)
        byteCount += stagedFiles.at (index).getOriginalBytes().size() + stagedFiles.at (index).getPlannedBytes().size();

    const auto setHash { [&hashes, &stagedFiles] (int index)
    {
        const int fileIndex { index / hashesPerFile };
        const auto& stagedFile { stagedFiles.at (fileIndex) };
        const std::string_view contents { index % hashesPerFile == 0 ? stagedFile.getOriginalBytes()
                                                                     : stagedFile.getPlannedBytes() };
        hashes.at (index) = juce::SHA256 (contents.data(), contents.size());
    } };

    if (byteCount >= parallelHashSize)
    {
        Jobs::run (hashCount, setHash);
    }
    else
    {
        for (int index { 0 }; index < hashCount; ++index)
            setHash (index);
    }

    return hashes;
}

static void appendStringMember (juce::OutputStream& json,
                                std::string_view key,
                                int index,
                                std::string_view value)
{
    jam::JsonWriter::appendKey (json, key, index);
    jam::JsonWriter::appendString (json, value, juce::JSON::Encoding::utf8);
}

static void appendFolderEntries (juce::OutputStream& entries, const jam::Array<juce::File>& folders)
{
    for (int index { 0 }; index < folders.size(); ++index)
    {
        jam::JsonWriter::appendSeparator (entries, index);
        jam::JsonWriter::appendString (entries, jam::Strings::getView (folders.at (index).getFullPathName()), juce::JSON::Encoding::utf8);
    }
}

static void appendFileEntries (juce::OutputStream& entries,
                               const jam::Array<StagedFile>& stagedFiles,
                               const jam::Array<juce::SHA256>& hashes)
{
    for (int index { 0 }; index < stagedFiles.size(); ++index)
    {
        const int hashIndex { index * hashesPerFile };
        jam::JsonWriter::appendSeparator (entries, index);
        jam::JsonWriter::appendObject (entries,
            [&stagedFiles, &hashes, index, hashIndex] (juce::OutputStream& entry)
            {
                const auto& stagedFile { stagedFiles.at (index) };
                const bool isExisting { stagedFile.isExisting() };
                const int postHashMember { isExisting ? 3 : 2 };
                appendStringMember (entry, jam::Strings::getView (Id::path.toString()), 0, jam::Strings::getView (stagedFile.getTargetFile().getFullPathName()));
                jam::JsonWriter::appendKey (entry, Id::existing, 1);
                jam::JsonWriter::appendBoolean (entry, isExisting);

                if (isExisting)
                    appendStringMember (entry, Id::preHash, 2, jam::Strings::getView (hashes.at (hashIndex).toHexString()));

                appendStringMember (entry, Id::postHash, postHashMember, jam::Strings::getView (hashes.at (hashIndex + 1).toHexString()));
                jam::JsonWriter::appendKey (entry, Id::executable, postHashMember + 1);
                jam::JsonWriter::appendBoolean (entry, stagedFile.isExecutable());
            });
    }
}

static void appendTwoDigits (std::string& text, int value)
{
    static constexpr int firstTwoDigitValue { 10 };

    if (value < firstTwoDigitValue)
        jam::Format::appendUtf8 (text, Chars::zero);

    jam::Format::appendDecimal (text, value);
}

static std::string getUtcText (const std::tm& utc, std::string_view dateSeparator, std::string_view timeSeparator)
{
    static constexpr int firstYear { 1900 };
    std::string text;
    jam::Format::appendDecimal (text, utc.tm_year + firstYear);
    jam::Format::appendBytes (text, dateSeparator);
    appendTwoDigits (text, utc.tm_mon + 1);
    jam::Format::appendBytes (text, dateSeparator);
    appendTwoDigits (text, utc.tm_mday);
    jam::Format::appendUtf8 (text, Chars::upperT);
    appendTwoDigits (text, utc.tm_hour);
    jam::Format::appendBytes (text, timeSeparator);
    appendTwoDigits (text, utc.tm_min);
    jam::Format::appendBytes (text, timeSeparator);
    appendTwoDigits (text, utc.tm_sec);
    return text;
}

static bool hasTypedMembers (const jam::JsonDocument& doc,
                             const jam::Document::Element& object,
                             std::initializer_list<std::pair<std::string_view, int>> requiredMembers)
{
    for (const auto& [key, expectedType] : requiredMembers)
    {
        if (not doc.contains (object, key) or doc.get (object, key).type != expectedType)
            return false;
    }

    return true;
}

static bool isExistingEntry (const jam::JsonDocument& doc,
                             const jam::Document::Element& entryElement)
{
    return not doc.contains (entryElement, Id::existing) or doc.getBoolean (doc.get (entryElement, Id::existing));
}

static bool isValidFileEntry (const jam::JsonDocument& doc,
                              const jam::Document::Element& filesElement,
                              const jam::Document::Token& entryToken,
                              const juce::File& blobFile)
{
    if (entryToken.type != map::JsonTokenType::object)
        return false;

    const auto* entryElement { doc.getChildByToken (filesElement, entryToken) };

    if (doc.contains (*entryElement, Id::existing) and doc.get (*entryElement, Id::existing).type != map::JsonTokenType::boolean)
        return false;

    if (isExistingEntry (doc, *entryElement))
    {
        return hasTypedMembers (doc, *entryElement, {
                   { jam::Strings::getView (Id::path.toString()), map::JsonTokenType::string },
                   { Id::preHash, map::JsonTokenType::string },
                   { Id::postHash, map::JsonTokenType::string },
                   { Id::executable, map::JsonTokenType::boolean } })
               and blobFile.existsAsFile();
    }

    return hasTypedMembers (doc, *entryElement, {
               { jam::Strings::getView (Id::path.toString()), map::JsonTokenType::string },
               { Id::postHash, map::JsonTokenType::string },
               { Id::executable, map::JsonTokenType::boolean } })
           and not blobFile.existsAsFile();
}

static bool isValidFilesArray (const jam::JsonDocument& doc,
                               const jam::Document::Element& rootElement,
                               std::string_view transactionId)
{
    const auto& filesToken { doc.get (rootElement, Id::files) };
    const auto* filesElement { doc.getChildByToken (rootElement, filesToken) };

    if (filesElement == nullptr)
        return false;

    const auto& entryTokens { *filesElement->get<jam::Document::Tokens> (Id::tokens) };

    for (int index { 0 }; index < entryTokens.size(); ++index)
        if (not isValidFileEntry (doc, *filesElement, entryTokens.at (index), UndoCache::getBlobFile (transactionId, index)))
            return false;

    return true;
}

static bool isValidFoldersArray (const jam::JsonDocument& doc,
                                 const jam::Document::Element& rootElement)
{
    if (not doc.contains (rootElement, Id::folders))
        return true;

    const auto& foldersToken { doc.get (rootElement, Id::folders) };

    if (foldersToken.type != map::JsonTokenType::array)
        return false;

    const auto* foldersElement { doc.getChildByToken (rootElement, foldersToken) };
    const auto& folderTokens { *foldersElement->get<jam::Document::Tokens> (Id::tokens) };

    for (int index { 0 }; index < folderTokens.size(); ++index)
        if (folderTokens.at (index).type != map::JsonTokenType::string)
            return false;

    return true;
}

static bool isValidTransactionDocument (const jam::JsonDocument& doc,
                                        std::string_view transactionId)
{
    const auto& rootToken { doc.getValue() };

    if (rootToken.type != map::JsonTokenType::object)
        return false;

    const auto* rootElement { doc.getChildByToken (*doc.getRoot(), rootToken) };

    if (rootElement == nullptr
        or not hasTypedMembers (doc, *rootElement, {
            { Id::timestamp, map::JsonTokenType::string },
            { Id::transactionCommand, map::JsonTokenType::string },
            { Id::files, map::JsonTokenType::array } }))
    {
        return false;
    }

    return isValidFoldersArray (doc, *rootElement) and isValidFilesArray (doc, *rootElement, transactionId);
}

static std::pair<juce::Result, std::unique_ptr<jam::JsonDocument>> getCorruptResult (std::string_view transactionId)
{
    const juce::String recordName { getRecordName (transactionId) };
    return { Stdio::fail (text::Diagnostics::failUndoCorrupt,
                          transactionId,
                          jam::Strings::getView (Id::diagnosticSeparator),
                          jam::Strings::getView (recordName)),
             nullptr };
}

static juce::File getApplicationDirectory()
{
    return jam::File::getUserApplicationDataDirectory().getChildFile (ProjectInfo::projectName);
}

static juce::File getDirectory()
{
    return getApplicationDirectory().getChildFile (Id::undoFolder.data());
}

static juce::File getPendingDirectory()
{
    return getApplicationDirectory().getChildFile (Id::pendingFolder.data());
}

static juce::File getPendingFile (std::string_view transactionId)
{
    return getPendingDirectory().getChildFile (getRecordName (transactionId));
}

juce::File UndoCache::getBlobDirectory()
{
    return getApplicationDirectory().getChildFile (Id::blobFolder.data());
}

static juce::File getRecordFile (std::string_view transactionId)
{
    const auto pendingFile { getPendingFile (transactionId) };

    if (pendingFile.getSize() > 0)
        return pendingFile;

    return getDirectory().getChildFile (getRecordName (transactionId));
}

juce::File UndoCache::getBlobFile (std::string_view transactionId, int index)
{
    juce::String blobName;
    blobName << jam::Strings::fromView (transactionId) << juce::String::charToString (Chars::dash) << index << jam::Strings::fromView (Id::blobSuffix);
    return getBlobDirectory().getChildFile (blobName);
}

juce::Result UndoCache::publishTransaction (std::string_view transactionId)
{
    if (jam::File::moveFileTo (getPendingFile (transactionId), getDirectory().getChildFile (getRecordName (transactionId))))
        return juce::Result::ok();

    return Stdio::fail (text::Diagnostics::failUndoPending, transactionId, text::Diagnostics::undoPending);
}

std::pair<juce::Result, bool> UndoCache::claimTransaction (std::string_view transactionId, bool isForce)
{
    if (not jam::File::createDirectory (getPendingDirectory()))
        return { Writer::getWriteFailure (jam::Strings::getView (getPendingDirectory().getFullPathName()), text::Diagnostics::failOverwriteFailed), false };

    const auto pendingFile { getPendingFile (transactionId) };
    const auto isCreated { jam::File::createNewFile (pendingFile) };

    if (not isCreated.has_value())
        return { Writer::getWriteFailure (jam::Strings::getView (pendingFile.getFullPathName()), text::Diagnostics::failOverwriteFailed), false };

    if (*isCreated)
    {
        if (jam::File::moveFileTo (getDirectory().getChildFile (getRecordName (transactionId)), pendingFile))
            return { juce::Result::ok(), true };

        juce::ignoreUnused (jam::File::deleteFile (pendingFile));
        return { Stdio::fail (text::Diagnostics::failUndoUnknown, transactionId), false };
    }

    if (isForce)
        return { juce::Result::ok(), false };

    return { Stdio::fail (text::Diagnostics::failUndoPending, transactionId, text::Diagnostics::undoPending), false };
}

static std::pair<juce::Result, std::string> addPendingFile (std::string_view compactPrefix,
                                                            int64_t firstSuffix)
{
    static constexpr size_t suffixHexLength { 6 };

    for (auto suffix { firstSuffix }; ; ++suffix)
    {
        std::string suffixHex;
        jam::Format::appendHex (suffixHex, static_cast<std::uint64_t> (suffix));
        std::string txId { compactPrefix };

        for (auto padding { suffixHex.size() }; padding < suffixHexLength; ++padding)
            jam::Format::appendUtf8 (txId, Chars::zero);

        jam::Format::appendBytes (txId, suffixHex);
        const auto pendingFile { getPendingFile (txId) };
        const auto isCreated { jam::File::createNewFile (pendingFile) };

        if (not isCreated.has_value())
            return { Writer::getWriteFailure (jam::Strings::getView (pendingFile.getFullPathName()), text::Diagnostics::failOverwriteFailed), {} };

        if (*isCreated)
        {
            if (not getDirectory().getChildFile (getRecordName (txId)).exists())
                return { juce::Result::ok(), txId };

            juce::ignoreUnused (jam::File::deleteFile (pendingFile));
        }
    }
}

std::pair<juce::Result, std::string> UndoCache::addTransaction (std::string_view commandName,
                                                                const jam::Array<StagedFile>& stagedFiles,
                                                                const jam::Array<juce::File>& folders)
{
    static constexpr int64_t microsecondsPerSecond { 1000000 };
    static constexpr std::string_view dateSeparator { "-" };
    static constexpr std::string_view timeSeparator { ":" };

    if (not jam::File::createDirectory (getDirectory())
        or not jam::File::createDirectory (getPendingDirectory())
        or not jam::File::createDirectory (getBlobDirectory()))
        return { Writer::getWriteFailure (jam::Strings::getView (getApplicationDirectory().getFullPathName()), text::Diagnostics::failOverwriteFailed), {} };

    const auto now { std::chrono::system_clock::now() };
    const int64_t microseconds { std::chrono::duration_cast<std::chrono::microseconds> (now.time_since_epoch()).count() };
    const auto seconds { microseconds / microsecondsPerSecond };
    const std::time_t time { static_cast<std::time_t> (seconds) };
    const std::tm utc { *std::gmtime (&time) };
    std::string compactPrefix { getUtcText (utc, {}, {}) };
    jam::Format::appendUtf8 (compactPrefix, Chars::dash);
    std::string timestamp { getUtcText (utc, dateSeparator, timeSeparator) };
    jam::Format::appendUtf8 (timestamp, Chars::upperZ);
    const auto [addStatus, addedId] { addPendingFile (compactPrefix, microseconds % microsecondsPerSecond) };

    if (addStatus.failed())
        return { addStatus, {} };

    const int fileCount { stagedFiles.size() };
    jam::Array<juce::Result> statuses;
    statuses.insertMultiple (0, juce::Result::ok(), fileCount + 1);

    Jobs::run (fileCount + 1, [&statuses, &transactionId = addedId, &timestamp, &commandName, &folders, &stagedFiles, fileCount] (int index)
    {
        if (index == fileCount)
        {
            const auto pendingFile { getPendingFile (transactionId) };
            const auto hashes (getContentHashes (stagedFiles));
            auto recordStatus { Writer::toNewFile (pendingFile, [&stagedFiles, &hashes, &folders, &timestamp, &commandName] (juce::OutputStream& json)
            {
                jam::JsonWriter::appendObject (json, [&stagedFiles, &hashes, &folders, &timestamp, &commandName] (juce::OutputStream& members)
                {
                    appendStringMember (members, Id::timestamp, 0, timestamp);
                    appendStringMember (members, Id::transactionCommand, 1, commandName);
                    jam::JsonWriter::appendKey (members, Id::folders, 2);
                    jam::JsonWriter::appendArray (members, [&folders] (juce::OutputStream& entries)
                    {
                        appendFolderEntries (entries, folders);
                    });
                    jam::JsonWriter::appendKey (members, Id::files, 3);
                    jam::JsonWriter::appendArray (members, [&stagedFiles, &hashes] (juce::OutputStream& entries)
                    {
                        appendFileEntries (entries, stagedFiles, hashes);
                    });
                });

                return true;
            }) };

            statuses.at (index) = recordStatus.wasOk() ? recordStatus : Writer::getWriteFailure (jam::Strings::getView (pendingFile.getFullPathName()), jam::Strings::getView (recordStatus.getErrorMessage()));
        }
        else
        {
            const auto& stagedFile { stagedFiles.at (index) };
            auto blobStatus { writeBlob (getBlobFile (transactionId, index), stagedFile) };

            statuses.at (index) = blobStatus.wasOk() ? blobStatus : Writer::getWriteFailure (stagedFile.getDisplayPath(), jam::Strings::getView (blobStatus.getErrorMessage()));
        }
    });

    const auto firstFailure { Stdio::getFirstFailure (statuses) };

    if (firstFailure.failed())
    {
        for (int blobIndex { 0 }; blobIndex < fileCount; ++blobIndex)
            juce::ignoreUnused (jam::File::deleteFile (getBlobFile (addedId, blobIndex)));

        removeTransaction (addedId);
        return { firstFailure, {} };
    }

    return { juce::Result::ok(), addedId };
}

void UndoCache::applyToTransactions (const std::function<void (std::string_view, const juce::Result&, const jam::JsonDocument*)>& function)
{
    const auto publishedIds (getSortedIds (getDirectory()));
    auto ids (getSortedIds (getPendingDirectory()));
    ids.addArray (publishedIds, 0, -1);
    ids.sort (false);
    ids.removeDuplicates (false);

    for (int index { ids.size() - 1 }; index >= 0; --index)
    {
        const auto transactionId { ids.getView (index) };
        const auto [status, document] { getTransactionDocument (transactionId) };

        if (status.wasOk() or getDirectory().getChildFile (getRecordName (transactionId)).existsAsFile())
            function (transactionId, status, document.get());
    }
}

std::pair<juce::Result, std::unique_ptr<jam::JsonDocument>> UndoCache::getTransactionDocument (std::string_view transactionId)
{
    const auto recordFile { getRecordFile (transactionId) };

    if (not recordFile.existsAsFile())
        return { Stdio::fail (text::Diagnostics::failUndoUnknown, transactionId), nullptr };

    auto doc { jam::JsonDocument::parse (recordFile) };

    if (doc == nullptr or not isValidTransactionDocument (*doc, transactionId))
        return getCorruptResult (transactionId);

    return { juce::Result::ok(), std::move (doc) };
}

static void removeBlobs (std::string_view transactionId)
{
    jam::Strings blobNames;

    Paths::applyToFileNames (UndoCache::getBlobDirectory(), [&blobNames, &transactionId] (std::string_view name)
    {
        if (name.size() > transactionId.size() and name.substr (0, transactionId.size()).compare (transactionId) == 0)
            blobNames.add (name);
    });

    for (int index { 0 }; index < blobNames.size(); ++index)
        juce::ignoreUnused (jam::File::deleteFile (UndoCache::getBlobDirectory().getChildFile (jam::Strings::fromView (blobNames.getView (index)))));
}

void UndoCache::removeTransaction (std::string_view transactionId)
{
    juce::ignoreUnused (jam::File::deleteFile (getPendingFile (transactionId)));
    juce::ignoreUnused (jam::File::deleteFile (getDirectory().getChildFile (getRecordName (transactionId))));
    removeBlobs (transactionId);
}

void UndoCache::removeOldestTransactions()
{
    static constexpr int maxTransactions { 32 };
    const auto directory { getDirectory() };
    const auto ids (getSortedIds (directory));
    const auto excessCount { ids.size() - maxTransactions };

    for (int index { 0 }; index < excessCount; ++index)
    {
        const auto transactionId { ids.getView (index) };

        if (jam::File::deleteFile (directory.getChildFile (getRecordName (transactionId))))
            removeBlobs (transactionId);
    }
}
