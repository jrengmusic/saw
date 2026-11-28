#include "Patch.h"
#include "Arguments.h"
#include "Input.h"
#include "LineDiff.h"
#include "Paths.h"
#include "PatchValidator.h"
#include "Stdio.h"
#include "TextDocument.h"
#include "Transaction.h"
#include "generated/Generated.h"

static std::pair<juce::Result, std::unique_ptr<jam::JsonDocument>> getValidatedJsonDocument (std::string_view jsonBytes)
{
    auto jsonDoc { jam::JsonDocument::read (jsonBytes) };

    if (jsonDoc == nullptr)
    {
        const auto syntaxStatus { jam::JsonDocument::isValid (jsonBytes) };
        return { Stdio::fail (text::Diagnostics::failPatchJsonInvalid, syntaxStatus), nullptr };
    }

    const auto validationStatus { PatchValidator {}.isValid (*jsonDoc) };

    if (validationStatus.failed())
        return { validationStatus, nullptr };

    return { juce::Result::ok(), std::move (jsonDoc) };
}

static std::tuple<jam::Strings, jam::Array<juce::File>, jam::Array<jam::Array<int>>> getGroupedEntries (const jam::JsonDocument& jsonDoc,
                                                                                                        const jam::Document::Element& rootArray,
                                                                                                        const jam::Document::Tokens& entryTokens)
{
    jam::Strings groupDisplayPaths;
    jam::Array<juce::File> groupFiles;
    jam::Array<jam::Array<int>> groupEntryIndices;
    jam::HashMap<std::string_view, int> groupIndexByCanonicalPath;
    jam::HashMap<std::string_view, int> groupIndexByPath;

    for (int entryIndex { 0 }; entryIndex < entryTokens.size(); ++entryIndex)
    {
        const auto* entry { jsonDoc.getChildByToken (rootArray, entryTokens.at (entryIndex)) };
        const auto filePath { jsonDoc.getString (jsonDoc.get (*entry, jam::Strings::getView (Id::file.toString()))) };
        const auto [pathEntry, isPathInserted] { groupIndexByPath.try_emplace (filePath, 0) };
        auto& [pathKey, pathGroupIndex] { *pathEntry };
        juce::ignoreUnused (pathKey);

        if (isPathInserted)
        {
            auto targetFile { Paths::getLinkedTarget (filePath) };
            const auto [groupEntry, isInserted] { groupIndexByCanonicalPath.try_emplace (jam::Strings::getView (targetFile.getFullPathName()), groupFiles.size()) };

            if (isInserted)
            {
                groupDisplayPaths.add (Paths::getDisplayPath (filePath));
                groupFiles.add (std::move (targetFile));
                groupEntryIndices.add (jam::Array<int> {});
            }

            auto& [groupCanonicalPath, canonicalGroupIndex] { *groupEntry };
            pathGroupIndex = canonicalGroupIndex;
        }

        groupEntryIndices.at (pathGroupIndex).add (entryIndex);
    }

    return { std::move (groupDisplayPaths), std::move (groupFiles), std::move (groupEntryIndices) };
}

static std::tuple<int, int, std::optional<std::string_view>> getDeleteChange (const juce::Identifier&,
                                                                              const jam::JsonDocument& jsonDoc,
                                                                              const jam::Document::Element& entry,
                                                                              std::string_view)
{
    return LineDiff::getDeletion (PatchValidator::getEntryLineRange (jsonDoc, entry));
}

static std::tuple<int, int, std::optional<std::string_view>> getReplaceChange (const juce::Identifier&,
                                                                               const jam::JsonDocument& jsonDoc,
                                                                               const jam::Document::Element& entry,
                                                                               std::string_view text)
{
    return LineDiff::getReplacement (PatchValidator::getEntryLineRange (jsonDoc, entry), LineDiff::getTextWithoutSingleTrailingNewline (text));
}

static std::tuple<int, int, std::optional<std::string_view>> getInsertChange (const juce::Identifier& key,
                                                                              const jam::JsonDocument& jsonDoc,
                                                                              const jam::Document::Element& entry,
                                                                              std::string_view text)
{
    static const juce::Identifier insertBeforeKey { jam::Strings::fromView (Id::insertBefore) };
    const int line { PatchValidator::getIntMember (jsonDoc, entry, jam::Strings::getView (key.toString())) };
    const int changeStart { key == insertBeforeKey ? line - 1 : line };
    return LineDiff::getInsertion (changeStart, LineDiff::getTextWithoutSingleTrailingNewline (text));
}

static const jam::Function::Map<juce::Identifier,
                                jam::Function::pointerCapacity,
                                std::tuple<int, int, std::optional<std::string_view>> (const jam::JsonDocument&, const jam::Document::Element&, std::string_view)>&
getEntryChanges()
{
    static const auto entryChanges (
        []
        {
            jam::Function::Map<juce::Identifier,
                               jam::Function::pointerCapacity,
                               std::tuple<int, int, std::optional<std::string_view>> (const jam::JsonDocument&, const jam::Document::Element&, std::string_view)> entryChanges;
            entryChanges.add (juce::Identifier (jam::Strings::fromView (Id::deletion)), &getDeleteChange);
            entryChanges.add (juce::Identifier (jam::Strings::fromView (Id::startLine)), &getReplaceChange);
            entryChanges.add (juce::Identifier (jam::Strings::fromView (Id::insertBefore)), &getInsertChange);
            entryChanges.add (juce::Identifier (jam::Strings::fromView (Id::insertAfter)), &getInsertChange);
            return entryChanges;
        }());

    return entryChanges;
}

static std::string_view getEntryText (const jam::JsonDocument& jsonDoc, const jam::Document::Element& entry)
{
    return jsonDoc.contains (entry, jam::Strings::getView (Id::text.toString()))
               ? jsonDoc.getString (jsonDoc.get (entry, jam::Strings::getView (Id::text.toString())))
               : std::string_view {};
}

static const jam::Document::Element& getEntryAt (const jam::JsonDocument& jsonDoc,
                                                 const jam::Document::Element& rootArray,
                                                 const jam::Document::Tokens& entryTokens,
                                                 const jam::Array<int>& entryIndices,
                                                 int position)
{
    return *jsonDoc.getChildByToken (rootArray, entryTokens.at (entryIndices.at (position)));
}

static int getColumnLineNumber (const jam::JsonDocument& jsonDoc, const jam::Document::Element& entry)
{
    return PatchValidator::getIntMember (jsonDoc, entry, jam::Strings::getView (Id::line.toString()));
}

static const juce::Identifier& getStartColKey()
{
    static const juce::Identifier startColKey { jam::Strings::fromView (Id::startCol) };
    return startColKey;
}

static bool isColumnOfLine (const jam::JsonDocument& jsonDoc, const jam::Document::Element& entry, int lineNumber)
{
    return PatchValidator::getEntryShapeKey (jsonDoc, entry) == getStartColKey()
           and getColumnLineNumber (jsonDoc, entry) == lineNumber;
}

static int getColumnGroupEnd (const jam::JsonDocument& jsonDoc,
                              const jam::Document::Element& rootArray,
                              const jam::Document::Tokens& entryTokens,
                              const jam::Array<int>& entryIndices,
                              int groupStart)
{
    const int lineNumber { getColumnLineNumber (jsonDoc, getEntryAt (jsonDoc, rootArray, entryTokens, entryIndices, groupStart)) };
    int groupEnd { groupStart + 1 };

    while (groupEnd < entryIndices.size() and isColumnOfLine (jsonDoc, getEntryAt (jsonDoc, rootArray, entryTokens, entryIndices, groupEnd), lineNumber))
        ++groupEnd;

    return groupEnd;
}

static void setColumnLineText (std::string& lineText,
                               const jam::JsonDocument& jsonDoc,
                               const jam::Document::Element& rootArray,
                               const jam::Document::Tokens& entryTokens,
                               const jam::Array<int>& entryIndices,
                               juce::Range<int> groupRange,
                               std::string_view line)
{
    lineText.clear();
    size_t cursor { 0 };

    for (int position { groupRange.getStart() }; position < groupRange.getEnd(); ++position)
    {
        const auto& entry { getEntryAt (jsonDoc, rootArray, entryTokens, entryIndices, position) };
        const auto startCol { static_cast<size_t> (PatchValidator::getIntMember (jsonDoc, entry, Id::startCol)) };
        const auto endCol { static_cast<size_t> (PatchValidator::getIntMember (jsonDoc, entry, Id::endCol)) };
        const auto replacement { jsonDoc.getString (jsonDoc.get (entry, jam::Strings::getView (Id::text.toString()))) };
        jam::Format::appendBytes (lineText, line.substr (cursor, startCol - cursor));
        jam::Format::appendBytes (lineText, replacement);
        cursor = endCol;
    }

    jam::Format::appendBytes (lineText, line.substr (cursor));
}

static std::tuple<int, int, int> getEntryOrder (const jam::JsonDocument& jsonDoc, const jam::Document::Element& entry)
{
    if (PatchValidator::getEntryShapeKey (jsonDoc, entry) == getStartColKey())
        return { getColumnLineNumber (jsonDoc, entry) - 1, 1, PatchValidator::getIntMember (jsonDoc, entry, Id::startCol) };

    const auto [start, length, insertedText] { getEntryChanges().get (PatchValidator::getEntryShapeKey (jsonDoc, entry), jsonDoc, entry, std::string_view {}) };
    juce::ignoreUnused (insertedText);
    return { start, length > 0 ? 1 : 0, 0 };
}

static juce::Result addTargetFile (Transaction& transaction,
                                   const jam::JsonDocument& jsonDoc,
                                   const jam::Document::Element& rootArray,
                                   const jam::Document::Tokens& entryTokens,
                                   const juce::File& targetFile,
                                   std::string_view displayPath,
                                   const jam::Array<int>& entryIndices)
{
    auto [readStatus, targetDoc] { TextDocument::fromTextFile (targetFile, displayPath, Input::allBytes) };

    if (readStatus.failed())
        return readStatus;

    const auto fileStatus { PatchValidator::isValidForFile (jsonDoc, entryIndices, *targetDoc) };

    if (fileStatus.failed())
        return fileStatus;

    jam::Array<std::pair<std::tuple<int, int, int>, int>> entryOrder;
    entryOrder.ensureStorageAllocated (entryIndices.size());

    for (int position { 0 }; position < entryIndices.size(); ++position)
        entryOrder.add ({ getEntryOrder (jsonDoc, getEntryAt (jsonDoc, rootArray, entryTokens, entryIndices, position)), position });

    std::sort (entryOrder.begin(), entryOrder.end());

    jam::Array<int> sortedIndices;
    sortedIndices.ensureStorageAllocated (entryIndices.size());

    for (const auto& [sortKey, position] : entryOrder)
        sortedIndices.add (entryIndices.at (position));

    const TextDocument& targetDocument { *targetDoc };
    LineDiff diff (targetDocument, [&jsonDoc, &rootArray, &entryTokens, &sortedIndices, &targetDocument] (const auto& addChange)
    {
        std::string lineText;
        int position { 0 };

        while (position < sortedIndices.size())
        {
            const auto& entry { getEntryAt (jsonDoc, rootArray, entryTokens, sortedIndices, position) };
            const auto key { PatchValidator::getEntryShapeKey (jsonDoc, entry) };

            if (key == getStartColKey())
            {
                const int groupEnd { getColumnGroupEnd (jsonDoc, rootArray, entryTokens, sortedIndices, position) };
                const int lineNumber { getColumnLineNumber (jsonDoc, entry) };
                setColumnLineText (lineText, jsonDoc, rootArray, entryTokens, sortedIndices, juce::Range<int> (position, groupEnd), targetDocument.getLine (lineNumber));
                addChange (lineNumber - 1, 1, std::string_view { lineText });
                position = groupEnd;
            }
            else
            {
                const auto text { getEntryText (jsonDoc, entry) };
                const auto [start, length, insertedText] { getEntryChanges().get (key, jsonDoc, entry, text) };
                addChange (start, length, insertedText);
                ++position;
            }
        }
    });

    return transaction.addFile (targetFile, std::move (targetDoc), std::move (diff), entryIndices.size());
}

static juce::Result addGroupedFiles (Transaction& transaction,
                                     const jam::JsonDocument& jsonDoc,
                                     const jam::Document::Element& rootArray,
                                     const jam::Document::Tokens& entryTokens,
                                     const jam::Strings& groupDisplayPaths,
                                     const jam::Array<juce::File>& groupFiles,
                                     const jam::Array<jam::Array<int>>& groupEntryIndices)
{
    jam::Array<int> groupOrder;

    for (int groupIndex { 0 }; groupIndex < groupDisplayPaths.size(); ++groupIndex)
        groupOrder.add (groupIndex);

    std::sort (groupOrder.begin(), groupOrder.end(),
        [&groupDisplayPaths = groupDisplayPaths] (int leftGroup, int rightGroup)
        {
            return groupDisplayPaths.getView (leftGroup) < groupDisplayPaths.getView (rightGroup);
        });

    for (int position { 0 }; position < groupOrder.size(); ++position)
    {
        const int groupIndex { groupOrder.at (position) };
        const auto fileStatus { addTargetFile (transaction, jsonDoc, rootArray, entryTokens, groupFiles.at (groupIndex), groupDisplayPaths.getView (groupIndex), groupEntryIndices.at (groupIndex)) };

        if (fileStatus.failed())
            return fileStatus;
    }

    return juce::Result::ok();
}

static Stdio::Output runTransaction (const Arguments& arguments, const jam::JsonDocument& jsonDoc)
{
    const auto* rootArray { jsonDoc.getChildByToken (*jsonDoc.getRoot(), jsonDoc.getValue()) };
    const auto& entryTokens { *rootArray->get<jam::Document::Tokens> (Id::tokens) };
    const auto [expectStatus, expectedCount] { Transaction::getExpectStatus (arguments) };

    if (expectStatus.failed())
        return Stdio::getFailure (expectStatus);

    if (arguments.contains (Id::expectOption) and expectedCount != entryTokens.size())
        return Transaction::getExpectMismatch ({}, expectedCount, entryTokens.size());

    const auto [groupDisplayPaths, groupFiles, groupEntryIndices] { getGroupedEntries (jsonDoc, *rootArray, entryTokens) };
    Transaction transaction { Id::patch };
    const auto addStatus { addGroupedFiles (transaction, jsonDoc, *rootArray, entryTokens, groupDisplayPaths, groupFiles, groupEntryIndices) };

    if (addStatus.failed())
        return Stdio::getFailure (addStatus);

    return transaction.run (arguments);
}

static Stdio::Output runJson (const Arguments& arguments, std::string_view jsonBytes)
{
    const auto [docStatus, jsonDoc] { getValidatedJsonDocument (jsonBytes) };

    if (docStatus.failed())
        return Stdio::getFailure (docStatus);

    return runTransaction (arguments, *jsonDoc);
}

static Stdio::Output runFile (const Arguments& arguments)
{
    const auto filePath { arguments.getValueView (Id::fileOption) };
    const auto targetFile { Paths::getLinkedTarget (filePath) };
    const auto [readStatus, document] { TextDocument::fromTextFile (targetFile, filePath, Input::maxBufferedReadSize) };

    if (readStatus.failed())
        return Stdio::getFailure (readStatus);

    return runJson (arguments, document->getBytes());
}

static Stdio::Output runStandardInput (const Arguments& arguments, const Input::Source& input)
{
    if (Stdio::isTerminalInput (input))
        return Stdio::getFailure (text::Diagnostics::failPatchRequiresInput);

    const auto [inputStatus, standardInput] { Input::create (input, Input::allLines) };

    if (inputStatus.failed())
        return Stdio::getFailure (inputStatus);

    return runJson (arguments, standardInput->getBytes());
}

Arguments Patch::getArguments (juce::Span<const std::string_view> argumentList)
{
    return Arguments { argumentList, { Id::applyOption }, { Id::fileOption, Id::expectOption }, {} };
}

Stdio::Output Patch::run (const Arguments& arguments, const Input::Source& input)
{
    if (arguments.getStatus().failed())
        return Stdio::getFailure (arguments.getStatus());

    if (arguments.getPositionals().size() > 0)
        return Stdio::getFailure (text::Diagnostics::failUnexpectedArgument, arguments.getPositionals().at (0));

    if (arguments.contains (Id::fileOption))
        return runFile (arguments);

    return runStandardInput (arguments, input);
}
