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
        return { juce::Result::fail (Stdio::getFormatted (text::Diagnostics::failPatchJsonInvalid,
                                                          syntaxStatus.getErrorMessage().toRawUTF8())),
                 nullptr };
    }

    const auto validationStatus { PatchValidator {}.isValid (*jsonDoc) };

    if (validationStatus.failed())
        return { validationStatus, nullptr };

    return { juce::Result::ok(), std::move (jsonDoc) };
}

static std::tuple<jam::Strings,
                  jam::HashMap<juce::String, juce::String>,
                  jam::HashMap<juce::String, jam::Array<int>>>
getGroupedEntries (const jam::JsonDocument& jsonDoc,
                   const jam::Document::Element& rootArray,
                   const jam::Document::Tokens& entryTokens)
{
    jam::Strings displayPaths;
    jam::HashMap<juce::String, juce::String> displayToCanonical;
    jam::HashMap<juce::String, jam::Array<int>> canonicalToEntries;

    for (int entryIndex { 0 }; entryIndex < entryTokens.size(); ++entryIndex)
    {
        const auto* entry { jsonDoc.getChildByToken (rootArray, entryTokens.at (entryIndex)) };
        const auto filePath { jam::Strings::fromView (jsonDoc.getString (jsonDoc.get (*entry, jam::Strings::getView (Id::file.toString())))) };
        const auto displayPath { jam::Strings::fromView (Paths::getDisplayPath (jam::Strings::getView (filePath))) };
        const auto canonicalPath { Paths::getLinkedTarget (jam::Strings::getView (filePath)).getFullPathName() };
        const auto [bucket, inserted] { canonicalToEntries.try_emplace (canonicalPath) };
        auto& [bucketKey, bucketIndices] { *bucket };
        juce::ignoreUnused (bucketKey);
        bucketIndices.add (entryIndex);

        if (inserted)
        {
            displayPaths.add (displayPath);
            displayToCanonical.try_emplace (displayPath, canonicalPath);
        }
    }

    displayPaths.sort (false);
    return { std::move (displayPaths), std::move (displayToCanonical), std::move (canonicalToEntries) };
}

static LineDiff::Change getDeleteChange (const juce::Identifier&,
                                         const jam::JsonDocument& jsonDoc,
                                         const jam::Document::Element& entry)
{
    const auto range { PatchValidator::getEntryLineRange (jsonDoc, entry) };
    return { range.getStart() - 1, range.getLength(), {} };
}

static LineDiff::Change getReplaceChange (const juce::Identifier&,
                                          const jam::JsonDocument& jsonDoc,
                                          const jam::Document::Element& entry)
{
    const auto range { PatchValidator::getEntryLineRange (jsonDoc, entry) };
    const auto text { jsonDoc.getString (jsonDoc.get (entry, jam::Strings::getView (Id::text.toString()))) };
    return { range.getStart() - 1, range.getLength(), LineDiff::getLines (text) };
}

static LineDiff::Change getInsertChange (const juce::Identifier& key,
                                         const jam::JsonDocument& jsonDoc,
                                         const jam::Document::Element& entry)
{
    const int line { PatchValidator::getIntMember (jsonDoc, entry, jam::Strings::getView (key.toString())) };
    const auto text { jsonDoc.getString (jsonDoc.get (entry, jam::Strings::getView (Id::text.toString()))) };
    const int changeStart { key == juce::Identifier (jam::Strings::fromView (Id::insertBefore)) ? line - 1 : line };
    return { changeStart, 0, LineDiff::getLines (text) };
}

static const jam::Function::Map<juce::Identifier,
                                jam::Function::pointerCapacity,
                                LineDiff::Change (const jam::JsonDocument&, const jam::Document::Element&)>&
getEntryChanges()
{
    static const auto entryChanges (
        []
        {
            jam::Function::Map<juce::Identifier,
                               jam::Function::pointerCapacity,
                               LineDiff::Change (const jam::JsonDocument&, const jam::Document::Element&)> entryChanges;
            entryChanges.add (juce::Identifier (jam::Strings::fromView (Id::deletion)), &getDeleteChange);
            entryChanges.add (juce::Identifier (jam::Strings::fromView (Id::startLine)), &getReplaceChange);
            entryChanges.add (juce::Identifier (jam::Strings::fromView (Id::insertBefore)), &getInsertChange);
            entryChanges.add (juce::Identifier (jam::Strings::fromView (Id::insertAfter)), &getInsertChange);
            return entryChanges;
        }());

    return entryChanges;
}

static LineDiff::Change getColumnLineChange (const jam::JsonDocument& jsonDoc,
                                             const jam::Document::Element& rootArray,
                                             const jam::Document::Tokens& entryTokens,
                                             const TextDocument& targetDoc,
                                             int lineNumber,
                                             jam::Array<int> lineEntryIndices)
{
    std::sort (lineEntryIndices.begin(), lineEntryIndices.end(),
        [&jsonDoc, &rootArray, &entryTokens] (int leftIndex, int rightIndex)
        {
            const auto* leftEntry { jsonDoc.getChildByToken (rootArray, entryTokens.at (leftIndex)) };
            const auto* rightEntry { jsonDoc.getChildByToken (rootArray, entryTokens.at (rightIndex)) };
            return PatchValidator::getIntMember (jsonDoc, *leftEntry, Id::startCol)
                 > PatchValidator::getIntMember (jsonDoc, *rightEntry, Id::startCol);
        });

    std::string lineText { targetDoc.getLine (lineNumber) };

    for (int index { 0 }; index < lineEntryIndices.size(); ++index)
    {
        const auto* entry { jsonDoc.getChildByToken (rootArray, entryTokens.at (lineEntryIndices.at (index))) };
        const auto startCol { static_cast<size_t> (PatchValidator::getIntMember (jsonDoc, *entry, Id::startCol)) };
        const auto endCol { static_cast<size_t> (PatchValidator::getIntMember (jsonDoc, *entry, Id::endCol)) };
        const auto replacement { jsonDoc.getString (jsonDoc.get (*entry, jam::Strings::getView (Id::text.toString()))) };
        lineText.replace (startCol, endCol - startCol, replacement);
    }

    return { lineNumber - 1, 1, LineDiff::getLinesPreservingEmpty (lineText) };
}

static bool isChangeEarlier (const LineDiff::Change& left, const LineDiff::Change& right) noexcept
{
    if (left.start != right.start)
        return left.start < right.start;

    return left.length == 0 and right.length > 0;
}

static jam::Array<LineDiff::Change> getFileChanges (const jam::JsonDocument& jsonDoc,
                                                    const jam::Document::Element& rootArray,
                                                    const jam::Document::Tokens& entryTokens,
                                                    const jam::Array<int>& entryIndices,
                                                    const TextDocument& targetDoc)
{
    jam::HashMap<int, jam::Array<int>> columnEditsByLine;
    jam::Array<LineDiff::Change> changes;

    for (int index { 0 }; index < entryIndices.size(); ++index)
    {
        const int entryIndex { entryIndices.at (index) };
        const auto* entry { jsonDoc.getChildByToken (rootArray, entryTokens.at (entryIndex)) };
        const auto key { PatchValidator::getEntryShapeKey (jsonDoc, *entry) };

        if (key == juce::Identifier (jam::Strings::fromView (Id::startCol)))
        {
            const auto [bucket, inserted] { columnEditsByLine.try_emplace (PatchValidator::getIntMember (jsonDoc, *entry, jam::Strings::getView (Id::line.toString()))) };
            auto& [bucketKey, bucketIndices] { *bucket };
            juce::ignoreUnused (bucketKey, inserted);
            bucketIndices.add (entryIndex);
        }
        else
            changes.add (getEntryChanges().get (key, jsonDoc, *entry));
    }

    for (auto& [lineNumber, lineEntryIndices] : columnEditsByLine)
        changes.add (getColumnLineChange (jsonDoc, rootArray, entryTokens, targetDoc, lineNumber, std::move (lineEntryIndices)));

    std::stable_sort (changes.begin(), changes.end(), &isChangeEarlier);
    return changes;
}

static juce::Result addTargetFile (Transaction& transaction,
                                   const jam::JsonDocument& jsonDoc,
                                   const jam::Document::Element& rootArray,
                                   const jam::Document::Tokens& entryTokens,
                                   const juce::File& targetFile,
                                   const juce::String& displayPath,
                                   const jam::Array<int>& entryIndices)
{
    auto [readStatus, targetDoc] { TextDocument::fromTextFile (targetFile, displayPath, Input::allBytes) };

    if (readStatus.failed())
        return readStatus;

    const auto fileStatus { PatchValidator::isValidForFile (jsonDoc, entryIndices, *targetDoc) };

    if (fileStatus.failed())
        return fileStatus;

    auto changes (getFileChanges (jsonDoc, rootArray, entryTokens, entryIndices, *targetDoc));
    return transaction.addFile (targetFile, std::move (targetDoc), LineDiff { std::move (changes) }, entryIndices.size());
}

static juce::Result addGroupedFiles (Transaction& transaction,
                                     const jam::JsonDocument& jsonDoc,
                                     const jam::Document::Element& rootArray,
                                     const jam::Document::Tokens& entryTokens)
{
    const auto [displayPaths, displayToCanonical, canonicalToEntries] { getGroupedEntries (jsonDoc, rootArray, entryTokens) };

    for (int index { 0 }; index < displayPaths.size(); ++index)
    {
        const auto displayPath { displayPaths.at (index) };
        const auto& canonicalPath { displayToCanonical.at (displayPath) };
        const auto fileStatus { addTargetFile (transaction,
                                               jsonDoc,
                                               rootArray,
                                               entryTokens,
                                               juce::File { canonicalPath },
                                               displayPath,
                                               canonicalToEntries.at (canonicalPath)) };

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
        return Stdio::getFailure (expectStatus.getErrorMessage());

    if (arguments.contains (Id::expectOption) and expectedCount != entryTokens.size())
        return Transaction::getExpectMismatch ({}, expectedCount, entryTokens.size());

    Transaction transaction { Id::patch };
    const auto addStatus { addGroupedFiles (transaction, jsonDoc, *rootArray, entryTokens) };

    if (addStatus.failed())
        return Stdio::getFailure (addStatus.getErrorMessage());

    return transaction.run (arguments, false);
}

static Stdio::Output runJson (const Arguments& arguments, std::string_view jsonBytes)
{
    const auto [docStatus, jsonDoc] { getValidatedJsonDocument (jsonBytes) };

    if (docStatus.failed())
        return Stdio::getFailure (docStatus.getErrorMessage());

    return runTransaction (arguments, *jsonDoc);
}

static Stdio::Output runFile (const Arguments& arguments)
{
    const auto filePath { arguments.getValue (Id::fileOption) };
    const auto targetFile { Paths::getLinkedTarget (jam::Strings::getView (filePath)) };
    const auto [readStatus, document] { TextDocument::fromTextFile (targetFile, filePath, Input::maxBufferedReadSize) };

    if (readStatus.failed())
        return Stdio::getFailure (readStatus.getErrorMessage());

    return runJson (arguments, document->getBytes());
}

static Stdio::Output runStandardInput (const Arguments& arguments, const std::optional<std::string>& input)
{
    if (Stdio::isTerminalInput (input))
        return Stdio::getFailure (text::Diagnostics::failPatchRequiresInput);

    const auto [inputStatus, standardInput] { Input::create (input, Input::allLines) };

    if (inputStatus.failed())
        return Stdio::getFailure (inputStatus.getErrorMessage());

    return runJson (arguments, standardInput->getBytes());
}

Arguments Patch::getArguments (const jam::Strings& argumentList)
{
    const std::vector<std::string_view> flagOptions { Id::applyOption };
    const std::vector<std::string_view> valueOptions { Id::fileOption, Id::expectOption };

    return Arguments { argumentList, flagOptions, valueOptions, {} };
}

Stdio::Output Patch::run (const Arguments& arguments, const std::optional<std::string>& input)
{
    if (arguments.getStatus().failed())
        return Stdio::getFailure (arguments.getStatus().getErrorMessage());

    if (arguments.getPositionals().size() > 0)
    {
        return Stdio::getFailure (Stdio::getFormatted (text::Diagnostics::failUnexpectedArgument,
                                                       arguments.getPositionals().at (0).toRawUTF8()));
    }

    if (arguments.contains (Id::fileOption))
        return runFile (arguments);

    return runStandardInput (arguments, input);
}
