#include "PatchValidator.h"
#include "generated/Generated.h"

static bool hasMember (const jam::JsonDocument& document,
                       const jam::Document::Element& object,
                       const juce::Identifier& key) noexcept
{
    return document.contains (object, jam::Strings::getView (key.toString()));
}

static const jam::Document::Token& getMember (const jam::JsonDocument& document,
                                              const jam::Document::Element& object,
                                              const juce::Identifier& key)
{
    return document.get (object, jam::Strings::getView (key.toString()));
}

static int getIntMember (const jam::JsonDocument& document,
                         const jam::Document::Element& object,
                         const juce::Identifier& key)
{
    return jam::Format::getNumber<int> (document.getView (getMember (document, object, key)));
}

static std::optional<int> getIntValue (const jam::JsonDocument& document,
                                       const jam::Document::Element& object,
                                       const juce::Identifier& key,
                                       int minimumValue)
{
    if (not hasMember (document, object, key))
        return std::nullopt;

    const auto& token { getMember (document, object, key) };

    if (token.type != map::JsonTokenType::number)
        return std::nullopt;

    const auto view { document.getView (token) };
    const bool isDigits { not view.empty()
                          and std::all_of (view.begin(), view.end(),
                                           [] (char ch) { return jam::Document::isDigit (static_cast<unsigned char> (ch)); }) };
    int parsed { 0 };
    const auto [endPointer, errorCode] { std::from_chars (view.data(), view.data() + view.size(), parsed) };

    if (isDigits and errorCode == std::errc {} and endPointer == view.data() + view.size() and parsed >= minimumValue)
        return jam::Format::getNumber<int> (view);

    return std::nullopt;
}

static bool hasValidString (const jam::JsonDocument& document,
                            const jam::Document::Element& object,
                            const juce::Identifier& key,
                            bool allowEmpty)
{
    if (not hasMember (document, object, key))
        return false;

    const auto& token { getMember (document, object, key) };

    if (token.type != map::JsonTokenType::string)
        return false;

    return allowEmpty or not document.getString (token).empty();
}

static bool hasOnlyAllowedKeys (const jam::JsonDocument& document,
                                const jam::Document::Element& object,
                                const jam::Strings& allowedKeys)
{
    const auto& tokens { *object.get<jam::Document::Tokens> (Id::tokens) };

    if (tokens.size() % jam::JsonDocument::numMemberTokens != 0)
        return false;

    jam::Strings seenKeys;

    for (int index { 0 }; index < tokens.size(); index += jam::JsonDocument::numMemberTokens)
    {
        const auto& keyToken { tokens.at (index) };

        if (keyToken.type != map::JsonTokenType::string)
            return false;

        const auto keyText { jam::Strings::fromView (document.getString (keyToken)) };

        if (not allowedKeys.contains (keyText, false) or not seenKeys.addIfNotAlreadyThere (keyText, false))
            return false;
    }

    return true;
}

static bool hasValidOptionalHash (const jam::JsonDocument& document,
                                  const jam::Document::Element& object)
{
    if (not hasMember (document, object, Id::hash))
        return true;

    const auto& token { getMember (document, object, Id::hash) };

    if (token.type != map::JsonTokenType::string)
        return false;

    const auto hashText { document.getString (token) };
    return static_cast<int> (hashText.size()) == TextDocument::lineHashLength
           and std::all_of (hashText.begin(), hashText.end(),
                            [] (char ch) { return jam::Document::isHexDigit (static_cast<unsigned char> (ch)); });
}

static bool hasValidBaseFields (const jam::JsonDocument& document,
                                const jam::Document::Element& object,
                                const jam::Strings& allowedKeys)
{
    return hasOnlyAllowedKeys (document, object, allowedKeys)
           and hasValidString (document, object, Id::file, false)
           and hasValidOptionalHash (document, object);
}

static bool hasValidStartEndLines (const jam::JsonDocument& document,
                                   const jam::Document::Element& object)
{
    const auto startLine { getIntValue (document, object, Id::startLine, 1) };
    const auto endLine { startLine.has_value() ? getIntValue (document, object, Id::endLine, *startLine) : std::nullopt };
    return startLine.has_value() and endLine.has_value();
}

static bool isValidColumnSpanEntry (const juce::Identifier&,
                                    const jam::JsonDocument& document,
                                    const jam::Document::Element& object)
{
    const jam::Strings allowedKeys { Id::file.toString(), Id::line.toString(), Id::startCol.toString(),
                                     Id::endCol.toString(), Id::text.toString(), Id::hash.toString() };
    const auto lineNumber { getIntValue (document, object, Id::line, 1) };
    const auto startColumn { getIntValue (document, object, Id::startCol, 0) };
    const auto endColumn { startColumn.has_value() ? getIntValue (document, object, Id::endCol, *startColumn) : std::nullopt };
    return hasValidBaseFields (document, object, allowedKeys)
           and lineNumber.has_value()
           and startColumn.has_value()
           and endColumn.has_value()
           and hasValidString (document, object, Id::text, true);
}

static bool isValidDeleteEntry (const juce::Identifier&,
                                const jam::JsonDocument& document,
                                const jam::Document::Element& object)
{
    const jam::Strings allowedKeys { Id::file.toString(), Id::line.toString(), Id::startLine.toString(),
                                     Id::endLine.toString(), Id::deletion.toString(), Id::hash.toString() };
    const auto& deleteToken { getMember (document, object, Id::deletion) };
    const bool isDeleteTrue { deleteToken.type == map::JsonTokenType::boolean and document.getBoolean (deleteToken) };
    const bool hasSingleLine { getIntValue (document, object, Id::line, 1).has_value()
                               and not hasMember (document, object, Id::startLine)
                               and not hasMember (document, object, Id::endLine) };
    const bool hasLineRange { hasValidStartEndLines (document, object) and not hasMember (document, object, Id::line) };
    return hasValidBaseFields (document, object, allowedKeys)
           and isDeleteTrue
           and (hasSingleLine or hasLineRange);
}

static bool isValidRangeReplaceEntry (const juce::Identifier&,
                                      const jam::JsonDocument& document,
                                      const jam::Document::Element& object)
{
    const jam::Strings allowedKeys { Id::file.toString(), Id::startLine.toString(), Id::endLine.toString(),
                                     Id::text.toString(), Id::hash.toString() };
    return hasValidBaseFields (document, object, allowedKeys)
           and hasValidStartEndLines (document, object)
           and hasValidString (document, object, Id::text, true);
}

static bool isValidInsertEntry (const juce::Identifier& key,
                                const jam::JsonDocument& document,
                                const jam::Document::Element& object)
{
    const int minimumLine { key == Id::insertAfter ? 0 : 1 };
    const jam::Strings allowedKeys { Id::file.toString(), key.toString(), Id::text.toString(), Id::hash.toString() };
    const auto targetLine { getIntValue (document, object, key, minimumLine) };
    return hasValidBaseFields (document, object, allowedKeys)
           and targetLine.has_value()
           and hasValidString (document, object, Id::text, true);
}

static const jam::Function::Map<juce::Identifier, jam::Function::pointerCapacity, bool (const jam::JsonDocument&, const jam::Document::Element&)>& getEntryValidators()
{
    static const auto entryValidators (
        []
        {
            jam::Function::Map<juce::Identifier, jam::Function::pointerCapacity, bool (const jam::JsonDocument&, const jam::Document::Element&)> entryValidators;
            entryValidators.add (Id::startCol, &isValidColumnSpanEntry);
            entryValidators.add (Id::deletion, &isValidDeleteEntry);
            entryValidators.add (Id::startLine, &isValidRangeReplaceEntry);
            entryValidators.add (Id::insertAfter, &isValidInsertEntry);
            entryValidators.add (Id::insertBefore, &isValidInsertEntry);
            return entryValidators;
        }());

    return entryValidators;
}

static juce::Identifier getEntryShapeKey (const jam::JsonDocument& document,
                                          const jam::Document::Element& object)
{
    const bool hasDeletion { hasMember (document, object, Id::deletion) };
    const bool hasColumnOrInsert { hasMember (document, object, Id::startCol)
                                   or hasMember (document, object, Id::insertAfter)
                                   or hasMember (document, object, Id::insertBefore) };

    if (hasDeletion)
        return hasColumnOrInsert ? juce::Identifier {} : Id::deletion;

    const jam::Array<juce::Identifier> shapeKeys { Id::startCol, Id::startLine, Id::insertAfter, Id::insertBefore };
    juce::Identifier matchedKey;
    int matchCount { 0 };

    for (int index { 0 }; index < shapeKeys.size(); ++index)
    {
        if (hasMember (document, object, shapeKeys.at (index)))
        {
            matchedKey = shapeKeys.at (index);
            ++matchCount;
        }
    }

    return matchCount == 1 ? matchedKey : juce::Identifier {};
}

static juce::String getEntryError (int index, const char* reason)
{
    return juce::String::formatted (text::Diagnostics::failPatchEntryInvalid, index, reason);
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

    const auto shapeKey { getEntryShapeKey (document, *entryElement) };

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
        return juce::Result::fail (juce::String::formatted (text::Diagnostics::failPatchJsonInvalid,
                                                            text::Diagnostics::failPatchRootNotArray));

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

static juce::Range<int> getEntryLineRange (const jam::JsonDocument& document,
                                           const jam::Document::Element& object)
{
    if (hasMember (document, object, Id::line))
    {
        const int line { getIntMember (document, object, Id::line) };
        return { line, line + 1 };
    }

    const int startLine { getIntMember (document, object, Id::startLine) };
    const int endLine { getIntMember (document, object, Id::endLine) };
    return { startLine, endLine + 1 };
}

static std::pair<bool, int> getColumnSpanBounds (const juce::Identifier&,
                                                 const jam::JsonDocument& document,
                                                 const jam::Document::Element& object,
                                                 const TextDocument& targetDocument)
{
    const int line { getIntMember (document, object, Id::line) };
    const int startCol { getIntMember (document, object, Id::startCol) };
    const int endCol { getIntMember (document, object, Id::endCol) };

    if (line < 1 or line > targetDocument.getNumLines())
        return { false, 0 };

    const int lineLength { static_cast<int> (targetDocument.getLine (line).size()) };
    return { startCol >= 0 and startCol <= endCol and endCol <= lineLength, line };
}

static std::pair<bool, int> getLineRangeBounds (const juce::Identifier&,
                                                const jam::JsonDocument& document,
                                                const jam::Document::Element& object,
                                                const TextDocument& targetDocument)
{
    const auto range { getEntryLineRange (document, object) };
    const bool inBounds { not range.isEmpty()
                          and range.getStart() >= 1
                          and range.getEnd() <= targetDocument.getNumLines() + 1 };
    return { inBounds, range.getStart() };
}

static std::pair<bool, int> getInsertBounds (const juce::Identifier& key,
                                             const jam::JsonDocument& document,
                                             const jam::Document::Element& object,
                                             const TextDocument& targetDocument)
{
    const int line { getIntMember (document, object, key) };
    const int minimumLine { key == Id::insertAfter ? 0 : 1 };
    return { line >= minimumLine and line <= targetDocument.getNumLines(), line };
}

static const jam::Function::Map<juce::Identifier, jam::Function::pointerCapacity, std::pair<bool, int> (const jam::JsonDocument&, const jam::Document::Element&, const TextDocument&)>& getEntryBounds()
{
    static const auto entryBounds (
        []
        {
            jam::Function::Map<juce::Identifier, jam::Function::pointerCapacity, std::pair<bool, int> (const jam::JsonDocument&, const jam::Document::Element&, const TextDocument&)> entryBounds;
            entryBounds.add (Id::startCol, &getColumnSpanBounds);
            entryBounds.add (Id::deletion, &getLineRangeBounds);
            entryBounds.add (Id::startLine, &getLineRangeBounds);
            entryBounds.add (Id::insertAfter, &getInsertBounds);
            entryBounds.add (Id::insertBefore, &getInsertBounds);
            return entryBounds;
        }());

    return entryBounds;
}

static juce::Result getHashCheckResult (const jam::JsonDocument& document,
                                        const jam::Document::Element& object,
                                        const TextDocument& targetDocument,
                                        int hashLine)
{
    if (not hasMember (document, object, Id::hash))
        return juce::Result::ok();

    const auto expectedHash { jam::Strings::fromView (document.getString (getMember (document, object, Id::hash))) };
    const auto actualHash { hashLine >= 1 ? targetDocument.getLineHash (hashLine) : juce::String {} };

    if (actualHash.compare (expectedHash) != 0)
        return juce::Result::fail (juce::String::formatted (text::Diagnostics::failHashMismatch,
                                                            targetDocument.getDisplayPath().toRawUTF8(),
                                                            hashLine,
                                                            expectedHash.toRawUTF8(),
                                                            actualHash.toRawUTF8()));

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
        const auto shapeKey { getEntryShapeKey (document, *entryElement) };
        jassert (not shapeKey.isNull());
        const auto [inBounds, hashLine] { getEntryBounds().get (shapeKey, document, *entryElement, targetDocument) };

        if (not inBounds)
            return juce::Result::fail (juce::String::formatted (text::Diagnostics::failPatchBounds,
                                                                targetDocument.getDisplayPath().toRawUTF8(),
                                                                entryIndex));

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

        if (hasMember (document, *entryElement, Id::startCol))
            spans.add ({ getIntMember (document, *entryElement, Id::line),
                         getIntMember (document, *entryElement, Id::startCol),
                         getIntMember (document, *entryElement, Id::endCol) });
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

        if (hasMember (document, *entryElement, Id::deletion) or hasMember (document, *entryElement, Id::startLine))
            ranges.add (getEntryLineRange (document, *entryElement));
    }

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

        if (hasMember (document, *entryElement, Id::insertAfter))
            insertLines.add (getIntMember (document, *entryElement, Id::insertAfter));

        if (hasMember (document, *entryElement, Id::insertBefore))
            insertLines.add (getIntMember (document, *entryElement, Id::insertBefore));
    }

    insertLines.sort();
    return insertLines;
}

static juce::Result getConflictFailure (const juce::String& displayPath, int conflictLine)
{
    return juce::Result::fail (juce::String::formatted (text::Diagnostics::failPatchConflict,
                                                        displayPath.toRawUTF8(),
                                                        conflictLine));
}

static bool isLineInRanges (const jam::Array<juce::Range<int>>& ranges, int line) noexcept
{
    for (int index { 0 }; index < ranges.size(); ++index)
        if (ranges.at (index).contains (line))
            return true;

    return false;
}

static juce::Result getColumnSpanConflict (const jam::Array<std::tuple<int, int, int>>& columnSpans,
                                           const jam::Array<juce::Range<int>>& lineRanges,
                                           const juce::String& displayPath)
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
                                          const juce::String& displayPath)
{
    for (int firstIndex { 0 }; firstIndex < lineRanges.size(); ++firstIndex)
    {
        for (int secondIndex { firstIndex + 1 }; secondIndex < lineRanges.size(); ++secondIndex)
        {
            if (lineRanges.at (firstIndex).intersects (lineRanges.at (secondIndex)))
                return getConflictFailure (displayPath,
                                           lineRanges.at (firstIndex).getIntersectionWith (lineRanges.at (secondIndex)).getStart());
        }
    }

    return juce::Result::ok();
}

static juce::Result getInsertConflict (const jam::Array<int>& insertLines,
                                       const jam::Array<juce::Range<int>>& lineRanges,
                                       const juce::String& displayPath)
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
    rules.add (Id::patch.toString(), &isValidPatchSchema);
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
    const auto& displayPath { targetDocument.getDisplayPath() };
    const auto columnConflict { getColumnSpanConflict (columnSpans, lineRanges, displayPath) };

    if (columnConflict.failed())
        return columnConflict;

    const auto rangeConflict { getLineRangeConflict (lineRanges, displayPath) };

    if (rangeConflict.failed())
        return rangeConflict;

    return getInsertConflict (insertLines, lineRanges, displayPath);
}
