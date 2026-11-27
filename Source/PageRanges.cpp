#include "PageRanges.h"
#include "Arguments.h"
#include "Stdio.h"
#include "TextDocument.h"
#include "generated/Generated.h"

static constexpr int firstLine { 1 };
static constexpr int noLine { 0 };
static constexpr size_t signLength { 1 };
static constexpr size_t fieldCount { 3 };
static constexpr char relativeSign { static_cast<char> (Chars::plus) };
static constexpr char fromEndSign { static_cast<char> (Chars::dash) };
static constexpr char colonSign { static_cast<char> (Chars::colon) };
static constexpr char newlineChar { static_cast<char> (Chars::newline) };
static constexpr size_t noColonCount { 0 };
static constexpr size_t twoColonCount { 2 };

using Fields = std::array<std::string_view, fieldCount>;

static bool startsWithSign (std::string_view field, char sign)
{
    return not field.empty() and field.front() == sign;
}

static std::pair<bool, int> getNumber (std::string_view digits)
{
    const auto value { Arguments::getInteger (digits) };
    const bool isValid { value.has_value() and *value >= noLine };
    return { isValid, isValid ? *value : noLine };
}

static std::pair<bool, int> getPlainNumber (std::string_view field)
{
    if (startsWithSign (field, relativeSign))
        return getNumber (field.substr (signLength));

    return getNumber (field);
}

static juce::Range<int> getClampedRange (int startLine, int endLine, int lineCount)
{
    const int firstSelected { std::max (startLine, firstLine) };
    const int lastSelected { std::min (endLine, lineCount) };
    return juce::Range<int> { firstSelected, std::max (lastSelected + firstLine, firstSelected) };
}

static std::pair<bool, int> getStartLine (std::string_view startText, int lineCount)
{
    if (startsWithSign (startText, fromEndSign))
    {
        const auto [isValid, fromEndCount] { getNumber (startText.substr (signLength)) };
        return { isValid, fromEndCount == noLine ? noLine : lineCount - fromEndCount + firstLine };
    }

    if (startText.empty())
        return { true, noLine };

    return getPlainNumber (startText);
}

static std::pair<bool, int> getEndLine (std::string_view endText, bool hasContext, int startLine, int lineCount)
{
    if (endText.empty())
        return { true, hasContext ? startLine : lineCount };

    if (startsWithSign (endText, relativeSign))
    {
        const auto [isValid, offset] { getNumber (endText.substr (signLength)) };
        return { isValid, startLine + offset };
    }

    return getNumber (endText);
}

static std::pair<bool, int> getContextSize (std::string_view contextText)
{
    if (contextText.empty())
        return { true, noLine };

    return getNumber (contextText);
}

static std::pair<bool, juce::Range<int>> getBackwardRange (std::string_view startText, std::string_view endText, int lineCount)
{
    const auto [isValid, fromEndCount] { getNumber (endText.substr (signLength)) };

    if (startsWithSign (startText, fromEndSign) or not isValid)
        return { false, {} };

    if (startText.empty())
        return { true, getClampedRange (noLine, lineCount - fromEndCount, lineCount) };

    const auto [isStartValid, startLine] { getPlainNumber (startText) };
    return { isStartValid, getClampedRange (startLine - fromEndCount, startLine, lineCount) };
}

static std::pair<bool, juce::Range<int>> getColonRange (const Fields& fields, bool hasContext, int lineCount)
{
    const auto& [startText, endText, contextText] { fields };

    if (startText.empty() and endText.empty())
        return { false, {} };

    if (hasContext and contextText.empty())
        return { false, {} };

    if (startsWithSign (endText, fromEndSign))
        return getBackwardRange (startText, endText, lineCount);

    const auto [isStartValid, startLine] { getStartLine (startText, lineCount) };
    const auto [isEndValid, endLine] { getEndLine (endText, hasContext, startLine, lineCount) };
    const auto [isContextValid, contextSize] { getContextSize (contextText) };
    const bool isValid { isStartValid and isEndValid and isContextValid };
    return { isValid, getClampedRange (startLine - contextSize, endLine + contextSize, lineCount) };
}

static std::optional<Fields> getFields (std::string_view rangeText)
{
    const auto firstColon { jam::simd::findByte (rangeText.data(), rangeText.size(), 0, colonSign) };
    const auto secondColon { jam::simd::findByte (rangeText.data(), rangeText.size(), firstColon + signLength, colonSign) };

    if (secondColon == rangeText.size())
        return Fields { rangeText.substr (0, firstColon), rangeText.substr (firstColon + signLength), std::string_view {} };

    const auto context { rangeText.substr (secondColon + signLength) };

    if (jam::simd::findByte (context.data(), context.size(), 0, colonSign) == context.size())
        return Fields { rangeText.substr (0, firstColon), rangeText.substr (firstColon + signLength, secondColon - firstColon - signLength), context };

    return std::nullopt;
}

static std::pair<bool, juce::Range<int>> getSingleRange (std::string_view rangeText, int lineCount)
{
    if (startsWithSign (rangeText, fromEndSign))
        return { false, {} };

    const auto [isValid, lineNumber] { getPlainNumber (rangeText) };
    return { isValid, getClampedRange (lineNumber, lineNumber, lineCount) };
}

static std::pair<bool, juce::Range<int>> getParsedRange (std::string_view rangeText, int lineCount)
{
    const size_t colonCount { jam::simd::countByte (rangeText.data(), rangeText.size(), colonSign) };

    if (colonCount == noColonCount)
        return getSingleRange (rangeText, lineCount);

    const auto fields { getFields (rangeText) };

    if (fields.has_value())
        return getColonRange (*fields, colonCount == twoColonCount, lineCount);

    return { false, {} };
}

static size_t getLineOffset (std::string_view bytes, int lineNumber)
{
    size_t offset { 0 };

    for (int line { firstLine }; line < lineNumber and offset < bytes.size(); ++line)
        offset = std::min (jam::simd::findByte (bytes.data(), bytes.size(), offset, newlineChar) + 1, bytes.size());

    return offset;
}

int PageRanges::getLineCount (std::string_view bytes) noexcept
{
    const bool isPartialLine { not bytes.empty() and bytes.back() != newlineChar };
    const auto newlineCount { jam::simd::countByte (bytes.data(), bytes.size(), newlineChar) };
    return static_cast<int> (newlineCount) + static_cast<int> (isPartialLine);
}

std::pair<juce::Result, juce::Range<int>> PageRanges::getLineRange (std::string_view rangeText, int lineCount)
{
    const auto [isValid, lineRange] { getParsedRange (rangeText, lineCount) };

    if (isValid)
        return { juce::Result::ok(), lineRange };

    return { Stdio::fail (text::Diagnostics::failLineRangeInvalid, rangeText), juce::Range<int> {} };
}

static juce::Range<int> getAllLines (int lineCount)
{
    return juce::Range<int> { firstLine, lineCount + firstLine };
}

std::pair<juce::Result, jam::Array<juce::Range<int>>> PageRanges::getRanges (juce::Span<const std::string_view> rangeTexts, int lineCount)
{
    jam::Array<juce::Range<int>> lineRanges;

    if (rangeTexts.empty())
        lineRanges.add (getAllLines (lineCount));

    for (const auto rangeText : rangeTexts)
    {
        const auto [rangeStatus, lineRange] { getLineRange (rangeText, lineCount) };

        if (rangeStatus.failed())
            return { rangeStatus, jam::Array<juce::Range<int>> {} };

        lineRanges.add (lineRange);
    }

    return { juce::Result::ok(), getUnion (std::move (lineRanges)) };
}

jam::Array<juce::Range<int>> PageRanges::getUnion (jam::Array<juce::Range<int>> lineRanges)
{
    std::sort (lineRanges.begin(), lineRanges.end(),
               [] (const juce::Range<int>& first, const juce::Range<int>& second) { return first.getStart() < second.getStart(); });
    jam::Array<juce::Range<int>> merged;

    for (const auto& lineRange : lineRanges)
    {
        if (not lineRange.isEmpty())
        {
            if (merged.isEmpty() or isGapBetween (merged.last(), lineRange))
                merged.add (lineRange);
            else
                merged.set (merged.size() - 1, merged.last().getUnionWith (lineRange));
        }
    }

    return merged;
}

bool PageRanges::isGapBetween (juce::Range<int> previousRange, juce::Range<int> nextRange) noexcept
{
    return nextRange.getStart() > previousRange.getEnd();
}

std::string_view PageRanges::getRangeBytes (std::string_view bytes, juce::Range<int> lineRange)
{
    const auto startOffset { getLineOffset (bytes, lineRange.getStart()) };
    const auto endOffset { getLineOffset (bytes, lineRange.getEnd()) };
    return bytes.substr (startOffset, endOffset - startOffset);
}

std::string PageRanges::getJoinedBytes (std::string_view bytes, const jam::Array<juce::Range<int>>& lineRanges)
{
    std::string joined;

    for (const auto& lineRange : lineRanges)
        jam::Format::appendBytes (joined, getRangeBytes (bytes, lineRange));

    return joined;
}

int PageRanges::getNumberOfLine (const jam::Array<int>& originalNumbers, int lineIndex)
{
    if (juce::isPositiveAndBelow (lineIndex, originalNumbers.size()))
        return originalNumbers.at (lineIndex);

    return lineIndex + firstLine;
}

jam::Array<int> PageRanges::getRealNumbers (const jam::Array<int>& originalNumbers, int lineCount)
{
    jam::Array<int> numbers;

    for (int lineIndex { 0 }; lineIndex < lineCount; ++lineIndex)
        numbers.add (getNumberOfLine (originalNumbers, lineIndex));

    return numbers;
}

jam::Array<int> PageRanges::getNonblankNumbers (std::string_view text, int lineCount)
{
    static constexpr int noLineNumber { 0 };
    jam::Array<int> numbers;
    int nonblankCount { 0 };

    TextDocument::applyToLines (text, juce::Range<int> { firstLine, lineCount + firstLine },
                                [&numbers, &nonblankCount] (int, std::string_view line)
    {
        if (not line.empty())
            ++nonblankCount;

        numbers.add (line.empty() ? noLineNumber : nonblankCount);
        return true;
    });

    return numbers;
}

jam::Array<int> PageRanges::getShownNumbers (std::string_view text, const jam::Array<int>& originalNumbers, bool isNumberingNonblank)
{
    const int lineCount { PageRanges::getLineCount (text) };

    if (isNumberingNonblank)
        return getNonblankNumbers (text, lineCount);

    return getRealNumbers (originalNumbers, lineCount);
}

int PageRanges::getLineChange (const jam::Array<int>& documentChanges, int lineNumber)
{
    const int changeIndex { lineNumber - firstLine };

    if (juce::isPositiveAndBelow (changeIndex, documentChanges.size()))
        return documentChanges.at (changeIndex);

    return map::LineChange::none;
}

jam::Array<int> PageRanges::getShownChanges (const jam::Array<int>& documentChanges, const jam::Array<int>& originalNumbers, int lineCount)
{
    jam::Array<int> changes;

    if (documentChanges.isEmpty())
        return changes;

    for (int lineIndex { 0 }; lineIndex < lineCount; ++lineIndex)
        changes.add (getLineChange (documentChanges, getNumberOfLine (originalNumbers, lineIndex)));

    return changes;
}

juce::Range<int> PageRanges::getContextRange (int lineNumber, int diffContext, int lineCount)
{
    return juce::Range<int> { std::max (firstLine, lineNumber - diffContext), std::min (lineCount, lineNumber + diffContext) + firstLine };
}

jam::Array<juce::Range<int>> PageRanges::getChangedRanges (const jam::Array<int>& shownChanges, int diffContext, int lineCount)
{
    jam::Array<juce::Range<int>> ranges;
    int lineNumber { firstLine };

    for (const auto change : shownChanges)
    {
        if (change != map::LineChange::none)
            ranges.add (getContextRange (lineNumber, diffContext, lineCount));

        ++lineNumber;
    }

    return getUnion (std::move (ranges));
}
