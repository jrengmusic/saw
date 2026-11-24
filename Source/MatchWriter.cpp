#include "MatchWriter.h"
#include "TextDocument.h"
#include "generated/Generated.h"

static size_t getPreviousLineStart (std::string_view bytes, size_t lineStart, size_t minOffset)
{
    assert (lineStart > minOffset);

    size_t previousStart { lineStart - 1 };

    while (previousStart > minOffset and bytes.at (previousStart - 1) != static_cast<char> (Chars::newline))
        --previousStart;

    return previousStart;
}

MatchWriter::MatchWriter (std::string_view newBytes,
                          std::string_view newDisplayPath,
                          int newBeforeContext,
                          int newAfterContext,
                          bool newIsOnlyMatching,
                          bool newIsShowingPathPrefix,
                          bool newIsShowingLineNumber,
                          bool newIsShowingHash,
                          int newFirstLineNumber)
    : bytes (newBytes),
      displayPath (newDisplayPath),
      beforeContext (newBeforeContext),
      afterContext (newAfterContext),
      isOnlyMatching (newIsOnlyMatching),
      isShowingPathPrefix (newIsShowingPathPrefix),
      isShowingLineNumber (newIsShowingLineNumber),
      isShowingHash (newIsShowingHash),
      printedLineNumber (newFirstLineNumber),
      countedLineNumber (newFirstLineNumber)
{
}

void MatchWriter::setLineNumber (size_t lineStart)
{
    assert (lineStart >= countedUpTo);

    countedLineNumber += static_cast<int> (jam::simd::countByte (bytes.data() + countedUpTo, lineStart - countedUpTo, static_cast<char> (Chars::newline)));
    countedUpTo = lineStart;
}

int MatchWriter::getLineNumber() const noexcept
{
    return countedLineNumber;
}

juce::Result MatchWriter::addMatch (const Pattern& pattern, size_t lineStart, size_t newlinePos)
{
    static constexpr size_t initialCapacity { 16 * 1024 };
    output.reserve (std::max (output.capacity(), std::min (bytes.size(), initialCapacity)));
    const std::string_view line { TextDocument::getLine (bytes, lineStart, newlinePos) };

    if (isNumbered())
        setLineNumber (lineStart);

    const int matchLineNumber { isNumbered() ? getLineNumber() : printedLineNumber };

    if (isOnlyMatching)
        return appendOnlyMatches (pattern, matchLineNumber, line);

    if (printedUpTo > 0)
        appendTrailingContext (lineStart);

    const int leadingCount { std::min (beforeContext, matchLineNumber - printedLineNumber) };
    size_t firstLeadingStart { lineStart };

    for (int index { 0 }; index < leadingCount; ++index)
        firstLeadingStart = getPreviousLineStart (bytes, firstLeadingStart, printedUpTo);

    if ((beforeContext != 0 or afterContext != 0) and printedUpTo > 0 and firstLeadingStart > printedUpTo)
    {
        jam::Format::appendBytes (output, Chars::doubleDash);
        output.push_back (static_cast<char> (Chars::newline));
    }

    int leadingLineNumber { matchLineNumber - leadingCount };
    size_t cursor { firstLeadingStart };

    while (cursor < lineStart)
    {
        cursor = appendContextLine (cursor, leadingLineNumber, static_cast<char> (Chars::dash));
        ++leadingLineNumber;
    }

    appendFormattedLine (matchLineNumber, line, static_cast<char> (Chars::colon));
    printedUpTo = newlinePos < bytes.size() ? newlinePos + 1 : bytes.size();
    printedLineNumber = matchLineNumber + 1;
    return juce::Result::ok();
}

std::string MatchWriter::getOutput()
{
    if (printedUpTo > 0)
        appendTrailingContext (bytes.size());

    return std::move (output);
}

size_t MatchWriter::getPrintedSize() const noexcept
{
    return printedUpTo;
}

bool MatchWriter::isNumbered() const noexcept
{
    return isShowingLineNumber or isShowingHash or beforeContext > 0 or afterContext > 0;
}

void MatchWriter::appendLinePrefix (int lineNumber, std::string_view line, char separator)
{
    if (isShowingPathPrefix)
    {
        jam::Format::appendBytes (output, displayPath);
        output.push_back (separator);
    }

    if (isShowingHash)
    {
        const auto lineHash { TextDocument::getLineHash (line) };
        jam::Format::appendDecimal (output, lineNumber);
        output.push_back (separator);
        jam::Format::appendBytes (output, jam::Strings::getView (lineHash));
        output.push_back (static_cast<char> (Chars::pipe));
    }
    else if (isShowingLineNumber)
    {
        jam::Format::appendDecimal (output, lineNumber);
        output.push_back (separator);
    }
}

void MatchWriter::appendFormattedLine (int lineNumber, std::string_view line, char separator)
{
    appendLinePrefix (lineNumber, line, separator);
    jam::Format::appendBytes (output, line);
    output.push_back (static_cast<char> (Chars::newline));
}

juce::Result MatchWriter::appendOnlyMatches (const Pattern& pattern, int lineNumber, std::string_view line)
{
    size_t offset { 0 };

    while (offset <= line.size())
    {
        const auto [findResult, captures] { pattern.findNext (line, offset) };

        if (findResult.failed())
            return findResult;

        if (captures.size() == 0)
            return juce::Result::ok();

        const std::string_view matchText { *captures.at (0) };
        const size_t matchEnd { static_cast<size_t> (matchText.data() - line.data()) + matchText.size() };

        if (not matchText.empty())
        {
            appendLinePrefix (lineNumber, line, static_cast<char> (Chars::colon));
            jam::Format::appendBytes (output, matchText);
            output.push_back (static_cast<char> (Chars::newline));
        }

        offset = matchText.empty() ? jam::RegExp::getNextPosition (line, matchEnd, true) : matchEnd;
    }

    return juce::Result::ok();
}

size_t MatchWriter::appendContextLine (size_t lineStart, int lineNumber, char separator)
{
    const size_t newlinePos { jam::simd::findByte (bytes.data(), bytes.size(), lineStart, static_cast<char> (Chars::newline)) };
    appendFormattedLine (lineNumber, TextDocument::getLine (bytes, lineStart, newlinePos), separator);
    return newlinePos < bytes.size() ? newlinePos + 1 : bytes.size();
}

void MatchWriter::appendTrailingContext (size_t limit)
{
    for (int index { 0 }; index < afterContext and printedUpTo < limit; ++index)
    {
        printedUpTo = appendContextLine (printedUpTo, printedLineNumber, static_cast<char> (Chars::dash));
        ++printedLineNumber;
    }
}
