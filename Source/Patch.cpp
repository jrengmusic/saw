#include "Patch.h"
#include "Arguments.h"
#include "LineDiff.h"
#include "PatchValidator.h"
#include "Stdio.h"
#include "TextDocument.h"
#include "Transaction.h"
#include "generated/Generated.h"

static std::pair<juce::Result, std::string> getFileJsonBytes (const juce::String& filePath)
{
    const auto resolved { juce::File::getCurrentWorkingDirectory().getChildFile (filePath).getLinkedTarget() };
    const auto [readStatus, document] { TextDocument::fromTextFile (resolved, filePath) };

    if (readStatus.failed())
        return { readStatus, {} };

    return { juce::Result::ok(), std::string { document->getBytes() } };
}

static std::pair<juce::Result, std::string> getPatchJsonInput (const Arguments& arguments)
{
    if (arguments.contains (Id::fileOption))
        return getFileJsonBytes (arguments.getValue (Id::fileOption));

    if (Stdio::isTerminalInput())
        return { juce::Result::fail (text::Diagnostics::failPatchRequiresInput), {} };

    return Stdio::getInput();
}

static std::pair<juce::Result, std::string> getValidatedInput (const Arguments& arguments)
{
    if (arguments.getStatus().failed())
        return { arguments.getStatus(), {} };

    if (arguments.getPositionals().size() > 0)
    {
        return { juce::Result::fail (juce::String::formatted (text::Diagnostics::failUnexpectedArgument,
                                                              arguments.getPositionals().at (0).toRawUTF8())),
                 {} };
    }

    return getPatchJsonInput (arguments);
}

static std::pair<juce::Result, std::unique_ptr<jam::JsonDocument>> getValidatedJsonDocument (const std::string& jsonBytes)
{
    auto jsonDoc { jam::JsonDocument::parse (std::string_view { jsonBytes }) };

    if (jsonDoc == nullptr)
    {
        const auto syntaxStatus { jam::JsonDocument::isValid (jsonBytes) };
        return { juce::Result::fail (juce::String::formatted (text::Diagnostics::failPatchJsonInvalid,
                                                              syntaxStatus.getErrorMessage().toRawUTF8())),
                 nullptr };
    }

    const auto validationStatus { PatchValidator {}.isValid (*jsonDoc) };

    if (validationStatus.failed())
        return { validationStatus, nullptr };

    return { juce::Result::ok(), std::move (jsonDoc) };
}

static std::pair<juce::Result, int> getExpectStatus (const Arguments& arguments)
{
    if (arguments.contains (Id::applyOption) and not arguments.contains (Id::expectOption))
        return { juce::Result::fail (text::Diagnostics::failApplyRequiresExpect), 0 };

    if (arguments.contains (Id::expectOption))
        return arguments.getInteger (Id::expectOption, 0);

    return { juce::Result::ok(), 0 };
}

static std::tuple<jam::Strings,
                  jam::HashMap<juce::String, juce::String>,
                  jam::HashMap<juce::String, jam::Array<int>>>
getGroupedEntries (const jam::JsonDocument& jsonDoc,
                   const jam::Document::Element& rootArray,
                   const jam::Document::Tokens& entryTokens)
{
    const auto workingDirectory { juce::File::getCurrentWorkingDirectory() };
    const auto fileKey { jam::Strings::getView (Id::file.toString()) };
    jam::Strings displayPaths;
    jam::HashMap<juce::String, juce::String> displayToCanonical;
    jam::HashMap<juce::String, jam::Array<int>> canonicalToEntries;

    for (int entryIndex { 0 }; entryIndex < entryTokens.size(); ++entryIndex)
    {
        const auto* entry { jsonDoc.getChildByToken (rootArray, entryTokens.at (entryIndex)) };
        const auto filePath { jam::Strings::fromView (jsonDoc.getString (jsonDoc.get (*entry, fileKey))) };
        const auto displayPath { filePath.replaceCharacter (Chars::backslash, Chars::slash) };
        const auto canonicalPath { workingDirectory.getChildFile (filePath).getLinkedTarget().getFullPathName() };
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

static int getIntField (const jam::JsonDocument& jsonDoc,
                        const jam::Document::Element& entry,
                        const juce::Identifier& key)
{
    const auto& token { jsonDoc.get (entry, jam::Strings::getView (key.toString())) };
    return jam::Format::getNumber<int> (jsonDoc.getView (token));
}

static juce::Identifier getLineOperationKey (const jam::JsonDocument& jsonDoc,
                                             const jam::Document::Element& entry)
{
    static const std::array<juce::Identifier, 4> operationKeys {
        Id::deletion,
        Id::startLine,
        Id::insertBefore,
        Id::insertAfter
    };

    for (const auto& key : operationKeys)
    {
        if (jsonDoc.contains (entry, jam::Strings::getView (key.toString())))
            return key;
    }

    jassertfalse;
    return {};
}

static LineDiff::Change getDeleteChange (const juce::Identifier&,
                                         const jam::JsonDocument& jsonDoc,
                                         const jam::Document::Element& entry)
{
    const auto hasSingleLine { jsonDoc.contains (entry, jam::Strings::getView (Id::line.toString())) };
    const int startLine { hasSingleLine ? getIntField (jsonDoc, entry, Id::line)
                                        : getIntField (jsonDoc, entry, Id::startLine) };
    const int endLine { hasSingleLine ? startLine
                                      : getIntField (jsonDoc, entry, Id::endLine) };
    return { startLine - 1, endLine - startLine + 1, {} };
}

static LineDiff::Change getReplaceChange (const juce::Identifier&,
                                          const jam::JsonDocument& jsonDoc,
                                          const jam::Document::Element& entry)
{
    const int startLine { getIntField (jsonDoc, entry, Id::startLine) };
    const int endLine { getIntField (jsonDoc, entry, Id::endLine) };
    const auto text { jsonDoc.getString (jsonDoc.get (entry, jam::Strings::getView (Id::text.toString()))) };
    return { startLine - 1, endLine - startLine + 1, LineDiff::getLines (text) };
}

static LineDiff::Change getInsertChange (const juce::Identifier& key,
                                         const jam::JsonDocument& jsonDoc,
                                         const jam::Document::Element& entry)
{
    const int line { getIntField (jsonDoc, entry, key) };
    const auto text { jsonDoc.getString (jsonDoc.get (entry, jam::Strings::getView (Id::text.toString()))) };
    const int changeStart { key == Id::insertBefore ? line - 1 : line };
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
            entryChanges.add (Id::deletion, &getDeleteChange);
            entryChanges.add (Id::startLine, &getReplaceChange);
            entryChanges.add (Id::insertBefore, &getInsertChange);
            entryChanges.add (Id::insertAfter, &getInsertChange);
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
            return getIntField (jsonDoc, *leftEntry, Id::startCol)
                 > getIntField (jsonDoc, *rightEntry, Id::startCol);
        });

    std::string lineText { targetDoc.getLine (lineNumber) };
    const auto textKey { jam::Strings::getView (Id::text.toString()) };

    for (int index { 0 }; index < lineEntryIndices.size(); ++index)
    {
        const auto* entry { jsonDoc.getChildByToken (rootArray, entryTokens.at (lineEntryIndices.at (index))) };
        const auto startCol { static_cast<size_t> (getIntField (jsonDoc, *entry, Id::startCol)) };
        const auto endCol { static_cast<size_t> (getIntField (jsonDoc, *entry, Id::endCol)) };
        const auto replacement { jsonDoc.getString (jsonDoc.get (*entry, textKey)) };
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
    const auto startColKey { jam::Strings::getView (Id::startCol.toString()) };
    jam::HashMap<int, jam::Array<int>> columnEditsByLine;
    jam::Array<LineDiff::Change> changes;

    for (int index { 0 }; index < entryIndices.size(); ++index)
    {
        const int entryIndex { entryIndices.at (index) };
        const auto* entry { jsonDoc.getChildByToken (rootArray, entryTokens.at (entryIndex)) };

        if (jsonDoc.contains (*entry, startColKey))
        {
            const auto [bucket, inserted] { columnEditsByLine.try_emplace (getIntField (jsonDoc, *entry, Id::line)) };
            auto& [bucketKey, bucketIndices] { *bucket };
            juce::ignoreUnused (bucketKey, inserted);
            bucketIndices.add (entryIndex);
        }
        else
            changes.add (getEntryChanges().get (getLineOperationKey (jsonDoc, *entry), jsonDoc, *entry));
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
    auto [readStatus, targetDoc] { TextDocument::fromFile (targetFile, displayPath) };

    if (readStatus.failed())
        return readStatus;

    if (targetDoc->isBinary())
        return juce::Result::fail (juce::String::formatted (text::Diagnostics::failBinaryFile, displayPath.toRawUTF8()));

    const auto fileStatus { PatchValidator::isValidForFile (jsonDoc, entryIndices, *targetDoc) };

    if (fileStatus.failed())
        return fileStatus;

    auto changes (getFileChanges (jsonDoc, rootArray, entryTokens, entryIndices, *targetDoc));
    transaction.addFile (targetFile, std::move (targetDoc), LineDiff { std::move (changes) }, entryIndices.size());
    return juce::Result::ok();
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

static int runTransaction (const Arguments& arguments, const jam::JsonDocument& jsonDoc)
{
    const auto* rootArray { jsonDoc.getChildByToken (*jsonDoc.getRoot(), jsonDoc.getValue()) };
    const auto& entryTokens { *rootArray->get<jam::Document::Tokens> (Id::tokens) };
    const auto [expectStatus, expectedCount] { getExpectStatus (arguments) };

    if (expectStatus.failed())
    {
        Stdio::printError (expectStatus.getErrorMessage());
        return Stdio::exitError;
    }

    if (arguments.contains (Id::expectOption) and expectedCount != entryTokens.size())
    {
        Stdio::printError (juce::String::formatted (text::Diagnostics::failExpectMismatch,
                                                    expectedCount,
                                                    entryTokens.size()));
        return Stdio::exitMismatch;
    }

    Transaction transaction { Id::patch.toString() };
    const auto addStatus { addGroupedFiles (transaction, jsonDoc, *rootArray, entryTokens) };

    if (addStatus.failed())
    {
        Stdio::printError (addStatus.getErrorMessage());
        return Stdio::exitError;
    }

    return transaction.run (arguments, false);
}

int Patch::run (const jam::Strings& argumentList)
{
    const Arguments arguments { argumentList,
                                1,
                                jam::Strings { Id::applyOption },
                                jam::Strings { Id::fileOption, Id::expectOption } };
    const auto [inputStatus, jsonBytes] { getValidatedInput (arguments) };

    if (inputStatus.failed())
    {
        Stdio::printError (inputStatus.getErrorMessage());
        return Stdio::exitError;
    }

    const auto [docStatus, jsonDoc] { getValidatedJsonDocument (jsonBytes) };

    if (docStatus.failed())
    {
        Stdio::printError (docStatus.getErrorMessage());
        return Stdio::exitError;
    }

    return runTransaction (arguments, *jsonDoc);
}
