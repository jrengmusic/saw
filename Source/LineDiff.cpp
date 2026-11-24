#include "LineDiff.h"
#include "Stdio.h"
#include "generated/Generated.h"

static constexpr int contextLineCount { 3 };

static std::string_view getLineWithoutCarriageReturn (std::string_view line)
{
    if (not line.empty() and line.at (line.size() - 1) == static_cast<char> (Chars::carriageReturn))
        line.remove_suffix (1);

    return line;
}

static std::string_view getTextWithoutSingleTrailingNewline (std::string_view text)
{
    if (not text.empty() and text.at (text.size() - 1) == static_cast<char> (Chars::newline))
    {
        text.remove_suffix (1);
        return getLineWithoutCarriageReturn (text);
    }

    return text;
}

static bool isIncompleteLine (const TextDocument& document, int lineNumber)
{
    return lineNumber == document.getNumLines() and not document.hasTrailingNewline();
}

static bool isLineEqual (const TextDocument& original, int originalLine, const TextDocument& target, int targetLine)
{
    return original.getLine (originalLine).compare (target.getLine (targetLine)) == 0
           and isIncompleteLine (original, originalLine) == isIncompleteLine (target, targetLine);
}

static int getMatchingPrefixLength (const TextDocument& original,
                                    juce::Range<int> originalRange,
                                    const TextDocument& target,
                                    juce::Range<int> targetRange)
{
    const int maxLength { std::min (originalRange.getLength(), targetRange.getLength()) };
    int length { 0 };

    while (length < maxLength
           and isLineEqual (original, originalRange.getStart() + length + 1, target, targetRange.getStart() + length + 1))
        ++length;

    return length;
}

static int getMatchingSuffixLength (const TextDocument& original,
                                    juce::Range<int> originalRange,
                                    const TextDocument& target,
                                    juce::Range<int> targetRange)
{
    const int maxLength { std::min (originalRange.getLength(), targetRange.getLength()) };
    int length { 0 };

    while (length < maxLength
           and isLineEqual (original, originalRange.getEnd() - length, target, targetRange.getEnd() - length))
        ++length;

    return length;
}

static bool isMyersInsertStep (const std::vector<int>& furthestX,
                               int distance,
                               int diagonal,
                               int offset)
{
    if (diagonal == -distance)
        return true;

    if (diagonal == distance)
        return false;

    return furthestX.at (static_cast<size_t> (diagonal - 1 + offset))
           < furthestX.at (static_cast<size_t> (diagonal + 1 + offset));
}

static std::pair<int, int> getMiddlePoint (const TextDocument& original,
                                           juce::Range<int> originalRange,
                                           const TextDocument& target,
                                           juce::Range<int> targetRange)
{
    const int originalCount { originalRange.getLength() };
    const int targetCount { targetRange.getLength() };
    const int delta { originalCount - targetCount };
    const bool isOdd { (originalCount + targetCount) % 2 != 0 };
    const int maxDistance { (originalCount + targetCount + 1) / 2 };
    const int offset { maxDistance + 1 };
    std::vector<int> forwardX (static_cast<size_t> (2 * maxDistance + 3), 0);
    std::vector<int> backwardX (static_cast<size_t> (2 * maxDistance + 3), 0);

    for (int distance { 0 }; distance <= maxDistance; ++distance)
    {
        const int lowest { -(distance - 2 * std::max (0, distance - targetCount)) };
        const int highest { distance - 2 * std::max (0, distance - originalCount) };

        for (int diagonal { lowest }; diagonal <= highest; diagonal += 2)
        {
            int x { isMyersInsertStep (forwardX, distance, diagonal, offset)
                        ? forwardX.at (static_cast<size_t> (diagonal + 1 + offset))
                        : forwardX.at (static_cast<size_t> (diagonal - 1 + offset)) + 1 };
            int y { x - diagonal };
            const int snake { getMatchingPrefixLength (original,
                                                       juce::Range<int> (originalRange.getStart() + x, originalRange.getEnd()),
                                                       target,
                                                       juce::Range<int> (targetRange.getStart() + y, targetRange.getEnd())) };
            x += snake;
            y += snake;
            forwardX.at (static_cast<size_t> (diagonal + offset)) = x;
            const int opposite { delta - diagonal };

            if (isOdd and opposite >= -(distance - 1) and opposite <= distance - 1
                and x + backwardX.at (static_cast<size_t> (opposite + offset)) >= originalCount)
                return { originalRange.getStart() + x, targetRange.getStart() + y };
        }

        for (int diagonal { lowest }; diagonal <= highest; diagonal += 2)
        {
            int x { isMyersInsertStep (backwardX, distance, diagonal, offset)
                        ? backwardX.at (static_cast<size_t> (diagonal + 1 + offset))
                        : backwardX.at (static_cast<size_t> (diagonal - 1 + offset)) + 1 };
            int y { x - diagonal };
            const int snake { getMatchingSuffixLength (original,
                                                       juce::Range<int> (originalRange.getStart(), originalRange.getEnd() - x),
                                                       target,
                                                       juce::Range<int> (targetRange.getStart(), targetRange.getEnd() - y)) };
            x += snake;
            y += snake;
            backwardX.at (static_cast<size_t> (diagonal + offset)) = x;
            const int opposite { delta - diagonal };

            if (not isOdd and opposite >= -distance and opposite <= distance
                and x + forwardX.at (static_cast<size_t> (opposite + offset)) >= originalCount)
                return { originalRange.getEnd() - x, targetRange.getEnd() - y };
        }
    }

    assert (false);
    return { originalRange.getEnd(), targetRange.getEnd() };
}

static void addCoalescedOperation (jam::Array<LineDiff::Change>& changes,
                                   const TextDocument& target,
                                   bool isInsert,
                                   int originalIndex,
                                   int targetIndex)
{
    if (changes.isEmpty() or originalIndex != changes.last().start + changes.last().length)
    {
        LineDiff::Change change;
        change.start = originalIndex;
        changes.add (std::move (change));
    }

    auto& currentChange { changes.last() };

    if (isInsert)
        currentChange.insertedLines.emplace_back (target.getLine (targetIndex + 1));
    else
        ++currentChange.length;
}

static void addRangeChanges (jam::Array<LineDiff::Change>& changes,
                             const TextDocument& original,
                             juce::Range<int> originalRange,
                             const TextDocument& target,
                             juce::Range<int> targetRange)
{
    const int prefix { getMatchingPrefixLength (original, originalRange, target, targetRange) };
    const juce::Range<int> originalTail (originalRange.getStart() + prefix, originalRange.getEnd());
    const juce::Range<int> targetTail (targetRange.getStart() + prefix, targetRange.getEnd());
    const int suffix { getMatchingSuffixLength (original, originalTail, target, targetTail) };
    const juce::Range<int> originalMiddle (originalTail.getStart(), originalTail.getEnd() - suffix);
    const juce::Range<int> targetMiddle (targetTail.getStart(), targetTail.getEnd() - suffix);

    if (originalMiddle.isEmpty() or targetMiddle.isEmpty())
    {
        for (int targetIndex { targetMiddle.getStart() }; targetIndex < targetMiddle.getEnd(); ++targetIndex)
            addCoalescedOperation (changes, target, true, originalMiddle.getStart(), targetIndex);

        for (int originalIndex { originalMiddle.getStart() }; originalIndex < originalMiddle.getEnd(); ++originalIndex)
            addCoalescedOperation (changes, target, false, originalIndex, targetMiddle.getStart());
    }
    else
    {
        const auto [middleOriginal, middleTarget] { getMiddlePoint (original, originalMiddle, target, targetMiddle) };
        addRangeChanges (changes, original,
                         juce::Range<int> (originalMiddle.getStart(), middleOriginal),
                         target,
                         juce::Range<int> (targetMiddle.getStart(), middleTarget));
        addRangeChanges (changes, original,
                         juce::Range<int> (middleOriginal, originalMiddle.getEnd()),
                         target,
                         juce::Range<int> (middleTarget, targetMiddle.getEnd()));
    }
}

static std::string_view getOriginalEnding (const TextDocument& original, int lineNumber)
{
    return original.getLineBytes (juce::Range<int> (lineNumber, lineNumber + 1)).substr (original.getLine (lineNumber).size());
}

template <typename Function>
static void appendOriginalRun (const TextDocument& original,
                               int startLine,
                               int endLine,
                               const Function& appendWithEnding)
{
    if (startLine < endLine)
    {
        const auto runBytes { original.getLineBytes (juce::Range<int> (startLine + 1, endLine + 1)) };
        const auto ending { getOriginalEnding (original, endLine) };
        appendWithEnding (runBytes.substr (0, runBytes.size() - ending.size()), ending);
    }
}

static jam::Array<juce::Range<int>> getHunkChangeRanges (const jam::Array<LineDiff::Change>& changes)
{
    jam::Array<juce::Range<int>> hunkRanges;
    int startIndex { 0 };

    while (startIndex < changes.size())
    {
        int endIndex { startIndex + 1 };

        while (endIndex < changes.size())
        {
            const auto& previous { changes.at (endIndex - 1) };
            const auto& next { changes.at (endIndex) };

            if (next.start - (previous.start + previous.length) > 2 * contextLineCount)
                break;

            ++endIndex;
        }

        hunkRanges.add (juce::Range<int> (startIndex, endIndex));
        startIndex = endIndex;
    }

    return hunkRanges;
}

static int getHunkDelta (const jam::Array<LineDiff::Change>& changes, juce::Range<int> hunkRange)
{
    int delta { 0 };

    for (int index { hunkRange.getStart() }; index < hunkRange.getEnd(); ++index)
    {
        const auto& change { changes.at (index) };
        delta += static_cast<int> (change.insertedLines.size()) - change.length;
    }

    return delta;
}

static void appendDiffLine (std::string& output,
                            char prefixChar,
                            std::string_view lineText,
                            bool isMissingTrailingNewline)
{
    output.push_back (prefixChar);
    jam::Format::appendBytes (output, lineText);
    output.push_back (static_cast<char> (Chars::newline));

    if (isMissingTrailingNewline)
        jam::Format::appendBytes (output, text::Diagnostics::diffNoNewline);
}

static void appendContextLines (std::string& output,
                                const TextDocument& original,
                                int originalLine,
                                int endOriginalLine)
{
    while (originalLine < endOriginalLine)
    {
        appendDiffLine (output, static_cast<char> (Chars::space), original.getLine (originalLine + 1), isIncompleteLine (original, originalLine + 1));
        ++originalLine;
    }
}

static int getIncompleteLineNumber (std::string_view bytes)
{
    static constexpr char newline { static_cast<char> (Chars::newline) };

    if (bytes.empty() or bytes.back() == newline)
        return 0;

    return static_cast<int> (jam::simd::countByte (bytes.data(), bytes.size(), newline)) + 1;
}

static void appendHunkChange (std::string& output,
                              const TextDocument& original,
                              int incompletePlannedLine,
                              const LineDiff::Change& change,
                              int originalLine,
                              int plannedLine)
{
    for (int index { 0 }; index < change.length; ++index)
    {
        const int lineIndex { originalLine + index };
        appendDiffLine (output, static_cast<char> (Chars::dash), original.getLine (lineIndex + 1), isIncompleteLine (original, lineIndex + 1));
    }

    for (int index { 0 }; index < static_cast<int> (change.insertedLines.size()); ++index)
    {
        const int lineIndex { plannedLine + index };
        appendDiffLine (output, static_cast<char> (Chars::plus), change.insertedLines.at (static_cast<size_t> (index)), lineIndex + 1 == incompletePlannedLine);
    }
}

static void appendHunkBody (std::string& output,
                            const TextDocument& original,
                            int incompletePlannedLine,
                            const jam::Array<LineDiff::Change>& changes,
                            juce::Range<int> hunkRange,
                            int oldStart,
                            int oldEnd,
                            int newStart)
{
    int originalLine { oldStart };
    int plannedLine { newStart };

    for (int index { hunkRange.getStart() }; index < hunkRange.getEnd(); ++index)
    {
        const auto& change { changes.at (index) };
        appendContextLines (output, original, originalLine, change.start);
        plannedLine += change.start - originalLine;
        originalLine = change.start;
        appendHunkChange (output, original, incompletePlannedLine, change, originalLine, plannedLine);
        originalLine += change.length;
        plannedLine += static_cast<int> (change.insertedLines.size());
    }

    appendContextLines (output, original, originalLine, oldEnd);
}

LineDiff::LineDiff (jam::Array<Change> explicitChanges)
    : changes (std::move (explicitChanges))
{
}

LineDiff::LineDiff (const TextDocument& original, const TextDocument& target)
{
    addRangeChanges (changes, original, juce::Range<int> (0, original.getNumLines()), target, juce::Range<int> (0, target.getNumLines()));
}

std::vector<std::string> LineDiff::getLines (std::string_view text)
{
    if (text.empty())
        return std::vector<std::string> { std::string {} };

    const auto remaining { getTextWithoutSingleTrailingNewline (text) };
    std::vector<std::string> lines;
    size_t cursor { 0 };

    while (true)
    {
        const auto newlineIndex { jam::simd::findByte (remaining.data(), remaining.size(), cursor, static_cast<char> (Chars::newline)) };
        lines.emplace_back (getLineWithoutCarriageReturn (remaining.substr (cursor, newlineIndex - cursor)));

        if (newlineIndex == remaining.size())
            return lines;

        cursor = newlineIndex + 1;
    }
}

std::vector<std::string> LineDiff::getLinesPreservingEmpty (std::string_view text)
{
    std::vector<std::string> lines;
    size_t cursor { 0 };

    while (true)
    {
        const auto newlineIndex { jam::simd::findByte (text.data(), text.size(), cursor, static_cast<char> (Chars::newline)) };
        auto lineView { text.substr (cursor, newlineIndex - cursor) };

        if (newlineIndex < text.size())
            lineView = getLineWithoutCarriageReturn (lineView);

        lines.emplace_back (lineView);

        if (newlineIndex == text.size())
            return lines;

        cursor = newlineIndex + 1;
    }
}

std::string LineDiff::getPlannedText (const TextDocument& original) const
{
    for (int index { 1 }; index < changes.size(); ++index)
        assert (changes.at (index - 1).start <= changes.at (index).start);

    const auto dominantEnding { original.getLineEnding() };
    const bool deletedFinalLine { not changes.isEmpty()
                                  and changes.last().length > 0
                                  and changes.last().insertedLines.empty()
                                  and changes.last().start + changes.last().length >= original.getNumLines() };
    const bool shouldEmitTrailingNewline { original.getNumLines() == 0
                                           or original.hasTrailingNewline()
                                           or deletedFinalLine };
    std::string plannedText;
    std::optional<std::string_view> lastEnding;
    const auto appendWithEnding = [&plannedText, &lastEnding, dominantEnding] (std::string_view bytes, std::string_view ending)
    {
        if (lastEnding.has_value() and lastEnding->empty())
            jam::Format::appendBytes (plannedText, dominantEnding);

        jam::Format::appendBytes (plannedText, bytes);
        jam::Format::appendBytes (plannedText, ending);
        lastEnding = ending;
    };
    int originalLine { 0 };

    for (int changeIndex { 0 }; changeIndex < changes.size(); ++changeIndex)
    {
        const auto& change { changes.at (changeIndex) };
        appendOriginalRun (original, originalLine, change.start, appendWithEnding);

        for (int index { 0 }; index < static_cast<int> (change.insertedLines.size()); ++index)
            appendWithEnding (change.insertedLines.at (static_cast<size_t> (index)),
                              index < change.length ? getOriginalEnding (original, change.start + index + 1) : dominantEnding);

        originalLine = change.start + change.length;
    }

    appendOriginalRun (original, originalLine, original.getNumLines(), appendWithEnding);

    if (lastEnding.has_value())
    {
        if (shouldEmitTrailingNewline and lastEnding->empty())
            jam::Format::appendBytes (plannedText, dominantEnding);

        if (not shouldEmitTrailingNewline)
            plannedText.resize (plannedText.size() - lastEnding->size());
    }

    return plannedText;
}

std::string LineDiff::getHunkRangeText (int zeroBasedStart, int lineCount)
{
    assert (lineCount >= 0);
    std::string text;

    if (lineCount == 0)
    {
        jam::Format::appendDecimal (text, zeroBasedStart);
        jam::Format::appendBytes (text, ",0");
        return text;
    }

    jam::Format::appendDecimal (text, zeroBasedStart + 1);

    if (lineCount > 1)
    {
        text.push_back (static_cast<char> (Chars::comma));
        jam::Format::appendDecimal (text, lineCount);
    }

    return text;
}

jam::Array<LineDiff::Change> LineDiff::getDiffChanges (const TextDocument& original) const
{
    jam::Array<Change> diffChanges;

    for (int index { 0 }; index < changes.size(); ++index)
        diffChanges.add (changes.at (index));

    const int numLines { original.getNumLines() };
    const int lastIndex { diffChanges.size() - 1 };
    const bool isLastLineChanged { lastIndex > 0
                                   and diffChanges.at (lastIndex - 1).length > 0
                                   and diffChanges.at (lastIndex - 1).start + diffChanges.at (lastIndex - 1).length == numLines };

    if (not isLastLineChanged)
    {
        auto& lastChange { diffChanges.last() };
        std::vector<std::string> completedLines;
        completedLines.emplace_back (original.getLine (numLines));
        completedLines.insert (completedLines.end(), lastChange.insertedLines.begin(), lastChange.insertedLines.end());
        lastChange.start = numLines - 1;
        lastChange.length = 1;
        lastChange.insertedLines = std::move (completedLines);
    }

    return diffChanges;
}

std::string LineDiff::getUnifiedDiff (const TextDocument& original, std::string_view plannedBytes) const
{
    if (changes.isEmpty())
        return {};

    const bool isCompletingLastLine { changes.last().start == original.getNumLines()
                                      and changes.last().length == 0
                                      and not changes.last().insertedLines.empty()
                                      and original.getNumLines() > 0
                                      and isIncompleteLine (original, original.getNumLines()) };
    const auto completingChanges (isCompletingLastLine ? getDiffChanges (original) : jam::Array<Change> {});
    const auto& activeChanges { isCompletingLastLine ? completingChanges : changes };
    const int incompletePlannedLine { getIncompleteLineNumber (plannedBytes) };
    const auto hunkRanges (getHunkChangeRanges (activeChanges));
    std::string output;
    jam::Format::appendBytes (output, jam::Strings::getView (Stdio::getFormatted (text::Diagnostics::diffOldHeaderFormat, original.getDisplayPath().toRawUTF8())));
    jam::Format::appendBytes (output, jam::Strings::getView (Stdio::getFormatted (text::Diagnostics::diffNewHeaderFormat, original.getDisplayPath().toRawUTF8())));
    int cumulativeDelta { 0 };

    for (int hunkIndex { 0 }; hunkIndex < hunkRanges.size(); ++hunkIndex)
    {
        const auto hunkRange { hunkRanges.at (hunkIndex) };
        const int oldStart { std::max (0, activeChanges.at (hunkRange.getStart()).start - contextLineCount) };
        const auto& lastChange { activeChanges.at (hunkRange.getEnd() - 1) };
        const int oldEnd { std::min (original.getNumLines(), lastChange.start + lastChange.length + contextLineCount) };
        const int hunkDelta { getHunkDelta (activeChanges, hunkRange) };
        const int newStart { oldStart + cumulativeDelta };
        jam::Format::appendBytes (output, "@@ -" + getHunkRangeText (oldStart, oldEnd - oldStart) + " +" + getHunkRangeText (newStart, (oldEnd - oldStart) + hunkDelta) + " @@\n");
        appendHunkBody (output, original, incompletePlannedLine, activeChanges, hunkRange, oldStart, oldEnd, newStart);
        cumulativeDelta += hunkDelta;
    }

    return output;
}
