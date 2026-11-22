#include "LineDiff.h"
#include "generated/Generated.h"

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

static int getMatchingLineRun (const TextDocument& original,
                               int origEnd,
                               const TextDocument& target,
                               int targEnd,
                               int origStart,
                               int targStart)
{
    int count { 0 };

    while (origStart + count < origEnd
           and targStart + count < targEnd
           and original.getLine (origStart + count + 1).compare (target.getLine (targStart + count + 1)) == 0)
        ++count;

    return count;
}

static int getCommonSuffixLength (const TextDocument& original, const TextDocument& target, int prefix)
{
    int suffix { 0 };
    const int origCount { original.getNumLines() };
    const int targCount { target.getNumLines() };

    while (suffix < origCount - prefix
           and suffix < targCount - prefix
           and original.getLine (origCount - suffix).compare (target.getLine (targCount - suffix)) == 0)
        ++suffix;

    return suffix;
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

static std::vector<std::vector<int>> getMyersTrace (const TextDocument& original,
                                                    const TextDocument& target,
                                                    int prefix,
                                                    int origMidLen,
                                                    int targMidLen)
{
    const int maxEditDistance { origMidLen + targMidLen };
    const int offset { maxEditDistance };
    std::vector<int> furthestX (static_cast<size_t> (2 * maxEditDistance + 1), 0);
    std::vector<std::vector<int>> trace;
    trace.reserve (static_cast<size_t> (maxEditDistance + 1));

    for (int distance { 0 }; distance <= maxEditDistance; ++distance)
    {
        trace.push_back (furthestX);

        for (int diagonal { -distance }; diagonal <= distance; diagonal += 2)
        {
            const bool shouldInsert { isMyersInsertStep (furthestX, distance, diagonal, offset) };
            int x { shouldInsert
                        ? furthestX.at (static_cast<size_t> (diagonal + 1 + offset))
                        : furthestX.at (static_cast<size_t> (diagonal - 1 + offset)) + 1 };
            int y { x - diagonal };
            const int snake { getMatchingLineRun (original, prefix + origMidLen, target, prefix + targMidLen, prefix + x, prefix + y) };
            x += snake;
            y += snake;
            furthestX.at (static_cast<size_t> (diagonal + offset)) = x;

            if (x >= origMidLen and y >= targMidLen)
                return trace;
        }
    }

    return trace;
}

static std::vector<std::tuple<bool, int, int>> getMyersOperations (const std::vector<std::vector<int>>& trace,
                                                                   int origMidLen,
                                                                   int targMidLen)
{
    const int offset { origMidLen + targMidLen };
    std::vector<std::tuple<bool, int, int>> operations;
    int x { origMidLen };
    int y { targMidLen };

    for (int distance { static_cast<int> (trace.size()) - 1 }; distance >= 1; --distance)
    {
        const auto& previousX { trace.at (static_cast<size_t> (distance)) };
        const int diagonal { x - y };
        const bool cameFromInsert { isMyersInsertStep (previousX, distance, diagonal, offset) };
        const int previousDiagonal { cameFromInsert ? diagonal + 1 : diagonal - 1 };
        const int prevX { previousX.at (static_cast<size_t> (previousDiagonal + offset)) };
        const int prevY { prevX - previousDiagonal };
        operations.emplace_back (cameFromInsert, prevX, prevY);
        x = prevX;
        y = prevY;
    }

    return operations;
}

static void addCoalescedOperation (jam::Array<LineDiff::Change>& changes,
                                   const TextDocument& target,
                                   int prefix,
                                   bool isInsert,
                                   int origIndex,
                                   int targIndex)
{
    const int origLineIndex { prefix + origIndex };

    if (changes.isEmpty() or origLineIndex != changes.last().start + changes.last().length)
    {
        LineDiff::Change change;
        change.start = origLineIndex;
        changes.add (std::move (change));
    }

    auto& currentChange { changes.last() };

    if (isInsert)
        currentChange.insertedLines.add (target.getLine (prefix + targIndex + 1));
    else
        ++currentChange.length;
}

static void appendOriginalLines (std::vector<std::string_view>& outputLines,
                                 const TextDocument& original,
                                 int startLine,
                                 int endLine)
{
    for (int lineIndex { startLine }; lineIndex < endLine; ++lineIndex)
        outputLines.push_back (original.getLine (lineIndex + 1));
}

static std::vector<std::string_view> getPlannedLineViews (const TextDocument& original,
                                                          const jam::Array<LineDiff::Change>& changes)
{
    for (int index { 1 }; index < changes.size(); ++index)
        assert (changes.at (index - 1).start <= changes.at (index).start);

    std::vector<std::string_view> outputLines;
    int originalLine { 0 };

    for (int changeIndex { 0 }; changeIndex < changes.size(); ++changeIndex)
    {
        const auto& change { changes.at (changeIndex) };
        appendOriginalLines (outputLines, original, originalLine, change.start);

        for (int lineIndex { 0 }; lineIndex < change.insertedLines.size(); ++lineIndex)
            outputLines.push_back (change.insertedLines.getView (lineIndex));

        originalLine = change.start + change.length;
    }

    appendOriginalLines (outputLines, original, originalLine, original.getNumLines());
    return outputLines;
}

static jam::Array<LineDiff::Change> getTrailingNewlineChange (const TextDocument& original)
{
    jam::Array<LineDiff::Change> trailingChanges;
    LineDiff::Change finalLineChange;
    finalLineChange.start = original.getNumLines() - 1;
    finalLineChange.length = 1;
    finalLineChange.insertedLines.add (original.getLine (original.getNumLines()));
    trailingChanges.add (std::move (finalLineChange));
    return trailingChanges;
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

            if (next.start - (previous.start + previous.length) > 2 * LineDiff::contextLineCount)
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
        delta += change.insertedLines.size() - change.length;
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
                                const TextDocument& planned,
                                int originalLine,
                                int plannedLine,
                                int endOriginalLine)
{
    while (originalLine < endOriginalLine)
    {
        const bool missingNewline { (originalLine + 1 == original.getNumLines() and not original.hasTrailingNewline())
                                    or (plannedLine + 1 == planned.getNumLines() and not planned.hasTrailingNewline()) };
        appendDiffLine (output, static_cast<char> (Chars::space), original.getLine (originalLine + 1), missingNewline);
        ++originalLine;
        ++plannedLine;
    }
}

static void appendHunkChange (std::string& output,
                              const TextDocument& original,
                              const TextDocument& planned,
                              const LineDiff::Change& change,
                              int originalLine,
                              int plannedLine)
{
    for (int index { 0 }; index < change.length; ++index)
    {
        const int lineIndex { originalLine + index };
        const bool missingNewline { lineIndex + 1 == original.getNumLines() and not original.hasTrailingNewline() };
        appendDiffLine (output, static_cast<char> (Chars::dash), original.getLine (lineIndex + 1), missingNewline);
    }

    for (int index { 0 }; index < change.insertedLines.size(); ++index)
    {
        const int lineIndex { plannedLine + index };
        const bool missingNewline { lineIndex + 1 == planned.getNumLines() and not planned.hasTrailingNewline() };
        appendDiffLine (output, static_cast<char> (Chars::plus), change.insertedLines.getView (index), missingNewline);
    }
}

static void appendHunkBody (std::string& output,
                            const TextDocument& original,
                            const TextDocument& planned,
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
        appendContextLines (output, original, planned, originalLine, plannedLine, change.start);
        plannedLine += change.start - originalLine;
        originalLine = change.start;
        appendHunkChange (output, original, planned, change, originalLine, plannedLine);
        originalLine += change.length;
        plannedLine += change.insertedLines.size();
    }

    appendContextLines (output, original, planned, originalLine, plannedLine, oldEnd);
}

LineDiff::LineDiff (jam::Array<Change> explicitChanges)
    : changes (std::move (explicitChanges))
{
}

LineDiff::LineDiff (const TextDocument& original, const TextDocument& target)
{
    const int prefix { getMatchingLineRun (original, original.getNumLines(), target, target.getNumLines(), 0, 0) };
    const int suffix { getCommonSuffixLength (original, target, prefix) };
    const int origMidLen { original.getNumLines() - prefix - suffix };
    const int targMidLen { target.getNumLines() - prefix - suffix };

    if (origMidLen > 0 and targMidLen > 0)
    {
        const auto trace { getMyersTrace (original, target, prefix, origMidLen, targMidLen) };
        const auto operations { getMyersOperations (trace, origMidLen, targMidLen) };

        for (int index { static_cast<int> (operations.size()) - 1 }; index >= 0; --index)
        {
            const auto& [isInsert, origIndex, targIndex] { operations.at (static_cast<size_t> (index)) };
            addCoalescedOperation (changes, target, prefix, isInsert, origIndex, targIndex);
        }
    }
    else if (origMidLen > 0 or targMidLen > 0)
    {
        Change change;
        change.start = prefix;
        change.length = origMidLen;

        for (int index { 0 }; index < targMidLen; ++index)
            change.insertedLines.add (target.getLine (prefix + index + 1));

        changes.add (std::move (change));
    }
}

jam::Strings LineDiff::getLines (std::string_view text)
{
    if (text.empty())
        return jam::Strings { juce::String {} };

    const auto remaining { getTextWithoutSingleTrailingNewline (text) };
    jam::Strings lines;
    size_t cursor { 0 };

    while (true)
    {
        const auto newlineIndex { jam::simd::findByte (remaining.data(), remaining.size(), cursor, static_cast<char> (Chars::newline)) };
        lines.add (getLineWithoutCarriageReturn (remaining.substr (cursor, newlineIndex - cursor)));

        if (newlineIndex == remaining.size())
            return lines;

        cursor = newlineIndex + 1;
    }
}

jam::Strings LineDiff::getLinesPreservingEmpty (std::string_view text)
{
    jam::Strings lines;
    size_t cursor { 0 };

    while (true)
    {
        const auto newlineIndex { jam::simd::findByte (text.data(), text.size(), cursor, static_cast<char> (Chars::newline)) };
        auto lineView { text.substr (cursor, newlineIndex - cursor) };

        if (newlineIndex < text.size())
            lineView = getLineWithoutCarriageReturn (lineView);

        lines.add (lineView);

        if (newlineIndex == text.size())
            return lines;

        cursor = newlineIndex + 1;
    }
}

std::string LineDiff::getPlannedText (const TextDocument& original) const
{
    const auto outputLines { getPlannedLineViews (original, changes) };

    if (outputLines.empty())
        return {};

    const auto lineEnding { original.getLineEnding() };
    const bool deletedFinalLine { not changes.isEmpty()
                                  and changes.last().length > 0
                                  and changes.last().insertedLines.size() == 0
                                  and changes.last().start + changes.last().length >= original.getNumLines() };
    const bool shouldEmitTrailingNewline { original.getNumLines() == 0
                                           or original.hasTrailingNewline()
                                           or deletedFinalLine };
    std::string plannedText;

    for (size_t index { 0 }; index < outputLines.size(); ++index)
    {
        jam::Format::appendBytes (plannedText, outputLines.at (index));

        if (index + 1 < outputLines.size() or shouldEmitTrailingNewline)
            jam::Format::appendBytes (plannedText, lineEnding);
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

std::string LineDiff::getUnifiedDiff (const TextDocument& original, const TextDocument& planned) const
{
    if (changes.isEmpty() and (original.hasTrailingNewline() == planned.hasTrailingNewline() or original.getNumLines() == 0))
        return {};

    const auto newlineOnlyChanges (changes.isEmpty() ? getTrailingNewlineChange (original) : jam::Array<Change> {});
    const auto& activeChanges { changes.isEmpty() ? newlineOnlyChanges : changes };
    const auto hunkRanges (getHunkChangeRanges (activeChanges));
    std::string output;
    jam::Format::appendBytes (output, jam::Strings::getView (juce::String::formatted (text::Diagnostics::diffOldHeaderFormat, original.getDisplayPath().toRawUTF8())));
    jam::Format::appendBytes (output, jam::Strings::getView (juce::String::formatted (text::Diagnostics::diffNewHeaderFormat, original.getDisplayPath().toRawUTF8())));
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
        appendHunkBody (output, original, planned, activeChanges, hunkRange, oldStart, oldEnd, newStart);
        cumulativeDelta += hunkDelta;
    }

    return output;
}
