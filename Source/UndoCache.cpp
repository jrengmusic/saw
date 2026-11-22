#include "UndoCache.h"
#include "Writer.h"
#include "generated/Generated.h"
#include <ctime>

static jam::Strings getSortedDirectoryNames (const juce::File& undoDir)
{
    jam::Strings ids;

    if (undoDir.isDirectory())
    {
        for (const auto& dirEntry : juce::RangedDirectoryIterator (undoDir, false,
                                                                    juce::String::charToString (Chars::asterisk),
                                                                    juce::File::findDirectories,
                                                                    juce::File::FollowSymlinks::no))
            ids.add (dirEntry.getFile().getFileName());

        ids.sort (false);
    }

    return ids;
}

static int getNextHexSuffix (const juce::File& undoDir, const juce::String& prefix)
{
    const auto existingIds { getSortedDirectoryNames (undoDir) };
    std::optional<int> maxSuffix;

    for (int index { 0 }; index < existingIds.size(); ++index)
    {
        const auto name { existingIds.at (index) };

        if (name.startsWith (prefix))
        {
            const auto suffixValue { name.fromFirstOccurrenceOf (prefix, false, false).getHexValue32() };

            if (not maxSuffix.has_value() or suffixValue > *maxSuffix)
                maxSuffix = suffixValue;
        }
    }

    if (maxSuffix.has_value())
        return *maxSuffix + 1;

    return juce::Random::getSystemRandom().nextInt (UndoCache::initialHexBound);
}

static juce::Result writeBlobs (const juce::File& txDir,
                                const jam::Array<std::string_view>& preEditBlobs)
{
    for (int index { 0 }; index < preEditBlobs.size(); ++index)
    {
        const auto blobName { juce::String (index) + Id::blobSuffix };
        const auto writeResult { Writer::toFile (txDir.getChildFile (blobName),
                                                 preEditBlobs.at (index),
                                                 false) };

        if (writeResult.failed())
            return writeResult;
    }

    return juce::Result::ok();
}

static void appendStringMember (std::string& json,
                                const juce::Identifier& key,
                                std::string_view value)
{
    jam::JsonWriter::appendKey (json, key);
    jam::JsonWriter::appendString (json, value, juce::JSON::Encoding::utf8);
}

static void appendFileEntries (std::string& entries,
                               const jam::Strings& canonicalPaths,
                               const jam::Strings& preHashes,
                               const jam::Strings& postHashes,
                               const jam::Array<bool>& executableFlags)
{
    for (int index { 0 }; index < canonicalPaths.size(); ++index)
    {
        const auto blobName { juce::String (index) + Id::blobSuffix };
        jam::JsonWriter::appendSeparator (entries);
        jam::JsonWriter::appendObject (entries,
            [&canonicalPaths, &blobName, &preHashes, &postHashes, &executableFlags, index] (std::string& entry)
            {
                appendStringMember (entry, Id::path, canonicalPaths.getView (index));
                appendStringMember (entry, Id::blob, jam::Strings::getView (blobName));
                appendStringMember (entry, Id::preHash, preHashes.getView (index));
                appendStringMember (entry, Id::postHash, postHashes.getView (index));
                jam::JsonWriter::appendKey (entry, Id::executable);
                jam::JsonWriter::appendBoolean (entry, executableFlags.at (index));
            });
    }
}

static std::string getMetaJson (const juce::String& txId,
                                const juce::String& timestamp,
                                const juce::String& commandName,
                                const jam::Strings& canonicalPaths,
                                const jam::Strings& preHashes,
                                const jam::Strings& postHashes,
                                const jam::Array<bool>& executableFlags)
{
    std::string json;

    jam::JsonWriter::appendObject (json,
        [&txId, &timestamp, &commandName, &canonicalPaths, &preHashes, &postHashes, &executableFlags] (std::string& members)
        {
            appendStringMember (members, Id::id, jam::Strings::getView (txId));
            appendStringMember (members, Id::timestamp, jam::Strings::getView (timestamp));
            appendStringMember (members, Id::command, jam::Strings::getView (commandName));
            jam::JsonWriter::appendKey (members, Id::files);
            jam::JsonWriter::appendArray (members,
                [&canonicalPaths, &preHashes, &postHashes, &executableFlags] (std::string& entries)
                {
                    appendFileEntries (entries, canonicalPaths, preHashes, postHashes, executableFlags);
                });
        });

    return json;
}

static bool hasTypedMembers (const jam::JsonDocument& doc,
                             const jam::Document::Element& object,
                             std::initializer_list<std::pair<const juce::Identifier*, int>> requiredMembers)
{
    for (const auto& [key, expectedType] : requiredMembers)
    {
        const auto keyView { jam::Strings::getView (key->toString()) };

        if (not doc.contains (object, keyView) or doc.get (object, keyView).type != expectedType)
            return false;
    }

    return true;
}

static bool isValidFileEntry (const jam::JsonDocument& doc,
                              const jam::Document::Element& filesElement,
                              const jam::Document::Token& entryToken,
                              const juce::File& txDir)
{
    if (entryToken.type != map::JsonTokenType::object)
        return false;

    const auto* entryElement { doc.getChildByToken (filesElement, entryToken) };

    if (entryElement == nullptr
        or not hasTypedMembers (doc, *entryElement, {
            { &Id::path, map::JsonTokenType::string },
            { &Id::blob, map::JsonTokenType::string },
            { &Id::preHash, map::JsonTokenType::string },
            { &Id::postHash, map::JsonTokenType::string },
            { &Id::executable, map::JsonTokenType::boolean } }))
    {
        return false;
    }

    const auto blobName { jam::Strings::fromView (
        doc.getString (doc.get (*entryElement, jam::Strings::getView (Id::blob.toString())))) };

    return blobName.isNotEmpty() and txDir.getChildFile (blobName).existsAsFile();
}

static bool isValidFilesArray (const jam::JsonDocument& doc,
                               const jam::Document::Element& rootElement,
                               const juce::File& txDir)
{
    const auto& filesToken { doc.get (rootElement, jam::Strings::getView (Id::files.toString())) };
    const auto* filesElement { doc.getChildByToken (rootElement, filesToken) };

    if (filesElement == nullptr)
        return false;

    const auto& entryTokens { *filesElement->get<jam::Document::Tokens> (Id::tokens) };

    for (int index { 0 }; index < entryTokens.size(); ++index)
        if (not isValidFileEntry (doc, *filesElement, entryTokens.at (index), txDir))
            return false;

    return true;
}

static bool isValidTransactionDocument (const jam::JsonDocument& doc,
                                        const juce::File& txDir)
{
    const auto& rootToken { doc.getValue() };

    if (rootToken.type != map::JsonTokenType::object)
        return false;

    const auto* rootElement { doc.getChildByToken (*doc.getRoot(), rootToken) };

    if (rootElement == nullptr
        or not hasTypedMembers (doc, *rootElement, {
            { &Id::id, map::JsonTokenType::string },
            { &Id::timestamp, map::JsonTokenType::string },
            { &Id::command, map::JsonTokenType::string },
            { &Id::files, map::JsonTokenType::array } }))
    {
        return false;
    }

    return isValidFilesArray (doc, *rootElement, txDir);
}

static std::pair<juce::Result, std::unique_ptr<jam::JsonDocument>> getCorruptResult (const juce::String& transactionId)
{
    return { juce::Result::fail (
                 juce::String::formatted (text::Diagnostics::failUndoCorrupt,
                                          transactionId.toRawUTF8(),
                                          Id::metaFile.toRawUTF8())),
             nullptr };
}

juce::File UndoCache::getDirectory()
{
    return jam::File::getUserApplicationDataDirectory (ProjectInfo::projectName).getChildFile (Id::undoFolder);
}

std::pair<juce::String, juce::String> UndoCache::getNextIdAndTimestamp()
{
    const std::time_t now { std::time (nullptr) };
    std::tm utcTime {};
#if JUCE_WINDOWS
    gmtime_s (&utcTime, &now);
#else
    gmtime_r (&now, &utcTime);
#endif
    std::array<char, timeBufferCapacity> compactBuffer {};
    std::strftime (compactBuffer.data(), compactBuffer.size(), Id::utcCompactFormat.toRawUTF8(), &utcTime);

    std::array<char, timeBufferCapacity> isoBuffer {};
    std::strftime (isoBuffer.data(), isoBuffer.size(), Id::utcIsoFormat.toRawUTF8(), &utcTime);

    const juce::String compactPrefix { juce::String::fromUTF8 (compactBuffer.data()) };
    const juce::String isoTimestamp { juce::String::fromUTF8 (isoBuffer.data()) };
    const auto prefix { compactPrefix + Chars::dash };
    const auto nextHex { getNextHexSuffix (getDirectory(), prefix) };
    const auto hexSuffix { juce::String::toHexString (nextHex).paddedLeft (Chars::zero, randomHexLength) };

    return { prefix + hexSuffix, isoTimestamp };
}

std::pair<juce::Result, juce::String> UndoCache::addTransaction (const juce::String& commandName,
                                                                 const jam::Strings& canonicalPaths,
                                                                 const jam::Strings& preHashes,
                                                                 const jam::Strings& postHashes,
                                                                 const jam::Array<bool>& executableFlags,
                                                                 const jam::Array<std::string_view>& preEditBlobs)
{
    jassert (canonicalPaths.size() == preHashes.size());
    jassert (canonicalPaths.size() == postHashes.size());
    jassert (canonicalPaths.size() == executableFlags.size());
    jassert (canonicalPaths.size() == preEditBlobs.size());

    const auto undoRoot { jam::File::getOrCreateDirectory (
        jam::File::getUserApplicationDataDirectory (ProjectInfo::projectName),
        Id::undoFolder) };
    const auto [txId, timestamp] { getNextIdAndTimestamp() };
    const auto txDir { jam::File::getOrCreateDirectory (undoRoot, txId) };
    const auto blobResult { writeBlobs (txDir, preEditBlobs) };

    if (blobResult.failed())
    {
        txDir.deleteRecursively (false);
        return { blobResult, {} };
    }

    const auto json { getMetaJson (txId, timestamp, commandName, canonicalPaths, preHashes, postHashes, executableFlags) };
    const auto metaResult { Writer::toFile (txDir.getChildFile (Id::metaFile), json, false) };

    if (metaResult.failed())
    {
        txDir.deleteRecursively (false);
        return { metaResult, {} };
    }

    removeOldestTransactions();
    return { juce::Result::ok(), txId };
}

jam::Strings UndoCache::getTransactionIds()
{
    const auto ids { getSortedDirectoryNames (getDirectory()) };
    jam::Strings newestFirst;
    newestFirst.ensureStorageAllocated (ids.size());

    for (int index { ids.size() - 1 }; index >= 0; --index)
        newestFirst.add (ids.getView (index));

    return newestFirst;
}

std::pair<juce::Result, std::unique_ptr<jam::JsonDocument>> UndoCache::getTransactionDocument (const juce::String& transactionId)
{
    const auto txDir { getDirectory().getChildFile (transactionId) };

    if (not txDir.isDirectory())
    {
        return { juce::Result::fail (
                     juce::String::formatted (text::Diagnostics::failUndoUnknown,
                                              transactionId.toRawUTF8())),
                 nullptr };
    }

    const auto metaFile { txDir.getChildFile (Id::metaFile) };

    if (not metaFile.existsAsFile())
        return getCorruptResult (transactionId);

    auto doc { jam::JsonDocument::parse (metaFile.loadFileAsString().toStdString()) };

    if (doc == nullptr or not isValidTransactionDocument (*doc, txDir))
        return getCorruptResult (transactionId);

    return { juce::Result::ok(), std::move (doc) };
}

void UndoCache::removeOldestTransactions()
{
    const auto undoDir { getDirectory() };
    const auto ids { getSortedDirectoryNames (undoDir) };
    const auto excessCount { ids.size() - maxTransactions };

    for (int index { 0 }; index < excessCount; ++index)
        undoDir.getChildFile (ids.at (index)).deleteRecursively (false);
}
