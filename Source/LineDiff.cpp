#include "LineDiff.h"
#include "generated/Generated.h"

static constexpr int contextLineCount { 3 };
static constexpr int firstIndex { 0 };

static std::string_view getLineWithoutCarriageReturn (std::string_view line)
{
    if (not line.empty() and line.at (line.size() - 1) == static_cast<char> (Chars::carriageReturn))
        line.remove_suffix (1);

    return line;
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

static bool isMyersInsertStep (const jam::Array<int>& furthestX,
                               int distance,
                               int diagonal,
                               int offset)
{
    if (diagonal == -distance)
        return true;

    if (diagonal == distance)
        return false;

    return furthestX.at (diagonal - 1 + offset)
           < furthestX.at (diagonal + 1 + offset);
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
    jam::Array<int> forwardX;
    jam::Array<int> backwardX;
    forwardX.insertMultiple (0, 0, 2 * maxDistance + 3);
    backwardX.insertMultiple (0, 0, 2 * maxDistance + 3);

    for (int distance { 0 }; distance <= maxDistance; ++distance)
    {
        const int lowest { -(distance - 2 * std::max (0, distance - targetCount)) };
        const int highest { distance - 2 * std::max (0, distance - originalCount) };

        for (int diagonal { lowest }; diagonal <= highest; diagonal += 2)
        {
            int x { isMyersInsertStep (forwardX, distance, diagonal, offset)
                        ? forwardX.at (diagonal + 1 + offset)
                        : forwardX.at (diagonal - 1 + offset) + 1 };
            int y { x - diagonal };
            const int snake { getMatchingPrefixLength (original,
                                                       juce::Range<int> (originalRange.getStart() + x, originalRange.getEnd()),
                                                       target,
                                                       juce::Range<int> (targetRange.getStart() + y, targetRange.getEnd())) };
            x += snake;
            y += snake;
            forwardX.at (diagonal + offset) = x;
            const int opposite { delta - diagonal };

            if (isOdd and opposite >= -(distance - 1) and opposite <= distance - 1
                and x + backwardX.at (opposite + offset) >= originalCount)
                return { originalRange.getStart() + x, targetRange.getStart() + y };
        }

        for (int diagonal { lowest }; diagonal <= highest; diagonal += 2)
        {
            int x { isMyersInsertStep (backwardX, distance, diagonal, offset)
                        ? backwardX.at (diagonal + 1 + offset)
                        : backwardX.at (diagonal - 1 + offset) + 1 };
            int y { x - diagonal };
            const int snake { getMatchingSuffixLength (original,
                                                       juce::Range<int> (originalRange.getStart(), originalRange.getEnd() - x),
                                                       target,
                                                       juce::Range<int> (targetRange.getStart(), targetRange.getEnd() - y)) };
            x += snake;
            y += snake;
            backwardX.at (diagonal + offset) = x;
            const int opposite { delta - diagonal };

            if (not isOdd and opposite >= -distance and opposite <= distance
                and x + forwardX.at (opposite + offset) >= originalCount)
                return { originalRange.getEnd() - x, targetRange.getEnd() - y };
        }
    }

    assert (false);
    return { originalRange.getEnd(), targetRange.getEnd() };
}

static void addCoalescedOperation (jam::Array<LineDiff::Change>& changes,
                                   jam::Array<std::pair<size_t, size_t>>& insertedLines,
                                   const TextDocument& target,
                                   bool isInsert,
                                   int originalIndex,
                                   int targetIndex)
{
    if (changes.isEmpty() or originalIndex != changes.last().start + changes.last().length)
    {
        LineDiff::Change change;
        change.start = originalIndex;
        change.insertedRange = juce::Range<int> (insertedLines.size(), insertedLines.size());
        changes.add (std::move (change));
    }

    auto& currentChange { changes.last() };

    if (isInsert)
    {
        const auto targetLine { target.getLine (targetIndex + 1) };
        const size_t offset { static_cast<size_t> (targetLine.data() - target.getBytes().data()) };
        insertedLines.add (std::pair<size_t, size_t> { offset, targetLine.size() });
        currentChange.insertedRange.setEnd (insertedLines.size());
    }
    else
        ++currentChange.length;
}

static void addRangeChanges (jam::Array<LineDiff::Change>& changes,
                             jam::Array<std::pair<size_t, size_t>>& insertedLines,
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
            addCoalescedOperation (changes, insertedLines, target, true, originalMiddle.getStart(), targetIndex);

        for (int originalIndex { originalMiddle.getStart() }; originalIndex < originalMiddle.getEnd(); ++originalIndex)
            addCoalescedOperation (changes, insertedLines, target, false, originalIndex, targetMiddle.getStart());
    }
    else
    {
        const auto [middleOriginal, middleTarget] { getMiddlePoint (original, originalMiddle, target, targetMiddle) };
        addRangeChanges (changes, insertedLines, original,
                         juce::Range<int> (originalMiddle.getStart(), middleOriginal),
                         target,
                         juce::Range<int> (targetMiddle.getStart(), middleTarget));
        addRangeChanges (changes, insertedLines, original,
                         juce::Range<int> (middleOriginal, originalMiddle.getEnd()),
                         target,
                         juce::Range<int> (middleTarget, targetMiddle.getEnd()));
    }
}

static std::string_view getOriginalEnding (const TextDocument& original, int lineNumber)
{
    return original.getLineBytes (juce::Range<int> (lineNumber, lineNumber + 1)).substr (original.getLine (lineNumber).size());
}

static int getHunkDelta (const jam::Array<LineDiff::Change>& changes, juce::Range<int> hunkRange)
{
    int delta { 0 };

    for (int index { hunkRange.getStart() }; index < hunkRange.getEnd(); ++index)
    {
        const auto& change { changes.at (index) };
        delta += change.insertedRange.getLength() - change.length;
    }

    return delta;
}

static void appendDiffLine (std::string& output,
                            juce::juce_wchar prefixChar,
                            std::string_view lineText,
                            bool isMissingTrailingNewline)
{
    jam::Format::appendUtf8 (output, prefixChar);
    jam::Format::appendBytes (output, lineText);
    jam::Format::appendUtf8 (output, Chars::newline);

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
        appendDiffLine (output, Chars::space, original.getLine (originalLine + 1), isIncompleteLine (original, originalLine + 1));
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

std::string_view LineDiff::getTextWithoutSingleTrailingNewline (std::string_view text)
{
    if (not text.empty() and text.at (text.size() - 1) == static_cast<char> (Chars::newline))
        text = getLineWithoutCarriageReturn (text.substr (0, text.size() - 1));

    return getLineWithoutCarriageReturn (text);
}

std::tuple<int, int, std::optional<std::string_view>> LineDiff::getDeletion (juce::Range<int> lineRange)
{
    return { lineRange.getStart() - 1, lineRange.getLength(), std::nullopt };
}

std::tuple<int, int, std::optional<std::string_view>> LineDiff::getReplacement (juce::Range<int> lineRange, std::string_view text)
{
    return { lineRange.getStart() - 1, lineRange.getLength(), text };
}

std::tuple<int, int, std::optional<std::string_view>> LineDiff::getInsertion (int start, std::string_view text)
{
    return { start, 0, text };
}

std::optional<std::string_view> LineDiff::appendChange (const TextDocument& original,
                                                        int start,
                                                        int length,
                                                        std::optional<std::string_view> insertedText,
                                                        std::optional<std::string_view> lastEnding,
                                                        bool isVerbatim)
{
    assert (changes.isEmpty() or changes.last().start <= start);
    const int nextOriginalLine { changes.isEmpty() ? 0 : changes.last().start + changes.last().length };
    lastEnding = appendOriginalRun (original, nextOriginalLine, start, lastEnding);

    Change change;
    change.start = start;
    change.length = length;
    change.insertedRange = juce::Range<int> (insertedLines.size(), insertedLines.size());

    if (insertedText.has_value())
    {
        const std::string_view text { *insertedText };
        lastEnding = isVerbatim ? appendVerbatimText (original, text, lastEnding)
                                : appendInsertedText (original, start, length, text, lastEnding);
        change.insertedRange.setEnd (insertedLines.size());
    }

    changes.add (std::move (change));
    return lastEnding;
}

std::optional<std::string_view> LineDiff::appendInsertedText (const TextDocument& original,
                                                              int start,
                                                              int length,
                                                              std::string_view text,
                                                              std::optional<std::string_view> lastEnding)
{
    size_t cursor { 0 };
    int pieceIndex { 0 };

    while (true)
    {
        const size_t newlineIndex { jam::simd::findByte (text.data(), text.size(), cursor, static_cast<char> (Chars::newline)) };
        auto lineView { text.substr (cursor, newlineIndex - cursor) };

        if (newlineIndex < text.size())
            lineView = getLineWithoutCarriageReturn (lineView);

        const auto ending { pieceIndex < length ? getOriginalEnding (original, start + pieceIndex + 1) : original.getLineEnding() };
        appendWithEnding (original, lineView, ending, lastEnding);
        lastEnding = ending;
        insertedLines.add (std::pair<size_t, size_t> { plannedText.size() - ending.size() - lineView.size(), lineView.size() });
        ++pieceIndex;

        if (newlineIndex == text.size())
            break;

        cursor = newlineIndex + 1;
    }

    return lastEnding;
}

std::optional<std::string_view> LineDiff::appendVerbatimText (const TextDocument& original,
                                                              std::string_view text,
                                                              std::optional<std::string_view> lastEnding)
{
    size_t cursor { 0 };

    while (cursor < text.size())
    {
        const size_t newlinePos { jam::simd::findByte (text.data(), text.size(), cursor, static_cast<char> (Chars::newline)) };
        const std::string_view lineView { TextDocument::getLine (text, cursor, newlinePos) };
        const std::string_view ending { TextDocument::getLineEnding (text, lineView) };
        appendWithEnding (original, lineView, ending, lastEnding);
        lastEnding = ending;
        insertedLines.add (std::pair<size_t, size_t> { plannedText.size() - ending.size() - lineView.size(), lineView.size() });
        cursor = newlinePos < text.size() ? newlinePos + 1 : text.size();
    }

    return lastEnding;
}

std::optional<std::string_view> LineDiff::appendOriginalRun (const TextDocument& original,
                                                             int startLine,
                                                             int endLine,
                                                             std::optional<std::string_view> lastEnding)
{
    if (startLine < endLine)
    {
        const auto runBytes { original.getLineBytes (juce::Range<int> (startLine + 1, endLine + 1)) };
        const auto ending { getOriginalEnding (original, endLine) };
        appendWithEnding (original, runBytes.substr (0, runBytes.size() - ending.size()), ending, lastEnding);
        return ending;
    }

    return lastEnding;
}

void LineDiff::appendWithEnding (const TextDocument& original,
                                 std::string_view bytes,
                                 std::string_view ending,
                                 std::optional<std::string_view> lastEnding)
{
    if (lastEnding.has_value() and lastEnding->empty())
        jam::Format::appendBytes (plannedText, original.getLineEnding());

    jam::Format::appendBytes (plannedText, bytes);
    jam::Format::appendBytes (plannedText, ending);
}

void LineDiff::setPlannedEnd (const TextDocument& original, std::optional<std::string_view> lastEnding, bool isVerbatim)
{
    if (not changes.isEmpty())
    {
        const int numLines { original.getNumLines() };
        lastEnding = appendOriginalRun (original, changes.last().start + changes.last().length, numLines, lastEnding);

        const bool deletedFinalLine { changes.last().length > 0
                                      and changes.last().insertedRange.isEmpty()
                                      and changes.last().start + changes.last().length >= numLines };
        const bool isNewlineAppended { numLines == 0 or original.hasTrailingNewline() or deletedFinalLine };

        if (lastEnding.has_value() and not isVerbatim)
        {
            if (isNewlineAppended and lastEnding->empty())
                jam::Format::appendBytes (plannedText, original.getLineEnding());

            if (not isNewlineAppended)
                plannedText.resize (plannedText.size() - lastEnding->size());
        }

        const bool isLastLineChanged { changes.size() > 1
                                       and changes.at (changes.size() - 2).length > 0
                                       and changes.at (changes.size() - 2).start + changes.at (changes.size() - 2).length == numLines };
        const bool isCompletingLastLine { numLines > 0
                                          and not original.hasTrailingNewline()
                                          and changes.last().start == numLines
                                          and changes.last().length == 0
                                          and not changes.last().insertedRange.isEmpty() };

        if (isCompletingLastLine and not isLastLineChanged)
        {
            const size_t lastLineSize { original.getLine (numLines).size() };
            const auto& [firstOffset, firstSize] { insertedLines.at (changes.last().insertedRange.getStart()) };
            juce::ignoreUnused (firstSize);
            const std::pair<size_t, size_t> originalLastLine { firstOffset - original.getLineEnding().size() - lastLineSize, lastLineSize };
            auto& lastChange { changes.last() };
            insertedLines.insert (lastChange.insertedRange.getStart(), originalLastLine);
            lastChange.start = numLines - 1;
            lastChange.length = 1;
            lastChange.insertedRange.setEnd (insertedLines.size());
        }
    }
}

LineDiff::LineDiff (const TextDocument& original, const TextDocument& target)
{
    addRangeChanges (changes, insertedLines, original, juce::Range<int> (0, original.getNumLines()), target, juce::Range<int> (0, target.getNumLines()));
}

std::string_view LineDiff::getPlannedBytes() const noexcept
{
    return plannedText;
}

void LineDiff::setRemovedMark (jam::Array<int>& lineChanges, int plannedBefore)
{
    if (plannedBefore == firstIndex)
    {
        if (not lineChanges.isEmpty())
            lineChanges.set (firstIndex, map::LineChange::removedAbove);
    }
    else
    {
        lineChanges.set (plannedBefore - 1, map::LineChange::removedBelow);
    }
}

void LineDiff::setInsertedMarks (jam::Array<int>& lineChanges, int plannedBefore, int length, int insertedLength)
{
    const int mark { length == 0 ? map::LineChange::added : map::LineChange::modified };

    for (int offset { 0 }; offset < insertedLength; ++offset)
        lineChanges.set (plannedBefore + offset, mark);
}

void LineDiff::setChangeMarks (jam::Array<int>& lineChanges, int plannedBefore, int length, juce::Range<int> insertedRange)
{
    if (insertedRange.isEmpty())
        setRemovedMark (lineChanges, plannedBefore);
    else
        setInsertedMarks (lineChanges, plannedBefore, length, insertedRange.getLength());
}

jam::Array<int> LineDiff::getLineChanges (int numLines) const
{
    jam::Array<int> lineChanges;
    lineChanges.resize (numLines);
    int plannedShift { 0 };

    applyToChanges ([&lineChanges, &plannedShift] (int start, int length, juce::Range<int> insertedRange)
    {
        const int plannedBefore { start + plannedShift };
        setChangeMarks (lineChanges, plannedBefore, length, insertedRange);
        plannedShift += insertedRange.getLength() - length;
    });

    return lineChanges;
}

void LineDiff::appendHunkRange (std::string& output, int zeroBasedStart, int lineCount)
{
    assert (lineCount >= 0);
    jam::Format::appendDecimal (output, lineCount == 0 ? zeroBasedStart : zeroBasedStart + 1);

    if (lineCount != 1)
    {
        jam::Format::appendUtf8 (output, Chars::comma);
        jam::Format::appendDecimal (output, lineCount);
    }
}

void LineDiff::appendHunkChange (std::string& output,
                                 const TextDocument& original,
                                 std::string_view plannedBytes,
                                 int incompletePlannedLine,
                                 const Change& change,
                                 int originalLine,
                                 int plannedLine) const
{
    for (int index { 0 }; index < change.length; ++index)
    {
        const int lineIndex { originalLine + index };
        appendDiffLine (output, Chars::dash, original.getLine (lineIndex + 1), isIncompleteLine (original, lineIndex + 1));
    }

    for (int index { 0 }; index < change.insertedRange.getLength(); ++index)
    {
        const int lineIndex { plannedLine + index };
        const auto [offset, size] { insertedLines.at (change.insertedRange.getStart() + index) };
        appendDiffLine (output, Chars::plus, plannedBytes.substr (offset, size), lineIndex + 1 == incompletePlannedLine);
    }
}

void LineDiff::appendHunkBody (std::string& output,
                               const TextDocument& original,
                               std::string_view plannedBytes,
                               int incompletePlannedLine,
                               juce::Range<int> hunkRange,
                               int oldStart,
                               int oldEnd,
                               int newStart) const
{
    int originalLine { oldStart };
    int plannedLine { newStart };

    for (int index { hunkRange.getStart() }; index < hunkRange.getEnd(); ++index)
    {
        const auto& change { changes.at (index) };
        appendContextLines (output, original, originalLine, change.start);
        plannedLine += change.start - originalLine;
        originalLine = change.start;
        appendHunkChange (output, original, plannedBytes, incompletePlannedLine, change, originalLine, plannedLine);
        originalLine += change.length;
        plannedLine += change.insertedRange.getLength();
    }

    appendContextLines (output, original, originalLine, oldEnd);
}

void LineDiff::appendUnifiedDiff (std::string& output, const TextDocument& original, std::string_view plannedBytes, bool isOriginalExisting, bool isPlannedExisting) const
{
    if (not changes.isEmpty())
    {
        const int incompletePlannedLine { getIncompleteLineNumber (plannedBytes) };
        jam::Format::appendBytes (output, text::Formats::diffOldHeader);
        jam::Format::appendBytes (output, isOriginalExisting ? original.getDisplayPath() : Id::nullDevice);
        jam::Format::appendUtf8 (output, Chars::newline);
        jam::Format::appendBytes (output, text::Formats::diffNewHeader);
        jam::Format::appendBytes (output, isPlannedExisting ? original.getDisplayPath() : Id::nullDevice);

        if (isPlannedExisting)
            jam::Format::appendBytes (output, text::Formats::diffPlanned);
        else
            jam::Format::appendUtf8 (output, Chars::newline);
        int cumulativeDelta { 0 };
        int hunkStart { 0 };

        while (hunkStart < changes.size())
        {
            int hunkEnd { hunkStart + 1 };

            while (hunkEnd < changes.size())
            {
                const auto& previous { changes.at (hunkEnd - 1) };
                const auto& next { changes.at (hunkEnd) };

                if (next.start - (previous.start + previous.length) > 2 * contextLineCount)
                    break;

                ++hunkEnd;
            }

            const juce::Range<int> hunkRange (hunkStart, hunkEnd);
            const int oldStart { std::max (0, changes.at (hunkStart).start - contextLineCount) };
            const auto& lastChange { changes.at (hunkEnd - 1) };
            const int oldEnd { std::min (original.getNumLines(), lastChange.start + lastChange.length + contextLineCount) };
            const int hunkDelta { getHunkDelta (changes, hunkRange) };
            const int newStart { oldStart + cumulativeDelta };
            jam::Format::appendBytes (output, text::Formats::diffHunkOpen);
            appendHunkRange (output, oldStart, oldEnd - oldStart);
            jam::Format::appendBytes (output, text::Formats::diffHunkNew);
            appendHunkRange (output, newStart, (oldEnd - oldStart) + hunkDelta);
            jam::Format::appendBytes (output, text::Formats::diffHunkClose);
            appendHunkBody (output, original, plannedBytes, incompletePlannedLine, hunkRange, oldStart, oldEnd, newStart);
            cumulativeDelta += hunkDelta;
            hunkStart = hunkEnd;
        }
    }
}
