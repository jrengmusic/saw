#include "UndoCache.h"
#include "Jobs.h"
#include "Paths.h"
#include "Stdio.h"
#include "TextDocument.h"
#include "Writer.h"
#include "generated/Generated.h"
#include <chrono>

static constexpr int hashesPerFile { 2 };

static juce::String getRecordName (const juce::String& transactionId)
{
    return transactionId + jam::Strings::fromView (Id::recordSuffix);
}

static jam::Strings getSortedIds (const juce::File& directory)
{
    const auto names (Paths::getFileNames (directory));
    jam::Strings ids;

    for (int index { 0 }; index < names.size(); ++index)
    {
        const auto name { names.getView (index) };

        if (name.size() > Id::recordSuffix.size() and name.substr (name.size() - Id::recordSuffix.size()).compare (Id::recordSuffix) == 0)
            ids.add (name.substr (0, name.size() - Id::recordSuffix.size()));
    }

    ids.sort (false);
    return ids;
}

static juce::Result writeBlob (const juce::File& blobFile,
                               const juce::File& targetFile,
                               std::string_view originalBytes,
                               std::string_view plannedBytes)
{
    if (jam::File::isSwappable (targetFile, blobFile.getParentDirectory()))
        return Writer::toStagedFile (blobFile, targetFile, plannedBytes);

    return Writer::toNewFile (blobFile, originalBytes);
}

static std::vector<juce::String> getContentHashes (const jam::Array<std::unique_ptr<TextDocument>>& originalDocuments,
                                                   const std::vector<std::string>& plannedTexts)
{
    static constexpr size_t parallelHashSize { 256 * 1024 };
    const int fileCount { originalDocuments.size() };
    const int hashCount { fileCount * hashesPerFile };
    std::vector<juce::String> hashes (static_cast<size_t> (hashCount));
    size_t byteCount { 0 };

    for (int index { 0 }; index < fileCount; ++index)
        byteCount += originalDocuments.at (index)->getBytes().size() + plannedTexts.at (static_cast<size_t> (index)).size();

    const auto setHash = [&hashes, &originalDocuments, &plannedTexts] (int index)
    {
        const int fileIndex { index / hashesPerFile };
        const std::string_view contents { index % hashesPerFile == 0 ? originalDocuments.at (fileIndex)->getBytes()
                                                                     : std::string_view { plannedTexts.at (static_cast<size_t> (fileIndex)) } };
        hashes.at (static_cast<size_t> (index)) = TextDocument::getContentHash (contents);
    };

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

static void appendStringMember (std::string& json,
                                std::string_view key,
                                std::string_view value)
{
    jam::JsonWriter::appendKey (json, key);
    jam::JsonWriter::appendString (json, value, juce::JSON::Encoding::utf8);
}

static void appendFileEntries (std::string& entries,
                               const jam::Array<juce::File>& targetFiles,
                               const std::vector<juce::String>& hashes,
                               const jam::Array<bool>& executableFlags)
{
    for (int index { 0 }; index < targetFiles.size(); ++index)
    {
        const auto hashIndex { static_cast<size_t> (index * hashesPerFile) };
        jam::JsonWriter::appendSeparator (entries);
        jam::JsonWriter::appendObject (entries,
            [&targetFiles, &hashes, &executableFlags, index, hashIndex] (std::string& entry)
            {
                appendStringMember (entry, jam::Strings::getView (Id::path.toString()), jam::Strings::getView (targetFiles.at (index).getFullPathName()));
                appendStringMember (entry, Id::preHash, jam::Strings::getView (hashes.at (hashIndex)));
                appendStringMember (entry, Id::postHash, jam::Strings::getView (hashes.at (hashIndex + 1)));
                jam::JsonWriter::appendKey (entry, Id::executable);
                jam::JsonWriter::appendBoolean (entry, executableFlags.at (index));
            });
    }
}

static std::string getRecordJson (const juce::String& timestamp,
                                  const juce::String& commandName,
                                  const jam::Array<juce::File>& targetFiles,
                                  const std::vector<juce::String>& hashes,
                                  const jam::Array<bool>& executableFlags)
{
    std::string json;

    jam::JsonWriter::appendObject (json,
        [&timestamp, &commandName, &targetFiles, &hashes, &executableFlags] (std::string& members)
        {
            appendStringMember (members, Id::timestamp, jam::Strings::getView (timestamp));
            appendStringMember (members, Id::transactionCommand, jam::Strings::getView (commandName));
            jam::JsonWriter::appendKey (members, Id::files);
            jam::JsonWriter::appendArray (members,
                [&targetFiles, &hashes, &executableFlags] (std::string& entries)
                {
                    appendFileEntries (entries, targetFiles, hashes, executableFlags);
                });
        });

    return json;
}

static bool isLeapYear (int year) noexcept
{
    static constexpr int leapInterval { 4 };
    static constexpr int centuryInterval { 100 };
    static constexpr int eraInterval { 400 };
    return (year % leapInterval == 0 and year % centuryInterval != 0) or year % eraInterval == 0;
}

static std::tuple<int, int, int> getUtcDate (int64_t daysSinceEpoch)
{
    static constexpr int epochYear { 1970 };
    static constexpr int daysPerYear { 365 };
    auto day { daysSinceEpoch };
    int year { epochYear };
    int yearLength { daysPerYear };

    while (day >= yearLength)
    {
        day -= yearLength;
        ++year;
        yearLength = daysPerYear + (isLeapYear (year) ? 1 : 0);
    }

    const std::array<int, 12> monthLengths { 31, isLeapYear (year) ? 29 : 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    size_t month { 0 };

    while (day >= monthLengths.at (month))
    {
        day -= monthLengths.at (month);
        ++month;
    }

    return { year, static_cast<int> (month) + 1, static_cast<int> (day) + 1 };
}

static juce::String getUtcText (int64_t seconds, const char* format)
{
    static constexpr int secondsPerMinute { 60 };
    static constexpr int minutesPerHour { 60 };
    static constexpr int secondsPerHour { secondsPerMinute * minutesPerHour };
    static constexpr int secondsPerDay { 24 * secondsPerHour };
    const auto [year, month, day] { getUtcDate (seconds / secondsPerDay) };
    const int daySeconds { static_cast<int> (seconds % secondsPerDay) };
    return Stdio::getFormatted (format,
                                year,
                                month,
                                day,
                                daySeconds / secondsPerHour,
                                daySeconds / secondsPerMinute % minutesPerHour,
                                daySeconds % secondsPerMinute);
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

static bool isValidFileEntry (const jam::JsonDocument& doc,
                              const jam::Document::Element& filesElement,
                              const jam::Document::Token& entryToken,
                              const juce::File& blobFile)
{
    if (entryToken.type != map::JsonTokenType::object)
        return false;

    const auto* entryElement { doc.getChildByToken (filesElement, entryToken) };

    return entryElement != nullptr
           and hasTypedMembers (doc, *entryElement, {
                   { jam::Strings::getView (Id::path.toString()), map::JsonTokenType::string },
                   { Id::preHash, map::JsonTokenType::string },
                   { Id::postHash, map::JsonTokenType::string },
                   { Id::executable, map::JsonTokenType::boolean } })
           and blobFile.existsAsFile();
}

static bool isValidFilesArray (const jam::JsonDocument& doc,
                               const jam::Document::Element& rootElement,
                               const juce::String& transactionId)
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

static bool isValidTransactionDocument (const jam::JsonDocument& doc,
                                        const juce::String& transactionId)
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

    return isValidFilesArray (doc, *rootElement, transactionId);
}

static std::pair<juce::Result, std::unique_ptr<jam::JsonDocument>> getCorruptResult (const juce::String& transactionId)
{
    return { juce::Result::fail (
                 Stdio::getFormatted (text::Diagnostics::failUndoCorrupt,
                                      transactionId.toRawUTF8(),
                                      getRecordName (transactionId).toRawUTF8())),
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

juce::File UndoCache::getBlobDirectory()
{
    return getApplicationDirectory().getChildFile (Id::blobFolder.data());
}

juce::File UndoCache::getPendingFile (const juce::String& transactionId)
{
    return getPendingDirectory().getChildFile (getRecordName (transactionId));
}

static juce::File getRecordFile (const juce::String& transactionId)
{
    const auto pendingFile { UndoCache::getPendingFile (transactionId) };

    if (pendingFile.getSize() > 0)
        return pendingFile;

    return getDirectory().getChildFile (getRecordName (transactionId));
}

juce::File UndoCache::getBlobFile (const juce::String& transactionId, int index)
{
    return getBlobDirectory().getChildFile (transactionId + Chars::dash + juce::String (index) + jam::Strings::fromView (Id::blobSuffix));
}

juce::Result UndoCache::publishTransaction (const juce::String& transactionId)
{
    if (jam::File::moveFileTo (getPendingFile (transactionId), getDirectory().getChildFile (getRecordName (transactionId))))
        return juce::Result::ok();

    return juce::Result::fail (text::Diagnostics::failOverwriteFailed);
}

std::pair<juce::Result, bool> UndoCache::claimTransaction (const juce::String& transactionId, bool isForce)
{
    if (not jam::File::createDirectory (getPendingDirectory()))
        return { juce::Result::fail (text::Diagnostics::failOverwriteFailed), false };

    const auto pendingFile { getPendingFile (transactionId) };
    const auto isCreated { jam::File::createNewFile (pendingFile) };

    if (not isCreated.has_value())
        return { juce::Result::fail (text::Diagnostics::failOverwriteFailed), false };

    if (*isCreated)
    {
        if (jam::File::moveFileTo (getDirectory().getChildFile (getRecordName (transactionId)), pendingFile))
            return { juce::Result::ok(), true };

        juce::ignoreUnused (jam::File::deleteFile (pendingFile));
        return { juce::Result::fail (Stdio::getFormatted (text::Diagnostics::failUndoUnknown,
                                                          transactionId.toRawUTF8())),
                 false };
    }

    if (isForce)
        return { juce::Result::ok(), false };

    return { juce::Result::fail (Stdio::getFormatted (text::Diagnostics::failUndoPending,
                                                      transactionId.toRawUTF8())),
             false };
}

static std::pair<juce::Result, juce::String> addPendingFile (const juce::String& compactPrefix,
                                                             int64_t firstSuffix)
{
    static constexpr int suffixHexLength { 6 };

    for (auto suffix { firstSuffix }; ; ++suffix)
    {
        const auto txId { compactPrefix + juce::String::toHexString (suffix).paddedLeft (Chars::zero, suffixHexLength) };
        const auto pendingFile { UndoCache::getPendingFile (txId) };
        const auto isCreated { jam::File::createNewFile (pendingFile) };

        if (not isCreated.has_value())
            return { juce::Result::fail (text::Diagnostics::failOverwriteFailed), {} };

        if (*isCreated)
        {
            if (not getDirectory().getChildFile (getRecordName (txId)).exists())
                return { juce::Result::ok(), txId };

            juce::ignoreUnused (jam::File::deleteFile (pendingFile));
        }
    }
}

std::pair<juce::Result, juce::String> UndoCache::addTransaction (const juce::String& commandName,
                                                                 const jam::Array<juce::File>& targetFiles,
                                                                 const jam::Array<bool>& executableFlags,
                                                                 const jam::Array<std::unique_ptr<TextDocument>>& originalDocuments,
                                                                 const std::vector<std::string>& plannedTexts)
{
    static constexpr int64_t microsecondsPerSecond { 1000000 };
    jassert (targetFiles.size() == executableFlags.size());
    jassert (targetFiles.size() == originalDocuments.size());
    jassert (static_cast<size_t> (targetFiles.size()) == plannedTexts.size());

    if (not jam::File::createDirectory (getDirectory())
        or not jam::File::createDirectory (getPendingDirectory())
        or not jam::File::createDirectory (getBlobDirectory()))
        return { juce::Result::fail (text::Diagnostics::failOverwriteFailed), {} };

    const auto now { std::chrono::system_clock::now() };
    const int64_t microseconds { std::chrono::duration_cast<std::chrono::microseconds> (now.time_since_epoch()).count() };
    const auto seconds { microseconds / microsecondsPerSecond };
    const auto compactPrefix { getUtcText (seconds, text::Diagnostics::utcCompactFormat) + Chars::dash };
    const auto timestamp { getUtcText (seconds, text::Diagnostics::utcIsoFormat) };
    const auto [addStatus, addedId] { addPendingFile (compactPrefix, microseconds % microsecondsPerSecond) };

    if (addStatus.failed())
        return { addStatus, {} };

    const juce::String txId { addedId };
    const int fileCount { targetFiles.size() };
    jam::Array<juce::Result> statuses;
    statuses.insertMultiple (0, juce::Result::ok(), fileCount + 1);

    Jobs::run (fileCount + 1, [&statuses, &txId, &timestamp, &commandName, &targetFiles, &executableFlags, &originalDocuments, &plannedTexts, fileCount] (int index)
    {
        if (index == fileCount)
            statuses.at (index) = Writer::toNewFile (getPendingFile (txId),
                                                     getRecordJson (timestamp, commandName, targetFiles, getContentHashes (originalDocuments, plannedTexts), executableFlags));
        else
            statuses.at (index) = writeBlob (getBlobFile (txId, index),
                                             targetFiles.at (index),
                                             originalDocuments.at (index)->getBytes(),
                                             plannedTexts.at (static_cast<size_t> (index)));
    });

    for (int index { 0 }; index < statuses.size(); ++index)
    {
        if (statuses.at (index).failed())
        {
            for (int blobIndex { 0 }; blobIndex < fileCount; ++blobIndex)
                juce::ignoreUnused (jam::File::deleteFile (getBlobFile (txId, blobIndex)));

            removeTransaction (txId);
            return { statuses.at (index), {} };
        }
    }

    return { juce::Result::ok(), txId };
}

jam::Strings UndoCache::getTransactionIds()
{
    const auto pendingIds (getSortedIds (getPendingDirectory()));
    auto ids (getSortedIds (getDirectory()));

    for (int index { 0 }; index < pendingIds.size(); ++index)
    {
        const auto [pendingStatus, pendingDocument] { getTransactionDocument (pendingIds.at (index)) };
        juce::ignoreUnused (pendingDocument);

        if (pendingStatus.wasOk())
            ids.add (pendingIds.getView (index));
    }

    ids.sort (false);
    ids.removeDuplicates (false);

    jam::Strings newestFirst;
    newestFirst.ensureStorageAllocated (ids.size());

    for (int index { ids.size() - 1 }; index >= 0; --index)
        newestFirst.add (ids.getView (index));

    return newestFirst;
}

std::pair<juce::Result, std::unique_ptr<jam::JsonDocument>> UndoCache::getTransactionDocument (const juce::String& transactionId)
{
    const auto recordFile { getRecordFile (transactionId) };

    if (not recordFile.existsAsFile())
    {
        return { juce::Result::fail (
                     Stdio::getFormatted (text::Diagnostics::failUndoUnknown,
                                          transactionId.toRawUTF8())),
                 nullptr };
    }

    auto doc { jam::JsonDocument::parse (recordFile) };

    if (doc == nullptr or not isValidTransactionDocument (*doc, transactionId))
        return getCorruptResult (transactionId);

    return { juce::Result::ok(), std::move (doc) };
}

static void removeBlobs (const juce::String& transactionId)
{
    int index { 0 };

    while (jam::File::deleteFile (UndoCache::getBlobFile (transactionId, index)))
        ++index;
}

void UndoCache::removeTransaction (const juce::String& transactionId)
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
        const auto transactionId { ids.at (index) };

        if (jam::File::deleteFile (directory.getChildFile (getRecordName (transactionId))))
            removeBlobs (transactionId);
    }
}
