#include "PatchValidator.h"
#include "Arguments.h"
#include "Stdio.h"
#include "generated/Generated.h"

int PatchValidator::getIntMember (const jam::JsonDocument& document,
                                  const jam::Document::Element& object,
                                  std::string_view key)
{
    return jam::Format::getNumber<int> (document.getView (document.get (object, key)));
}

static std::optional<int> getIntValue (const jam::JsonDocument& document,
                                       const jam::Document::Element& object,
                                       std::string_view key,
                                       int minimumValue)
{
    if (not document.contains (object, key))
        return std::nullopt;

    const auto& token { document.get (object, key) };

    if (token.type != map::JsonTokenType::number)
        return std::nullopt;

    const auto parsedValue { Arguments::getInteger (document.getView (token)) };

    if (parsedValue.has_value() and *parsedValue >= minimumValue and *parsedValue < Arguments::openRangeEnd)
        return parsedValue;

    return std::nullopt;
}

static bool hasValidString (const jam::JsonDocument& document,
                            const jam::Document::Element& object,
                            std::string_view key,
                            bool allowEmpty)
{
    if (not document.contains (object, key))
        return false;

    const auto& token { document.get (object, key) };

    if (token.type != map::JsonTokenType::string)
        return false;

    return allowEmpty or not document.getString (token).empty();
}

static bool hasEarlierKey (const jam::JsonDocument& document,
                           const jam::Document::Tokens& tokens,
                           int index,
                           std::string_view keyText)
{
    for (int earlierIndex { 0 }; earlierIndex < index; earlierIndex += jam::JsonDocument::numMemberTokens)
        if (document.getString (tokens.at (earlierIndex)).compare (keyText) == 0)
            return true;

    return false;
}

static bool hasOnlyAllowedKeys (const jam::JsonDocument& document,
                                const jam::Document::Element& object,
                                juce::Span<const std::string_view> allowedKeys)
{
    const auto& tokens { *object.get<jam::Document::Tokens> (Id::tokens) };

    if (tokens.size() % jam::JsonDocument::numMemberTokens != 0)
        return false;

    for (int index { 0 }; index < tokens.size(); index += jam::JsonDocument::numMemberTokens)
    {
        const auto& keyToken { tokens.at (index) };

        if (keyToken.type != map::JsonTokenType::string)
            return false;

        const auto keyText { document.getString (keyToken) };
        const bool isAllowedKey { std::any_of (allowedKeys.begin(), allowedKeys.end(),
                                               [&keyText] (std::string_view allowedKey)
                                               {
                                                   return allowedKey.compare (keyText) == 0;
                                               }) };

        if (not isAllowedKey or hasEarlierKey (document, tokens, index, keyText))
            return false;
    }

    return true;
}

static bool hasValidOptionalHash (const jam::JsonDocument& document,
                                  const jam::Document::Element& object)
{
    if (not document.contains (object, Id::hash))
        return true;

    const auto& token { document.get (object, Id::hash) };

    if (token.type != map::JsonTokenType::string)
        return false;

    const auto hashText { document.getString (token) };
    return static_cast<int> (hashText.size()) == TextDocument::lineHashLength
           and std::all_of (hashText.begin(), hashText.end(),
                            [] (char ch) { return jam::Document::isHexDigit (static_cast<unsigned char> (ch)); });
}

static bool hasValidBaseFields (const jam::JsonDocument& document,
                                const jam::Document::Element& object,
                                juce::Span<const std::string_view> allowedKeys)
{
    return hasOnlyAllowedKeys (document, object, allowedKeys)
           and hasValidString (document, object, jam::Strings::getView (Id::file.toString()), false)
           and hasValidOptionalHash (document, object);
}

static bool hasValidStartEndLines (const jam::JsonDocument& document,
                                   const jam::Document::Element& object)
{
    const auto startLine { getIntValue (document, object, Id::startLine, 1) };
    const auto endLine { startLine.has_value() ? getIntValue (document, object, Id::lastLine, *startLine) : std::nullopt };
    return startLine.has_value() and endLine.has_value();
}

static bool isValidColumnSpanEntry (const juce::Identifier&,
                                    const jam::JsonDocument& document,
                                    const jam::Document::Element& object)
{
    const std::array allowedKeys { jam::Strings::getView (Id::file.toString()),
                                   jam::Strings::getView (Id::line.toString()),
                                   Id::startCol,
                                   Id::endCol,
                                   jam::Strings::getView (Id::text.toString()),
                                   Id::hash };
    const auto lineNumber { getIntValue (document, object, jam::Strings::getView (Id::line.toString()), 1) };
    const auto startColumn { getIntValue (document, object, Id::startCol, 0) };
    const auto endColumn { startColumn.has_value() ? getIntValue (document, object, Id::endCol, *startColumn) : std::nullopt };
    return hasValidBaseFields (document, object, allowedKeys)
           and lineNumber.has_value()
           and startColumn.has_value()
           and endColumn.has_value()
           and hasValidString (document, object, jam::Strings::getView (Id::text.toString()), true);
}

static bool isValidDeleteEntry (const juce::Identifier&,
                                const jam::JsonDocument& document,
                                const jam::Document::Element& object)
{
    const std::array allowedKeys { jam::Strings::getView (Id::file.toString()),
                                   jam::Strings::getView (Id::line.toString()),
                                   Id::startLine,
                                   Id::lastLine,
                                   Id::deletion,
                                   Id::hash };
    const auto& deleteToken { document.get (object, Id::deletion) };
    const bool isDeleteTrue { deleteToken.type == map::JsonTokenType::boolean and document.getBoolean (deleteToken) };
    const bool hasSingleLine { getIntValue (document, object, jam::Strings::getView (Id::line.toString()), 1).has_value()
                               and not document.contains (object, Id::startLine)
                               and not document.contains (object, Id::lastLine) };
    const bool hasLineRange { hasValidStartEndLines (document, object) and not document.contains (object, jam::Strings::getView (Id::line.toString())) };
    return hasValidBaseFields (document, object, allowedKeys)
           and isDeleteTrue
           and (hasSingleLine or hasLineRange);
}

static bool isValidRangeReplaceEntry (const juce::Identifier&,
                                      const jam::JsonDocument& document,
                                      const jam::Document::Element& object)
{
    const std::array allowedKeys { jam::Strings::getView (Id::file.toString()),
                                   Id::startLine,
                                   Id::lastLine,
                                   jam::Strings::getView (Id::text.toString()),
                                   Id::hash };
    return hasValidBaseFields (document, object, allowedKeys)
           and hasValidStartEndLines (document, object)
           and hasValidString (document, object, jam::Strings::getView (Id::text.toString()), true);
}

static bool isValidInsertEntry (const juce::Identifier& key,
                                const jam::JsonDocument& document,
                                const jam::Document::Element& object)
{
    static const juce::Identifier insertAfterKey { jam::Strings::fromView (Id::insertAfter) };
    const int minimumLine { key == insertAfterKey ? 0 : 1 };
    const std::array allowedKeys { jam::Strings::getView (Id::file.toString()),
                                   jam::Strings::getView (key.toString()),
                                   jam::Strings::getView (Id::text.toString()),
                                   Id::hash };
    const auto targetLine { getIntValue (document, object, jam::Strings::getView (key.toString()), minimumLine) };
    return hasValidBaseFields (document, object, allowedKeys)
           and targetLine.has_value()
           and hasValidString (document, object, jam::Strings::getView (Id::text.toString()), true);
}

static const jam::Function::Map<juce::Identifier, jam::Function::pointerCapacity, bool (const jam::JsonDocument&, const jam::Document::Element&)>& getEntryValidators()
{
    static const auto entryValidators (
        []
        {
            jam::Function::Map<juce::Identifier, jam::Function::pointerCapacity, bool (const jam::JsonDocument&, const jam::Document::Element&)> entryValidators;
            entryValidators.add (juce::Identifier (jam::Strings::fromView (Id::startCol)), &isValidColumnSpanEntry);
            entryValidators.add (juce::Identifier (jam::Strings::fromView (Id::deletion)), &isValidDeleteEntry);
            entryValidators.add (juce::Identifier (jam::Strings::fromView (Id::startLine)), &isValidRangeReplaceEntry);
            entryValidators.add (juce::Identifier (jam::Strings::fromView (Id::insertAfter)), &isValidInsertEntry);
            entryValidators.add (juce::Identifier (jam::Strings::fromView (Id::insertBefore)), &isValidInsertEntry);
            return entryValidators;
        }());

    return entryValidators;
}

juce::Identifier PatchValidator::getEntryShapeKey (const jam::JsonDocument& document,
                                                   const jam::Document::Element& object)
{
    static const juce::Identifier deletionKey { jam::Strings::fromView (Id::deletion) };
    const bool hasDeletion { document.contains (object, Id::deletion) };
    const bool hasColumnOrInsert { document.contains (object, Id::startCol)
                                   or document.contains (object, Id::insertAfter)
                                   or document.contains (object, Id::insertBefore) };

    if (hasDeletion)
        return hasColumnOrInsert ? juce::Identifier {} : deletionKey;

    const std::array shapeKeys { Id::startCol, Id::startLine, Id::insertAfter, Id::insertBefore };
    std::string_view matchedKey;
    int matchCount { 0 };

    for (const auto shapeKey : shapeKeys)
    {
        if (document.contains (object, shapeKey))
        {
            matchedKey = shapeKey;
            ++matchCount;
        }
    }

    return matchCount == 1 ? juce::Identifier (jam::Strings::fromView (matchedKey)) : juce::Identifier {};
}

static juce::String getEntryError (int index, std::string_view reason)
{
    return Stdio::fail (text::Diagnostics::failPatchEntryInvalid, index, jam::Strings::getView (Id::diagnosticSeparator), reason).getErrorMessage();
}

static juce::String getEntryFailure (const jam::JsonDocument& document,
                                     const jam::Document::Element& arrayElement,
                                     const jam::Document::Token& entryToken,
                                     int index)
{
    const auto* entryElement { entryToken.type == map::JsonTokenType::object
                                   ? document.getChildByToken (arrayElement, entryToken)
                                   : nullptr };

    if (entryElement == nullptr)
        return getEntryError (index, text::Diagnostics::failPatchEntryNotObject);

    const auto shapeKey { PatchValidator::getEntryShapeKey (document, *entryElement) };

    if (shapeKey.isNull())
        return getEntryError (index, text::Diagnostics::failPatchEntryShape);

    if (not getEntryValidators().get (shapeKey, document, *entryElement))
        return getEntryError (index, text::Diagnostics::failPatchEntryFields);

    return {};
}

static juce::Result isValidPatchSchema (const juce::String&, const jam::Document& document)
{
    const auto& jsonDocument { static_cast<const jam::JsonDocument&> (document) };
    const auto& rootTokens { *document.getRoot()->get<jam::Document::Tokens> (Id::tokens) };
    const auto* arrayElement { not rootTokens.isEmpty() and rootTokens.first().type == map::JsonTokenType::array
                                   ? jsonDocument.getChildByToken (*document.getRoot(), rootTokens.first())
                                   : nullptr };

    if (arrayElement == nullptr)
        return Stdio::fail (text::Diagnostics::failPatchJsonInvalid, text::Diagnostics::failPatchRootNotArray);

    const auto& entryTokens { *arrayElement->get<jam::Document::Tokens> (Id::tokens) };
    jam::Strings failures;

    for (int index { 0 }; index < entryTokens.size(); ++index)
    {
        const auto entryFailure { getEntryFailure (jsonDocument, *arrayElement, entryTokens.at (index), index) };

        if (entryFailure.isNotEmpty())
            failures.add (entryFailure);
    }

    return jam::Document::Validator::getFailures (failures);
}

juce::Range<int> PatchValidator::getEntryLineRange (const jam::JsonDocument& document,
                                                    const jam::Document::Element& object)
{
    if (document.contains (object, jam::Strings::getView (Id::line.toString())))
    {
        const int line { getIntMember (document, object, jam::Strings::getView (Id::line.toString())) };
        return { line, line + 1 };
    }

    const int startLine { getIntMember (document, object, Id::startLine) };
    const int endLine { getIntMember (document, object, Id::lastLine) };
    return { startLine, endLine + 1 };
}

static std::pair<bool, int> getColumnSpanBounds (const juce::Identifier&,
                                                 const jam::JsonDocument& document,
                                                 const jam::Document::Element& object,
                                                 const TextDocument& targetDocument)
{
    const int line { PatchValidator::getIntMember (document, object, jam::Strings::getView (Id::line.toString())) };
    const int endCol { PatchValidator::getIntMember (document, object, Id::endCol) };

    if (line > targetDocument.getNumLines())
        return { false, 0 };

    const int lineLength { static_cast<int> (targetDocument.getLine (line).size()) };
    return { endCol <= lineLength, line };
}

static std::pair<bool, int> getLineRangeBounds (const juce::Identifier&,
                                                const jam::JsonDocument& document,
                                                const jam::Document::Element& object,
                                                const TextDocument& targetDocument)
{
    const auto range { PatchValidator::getEntryLineRange (document, object) };
    const bool inBounds { range.getEnd() <= targetDocument.getNumLines() + 1 };
    return { inBounds, range.getStart() };
}

static std::pair<bool, int> getInsertBounds (const juce::Identifier& key,
                                             const jam::JsonDocument& document,
                                             const jam::Document::Element& object,
                                             const TextDocument& targetDocument)
{
    const int line { PatchValidator::getIntMember (document, object, jam::Strings::getView (key.toString())) };
    return { line <= targetDocument.getNumLines(), line };
}

static const jam::Function::Map<juce::Identifier, jam::Function::pointerCapacity, std::pair<bool, int> (const jam::JsonDocument&, const jam::Document::Element&, const TextDocument&)>& getEntryBounds()
{
    static const auto entryBounds (
        []
        {
            jam::Function::Map<juce::Identifier, jam::Function::pointerCapacity, std::pair<bool, int> (const jam::JsonDocument&, const jam::Document::Element&, const TextDocument&)> entryBounds;
            entryBounds.add (juce::Identifier (jam::Strings::fromView (Id::startCol)), &getColumnSpanBounds);
            entryBounds.add (juce::Identifier (jam::Strings::fromView (Id::deletion)), &getLineRangeBounds);
            entryBounds.add (juce::Identifier (jam::Strings::fromView (Id::startLine)), &getLineRangeBounds);
            entryBounds.add (juce::Identifier (jam::Strings::fromView (Id::insertAfter)), &getInsertBounds);
            entryBounds.add (juce::Identifier (jam::Strings::fromView (Id::insertBefore)), &getInsertBounds);
            return entryBounds;
        }());

    return entryBounds;
}

static juce::Result getHashCheckResult (const jam::JsonDocument& document,
                                        const jam::Document::Element& object,
                                        const TextDocument& targetDocument,
                                        int hashLine)
{
    if (document.contains (object, Id::hash))
        return targetDocument.getLineHashStatus (hashLine, document.getString (document.get (object, Id::hash)));

    return juce::Result::ok();
}

static juce::Result getBoundsAndHashResult (const jam::JsonDocument& document,
                                            const jam::Document::Element& arrayElement,
                                            const jam::Document::Tokens& entryTokens,
                                            const jam::Array<int>& entryIndices,
                                            const TextDocument& targetDocument)
{
    for (int index { 0 }; index < entryIndices.size(); ++index)
    {
        const int entryIndex { entryIndices.at (index) };
        const auto* entryElement { document.getChildByToken (arrayElement, entryTokens.at (entryIndex)) };
        jassert (entryElement != nullptr);
        const auto shapeKey { PatchValidator::getEntryShapeKey (document, *entryElement) };
        jassert (not shapeKey.isNull());
        const auto [inBounds, hashLine] { getEntryBounds().get (shapeKey, document, *entryElement, targetDocument) };

        if (not inBounds)
            return Stdio::fail (text::Diagnostics::failPatchBounds,
                                targetDocument.getDisplayPath(),
                                text::Diagnostics::atEntry,
                                entryIndex);

        const auto hashResult { getHashCheckResult (document, *entryElement, targetDocument, hashLine) };

        if (hashResult.failed())
            return hashResult;
    }

    return juce::Result::ok();
}

static jam::Array<std::tuple<int, int, int>> getColumnSpans (const jam::JsonDocument& document,
                                                             const jam::Document::Element& arrayElement,
                                                             const jam::Document::Tokens& entryTokens,
                                                             const jam::Array<int>& entryIndices)
{
    jam::Array<std::tuple<int, int, int>> spans;

    for (int index { 0 }; index < entryIndices.size(); ++index)
    {
        const auto* entryElement { document.getChildByToken (arrayElement, entryTokens.at (entryIndices.at (index))) };

        if (document.contains (*entryElement, Id::startCol))
            spans.add ({ PatchValidator::getIntMember (document, *entryElement, jam::Strings::getView (Id::line.toString())),
                         PatchValidator::getIntMember (document, *entryElement, Id::startCol),
                         PatchValidator::getIntMember (document, *entryElement, Id::endCol) });
    }

    spans.sort();
    return spans;
}

static jam::Array<juce::Range<int>> getLineRanges (const jam::JsonDocument& document,
                                                   const jam::Document::Element& arrayElement,
                                                   const jam::Document::Tokens& entryTokens,
                                                   const jam::Array<int>& entryIndices)
{
    jam::Array<juce::Range<int>> ranges;

    for (int index { 0 }; index < entryIndices.size(); ++index)
    {
        const auto* entryElement { document.getChildByToken (arrayElement, entryTokens.at (entryIndices.at (index))) };

        if (document.contains (*entryElement, Id::deletion) or document.contains (*entryElement, Id::startLine))
            ranges.add (PatchValidator::getEntryLineRange (document, *entryElement));
    }

    std::sort (ranges.begin(), ranges.end(),
        [] (const juce::Range<int>& leftRange, const juce::Range<int>& rightRange)
        {
            return leftRange.getStart() < rightRange.getStart();
        });

    return ranges;
}

static jam::Array<int> getInsertLines (const jam::JsonDocument& document,
                                       const jam::Document::Element& arrayElement,
                                       const jam::Document::Tokens& entryTokens,
                                       const jam::Array<int>& entryIndices)
{
    jam::Array<int> insertLines;

    for (int index { 0 }; index < entryIndices.size(); ++index)
    {
        const auto* entryElement { document.getChildByToken (arrayElement, entryTokens.at (entryIndices.at (index))) };

        if (document.contains (*entryElement, Id::insertAfter))
            insertLines.add (PatchValidator::getIntMember (document, *entryElement, Id::insertAfter));

        if (document.contains (*entryElement, Id::insertBefore))
            insertLines.add (PatchValidator::getIntMember (document, *entryElement, Id::insertBefore));
    }

    insertLines.sort();
    return insertLines;
}

static juce::Result getConflictFailure (std::string_view displayPath, int conflictLine)
{
    return Stdio::fail (text::Diagnostics::failPatchConflict, displayPath, text::Diagnostics::atLine, conflictLine);
}

static bool isLineInRanges (const jam::Array<juce::Range<int>>& ranges, int line) noexcept
{
    const auto rangeEnd { std::upper_bound (ranges.begin(), ranges.end(), line,
        [] (int value, const juce::Range<int>& range)
        {
            return value < range.getStart();
        }) };

    return rangeEnd != ranges.begin() and std::prev (rangeEnd)->contains (line);
}

static juce::Result getColumnSpanConflict (const jam::Array<std::tuple<int, int, int>>& columnSpans,
                                           const jam::Array<juce::Range<int>>& lineRanges,
                                           std::string_view displayPath)
{
    for (int index { 0 }; index < columnSpans.size(); ++index)
    {
        const auto& [line, startCol, endCol] { columnSpans.at (index) };
        juce::ignoreUnused (startCol, endCol);

        if (isLineInRanges (lineRanges, line))
            return getConflictFailure (displayPath, line);
    }

    for (int index { 1 }; index < columnSpans.size(); ++index)
    {
        const auto& [prevLine, prevStartCol, prevEndCol] { columnSpans.at (index - 1) };
        const auto& [currLine, currStartCol, currEndCol] { columnSpans.at (index) };
        juce::ignoreUnused (currEndCol);

        if (prevLine == currLine and (prevEndCol > currStartCol or prevStartCol == currStartCol))
            return getConflictFailure (displayPath, currLine);
    }

    return juce::Result::ok();
}

static juce::Result getLineRangeConflict (const jam::Array<juce::Range<int>>& lineRanges,
                                          std::string_view displayPath)
{
    for (int index { 1 }; index < lineRanges.size(); ++index)
    {
        if (lineRanges.at (index - 1).intersects (lineRanges.at (index)))
            return getConflictFailure (displayPath, lineRanges.at (index).getStart());
    }

    return juce::Result::ok();
}

static juce::Result getInsertConflict (const jam::Array<int>& insertLines,
                                       const jam::Array<juce::Range<int>>& lineRanges,
                                       std::string_view displayPath)
{
    for (int index { 0 }; index < insertLines.size(); ++index)
        if (isLineInRanges (lineRanges, insertLines.at (index)))
            return getConflictFailure (displayPath, insertLines.at (index));

    for (int index { 1 }; index < insertLines.size(); ++index)
        if (insertLines.at (index) == insertLines.at (index - 1))
            return getConflictFailure (displayPath, insertLines.at (index));

    return juce::Result::ok();
}

void PatchValidator::addRules (Rules& rules)
{
    jam::JsonValidator::addRules (rules);
    rules.add (jam::Strings::fromView (Id::patch), &isValidPatchSchema);
}

const PatchValidator::Rules& PatchValidator::getRules() const
{
    static const auto rules (
        []
        {
            Rules rules;
            addRules (rules);
            return rules;
        }());

    return rules;
}

juce::Result PatchValidator::isValidForFile (const jam::JsonDocument& document,
                                             const jam::Array<int>& entryIndices,
                                             const TextDocument& targetDocument)
{
    const auto& rootToken { document.getValue() };
    const auto* arrayElement { document.getChildByToken (*document.getRoot(), rootToken) };
    jassert (arrayElement != nullptr);
    const auto& entryTokens { *arrayElement->get<jam::Document::Tokens> (Id::tokens) };
    const auto boundsResult { getBoundsAndHashResult (document, *arrayElement, entryTokens, entryIndices, targetDocument) };

    if (boundsResult.failed())
        return boundsResult;

    const auto columnSpans (getColumnSpans (document, *arrayElement, entryTokens, entryIndices));
    const auto lineRanges (getLineRanges (document, *arrayElement, entryTokens, entryIndices));
    const auto insertLines (getInsertLines (document, *arrayElement, entryTokens, entryIndices));
    const auto displayPath { targetDocument.getDisplayPath() };
    const auto columnConflict { getColumnSpanConflict (columnSpans, lineRanges, displayPath) };

    if (columnConflict.failed())
        return columnConflict;

    const auto rangeConflict { getLineRangeConflict (lineRanges, displayPath) };

    if (rangeConflict.failed())
        return rangeConflict;

    return getInsertConflict (insertLines, lineRanges, displayPath);
}
