#include "Search.h"
#include "Input.h"
#include "Jobs.h"
#include "MatchWriter.h"
#include "StandardStream.h"
#include "Stdio.h"
#include "TextDocument.h"
#include "generated/Generated.h"

static constexpr int initialLineNumber { 1 };

static std::string getFormattedCountOutput (std::string_view displayPath,
                                            int matchCount,
                                            bool isShowingPathPrefix)
{
    std::string output;

    if (isShowingPathPrefix)
    {
        jam::Format::appendBytes (output, displayPath);
        output.push_back (static_cast<char> (Chars::colon));
    }

    jam::Format::appendDecimal (output, matchCount);
    output.push_back (static_cast<char> (Chars::newline));
    return output;
}

static size_t getLineStart (const char* data, size_t pos, size_t minOffset) noexcept
{
    static constexpr size_t wordSize { sizeof (std::uint64_t) };
    static constexpr std::uint64_t lowBits { 0x0101010101010101 };
    static constexpr std::uint64_t highBits { 0x8080808080808080 };
    static constexpr std::uint64_t newlines { lowBits * static_cast<std::uint64_t> (Chars::newline) };
    size_t lineStart { pos };

    while (lineStart - minOffset >= wordSize)
    {
        std::uint64_t word {};
        std::memcpy (&word, data + lineStart - wordSize, wordSize);
        const std::uint64_t difference { word ^ newlines };

        if (((difference - lowBits) & ~ difference & highBits) != 0)
            break;

        lineStart -= wordSize;
    }

    while (lineStart > minOffset and data[lineStart - 1] != static_cast<char> (Chars::newline))
        --lineStart;

    return lineStart;
}

static std::tuple<juce::Result, int, std::string> getBinaryOutcome (std::string_view displayPath, bool isExplicit)
{
    if (isExplicit)
        return { TextDocument::getBinaryFailure (jam::Strings::fromView (displayPath)),
                 0,
                 {} };

    return { juce::Result::ok(), 0, {} };
}

static std::tuple<juce::Result, int, std::string> getCountOutcome (std::string_view displayPath,
                                                                   int matchCount,
                                                                   bool isExplicit,
                                                                   bool isShowingPathPrefix)
{
    if (isExplicit or matchCount > 0)
        return { juce::Result::ok(), matchCount, getFormattedCountOutput (displayPath, matchCount, isShowingPathPrefix) };

    return { juce::Result::ok(), 0, {} };
}

static std::pair<juce::Result, bool> getLineMatch (const Pattern& pattern,
                                                   std::string_view line,
                                                   bool isPossible,
                                                   jam::RegExpAutomaton& automaton)
{
    if (not isPossible) return { juce::Result::ok(), false };

    if (pattern.isConfirmed()) return { juce::Result::ok(), true };

    return pattern.hasMatch (line, automaton);
}

static std::vector<size_t> getChunkEnds (std::string_view bytes)
{
    static constexpr size_t chunkSize { 1024 * 1024 };
    std::vector<size_t> chunkEnds;
    size_t chunkStart { 0 };

    while (chunkStart < bytes.size())
    {
        const size_t targetEnd { std::min (chunkStart + chunkSize, bytes.size()) };
        const size_t newlinePos { jam::simd::findByte (bytes.data(), bytes.size(), targetEnd - 1, static_cast<char> (Chars::newline)) };
        chunkStart = newlinePos < bytes.size() ? newlinePos + 1 : bytes.size();
        chunkEnds.push_back (chunkStart);
    }

    return chunkEnds;
}

static std::string_view getChunk (std::string_view bytes, const std::vector<size_t>& chunkEnds, int chunkIndex)
{
    const size_t index { static_cast<size_t> (chunkIndex) };
    const size_t chunkStart { index == 0 ? size_t { 0 } : chunkEnds.at (index - 1) };
    return bytes.substr (chunkStart, chunkEnds.at (index) - chunkStart);
}

static int getFirstLineNumber (const jam::Array<size_t>& newlineCounts, int chunkIndex)
{
    size_t newlinesBefore { 0 };

    for (int index { 0 }; index < chunkIndex; ++index)
        newlinesBefore += newlineCounts.at (index);

    return initialLineNumber + static_cast<int> (newlinesBefore);
}

static juce::Result getFirstFailure (const jam::Array<juce::Result>& chunkStatuses)
{
    for (int index { 0 }; index < chunkStatuses.size(); ++index)
    {
        if (chunkStatuses.at (index).failed())
            return chunkStatuses.at (index);
    }

    return juce::Result::ok();
}

Search::Search (const Arguments& arguments, int newBeforeContext, int newAfterContext, int newAfterLine, juce::Range<int> newLineRange)
    : isQuiet (arguments.contains (Id::quietOption)),
      isFilesWithMatches (arguments.contains (Id::filesWithMatchesOption)),
      isCount (arguments.contains (Id::countOption)),
      isOnlyMatching (arguments.contains (Id::onlyMatchingOption)),
      isInvert (arguments.contains (Id::invertOption)),
      isFirst (arguments.contains (Id::firstOption)),
      isShowingLineNumber (not arguments.contains (Id::noLineNumberOption)),
      isShowingHash (arguments.contains (Id::hashOption)),
      beforeContext (newBeforeContext),
      afterContext (newAfterContext),
      afterLine (newAfterLine),
      lineRange (newLineRange)
{
}

std::tuple<juce::Result, int, std::string> Search::run (std::string_view bytes,
                                                        std::string_view displayPath,
                                                        const Pattern& pattern,
                                                        jam::RegExpAutomaton& automaton,
                                                        bool isExplicit,
                                                        bool isShowingPathPrefix) const
{
    if (isChunkedSearch (bytes.size()))
        return runChunks (bytes, displayPath, pattern, isExplicit, isShowingPathPrefix);

    const bool isRangeBounded { lineRange.getEnd() != Arguments::openRangeEnd };
    const std::string_view rangeBytes { isRangeBounded ? TextDocument::getLeadingLines (bytes, lineRange.getEnd() - 1 + std::min (afterContext, Arguments::openRangeEnd - lineRange.getEnd())) : bytes };
    auto [searchResult, searchCount, searchOutput, searchedSize] { runLines (rangeBytes, displayPath, pattern, automaton, isExplicit, isShowingPathPrefix, initialLineNumber) };
    const bool isBinaryRelevant { isExplicit or isCount or searchResult.failed() or searchCount > 0 };

    if (isBinaryRelevant and TextDocument::isBinary (rangeBytes.substr (0, searchedSize)))
        return getBinaryOutcome (displayPath, isExplicit);

    return { std::move (searchResult), searchCount, std::move (searchOutput) };
}

std::tuple<juce::Result, int, std::string> Search::runFile (const juce::File& file,
                                                            std::string_view displayPath,
                                                            const Pattern& pattern,
                                                            jam::RegExpAutomaton& automaton,
                                                            bool isExplicit,
                                                            bool isShowingPathPrefix) const
{
    const auto input { Input::fromFile (file, Input::maxBufferedReadSize) };

    if (input == nullptr)
        return { TextDocument::getReadFailure (file, jam::Strings::fromView (displayPath)), 0, {} };

    if (input->getBytes().empty())
        return getEmptyFileOutcome (displayPath, isExplicit, isShowingPathPrefix);

    return run (input->getBytes(), displayPath, pattern, automaton, isExplicit, isShowingPathPrefix);
}

std::tuple<juce::Result, int, std::string, size_t> Search::runStream (const Pattern& pattern) const
{
    static constexpr size_t initialCapacity { 256 * 1024 };
    const bool isStoppingAtFirst { isQuiet or isFilesWithMatches or isFirst };
    const bool isNumbered { isShowingLineNumber or isShowingHash or afterLine > 0 or lineRange.getStart() != 1 or lineRange.getEnd() != Arguments::openRangeEnd };
    jam::RegExpAutomaton automaton;
    size_t capacity { initialCapacity };
    juce::HeapBlock<char> buffer { capacity };
    size_t size { 0 };
    size_t readSize { 0 };
    int lineNumber { initialLineNumber };
    int matchCount { 0 };
    std::string output;
    juce::int64 numRead { 1 };

    while (numRead > 0 and lineNumber < lineRange.getEnd() and not (isStoppingAtFirst and matchCount > 0))
    {
        numRead = StandardStream::read (buffer.getData() + size, capacity - size);

        if (numRead < 0)
            return { juce::Result::fail (text::Diagnostics::failStandardInput), 0, {}, readSize };

        size += static_cast<size_t> (numRead);
        readSize += static_cast<size_t> (numRead);
        const std::string_view pending { buffer.getData(), size };
        const size_t lastNewline { pending.substr (size - static_cast<size_t> (numRead)).rfind (static_cast<char> (Chars::newline)) };
        const size_t completeSize { lastNewline == std::string_view::npos ? size_t { 0 } : (size - static_cast<size_t> (numRead)) + lastNewline + 1 };
        const std::string_view chunk { pending.substr (0, numRead == 0 ? size : completeSize) };

        if (not chunk.empty())
        {
            const std::string_view rangeChunk { lineRange.getEnd() == Arguments::openRangeEnd ? chunk : TextDocument::getLeadingLines (chunk, lineRange.getEnd() - lineNumber) };
            const auto [chunkStatus, chunkMatches, chunkOutput, searchedSize] { runLines (rangeChunk, Id::stdinPath, pattern, automaton, true, false, lineNumber) };

            if (TextDocument::isBinary (rangeChunk.substr (0, searchedSize)))
            {
                auto [binaryResult, binaryCount, binaryOutput] { getBinaryOutcome (Id::stdinPath, true) };
                return { std::move (binaryResult), binaryCount, std::move (binaryOutput), readSize };
            }

            if (chunkStatus.failed())
                return { chunkStatus, 0, {}, readSize };

            matchCount += chunkMatches;

            if (isFilesWithMatches or not isCount)
                output.append (chunkOutput);

            if (isNumbered)
                lineNumber += static_cast<int> (jam::simd::countByte (chunk.data(), chunk.size(), static_cast<char> (Chars::newline)));

            size -= chunk.size();
            std::memmove (buffer.getData(), buffer.getData() + chunk.size(), size);
        }
        else if (size == capacity)
        {
            capacity *= 2;
            buffer.realloc (capacity);
        }
    }

    if (isCount and not isQuiet and not isFilesWithMatches)
        output = getFormattedCountOutput (Id::stdinPath, matchCount, false);

    return { juce::Result::ok(), matchCount, std::move (output), readSize };
}

std::tuple<juce::Result, int, std::string, size_t> Search::runLines (std::string_view bytes,
                                                                     std::string_view displayPath,
                                                                     const Pattern& pattern,
                                                                     jam::RegExpAutomaton& automaton,
                                                                     bool isExplicit,
                                                                     bool isShowingPathPrefix,
                                                                     int firstLineNumber) const
{
    const bool isLineLimited { afterLine > 0 or lineRange.getStart() != 1 or lineRange.getEnd() != Arguments::openRangeEnd };
    const bool isLineAppended { not isCount and not (isInvert and isOnlyMatching) };
    MatchWriter writer { bytes, displayPath, beforeContext, afterContext, isOnlyMatching, isShowingPathPrefix, isShowingLineNumber, isShowingHash, firstLineNumber };
    size_t cursor { 0 };
    const bool isInvertFirst { isInvert and (isQuiet or isFilesWithMatches or isFirst) };
    size_t invertCandidate { isInvert and not isInvertFirst ? pattern.findCandidate (bytes, 0, automaton) : bytes.size() };
    int matchCount { 0 };
    size_t searchedSize { bytes.size() };

    while (cursor < bytes.size())
    {
        const size_t pos { isInvert ? cursor : pattern.findCandidate (bytes, cursor, automaton) };

        if (pos >= bytes.size())
            break;

        const size_t lineStart { getLineStart (bytes.data(), pos, cursor) };
        const size_t newlinePos { jam::simd::findByte (bytes.data(), bytes.size(), pos, static_cast<char> (Chars::newline)) };
        const std::string_view line { TextDocument::getLine (bytes, lineStart, newlinePos) };
        const size_t lineEnd { newlinePos < bytes.size() ? newlinePos + 1 : bytes.size() };

        if (isLineLimited)
            writer.setLineNumber (lineStart);

        const int lineNumber { isLineLimited ? writer.getLineNumber() : firstLineNumber };

        if (isInvertFirst)
            invertCandidate = pattern.findCandidate (bytes.substr (0, lineEnd), lineStart, automaton);
        else if (isInvert and invertCandidate < lineStart)
            invertCandidate = pattern.findCandidate (bytes, lineStart, automaton);

        const size_t candidate { isInvert ? invertCandidate : pos };
        const bool isPossible { candidate < bytes.size() and candidate <= newlinePos };

        if (lineNumber >= lineRange.getEnd())
        {
            searchedSize = lineStart;
            break;
        }

        if (lineNumber > afterLine and lineRange.contains (lineNumber))
        {
            const auto [matchResult, isMatching] { getLineMatch (pattern, line, isPossible, automaton) };

            if (matchResult.failed())
                return { matchResult, 0, {}, lineEnd };

            if (isMatching != isInvert)
            {
                ++matchCount;

                if (isQuiet)
                    return { juce::Result::ok(), 1, {}, lineEnd };

                if (isFilesWithMatches)
                {
                    std::string output;
                    jam::Format::appendBytes (output, displayPath);
                    output.push_back (static_cast<char> (Chars::newline));
                    return { juce::Result::ok(), 1, std::move (output), lineEnd };
                }

                if (isLineAppended)
                {
                    const auto appendResult { writer.addMatch (pattern, lineStart, newlinePos) };

                    if (appendResult.failed())
                        return { appendResult, 0, {}, lineEnd };
                }

                if (isFirst)
                {
                    searchedSize = lineEnd;
                    break;
                }
            }
        }

        cursor = lineEnd;
    }

    if (isQuiet or isFilesWithMatches)
        return { juce::Result::ok(), matchCount, {}, searchedSize };

    if (isCount)
    {
        auto [countResult, countMatches, countOutput] { getCountOutcome (displayPath, matchCount, isExplicit, isShowingPathPrefix) };
        return { std::move (countResult), countMatches, std::move (countOutput), searchedSize };
    }

    auto output { writer.getOutput() };
    return { juce::Result::ok(), matchCount, std::move (output), std::max (searchedSize, writer.getPrintedSize()) };
}

std::tuple<juce::Result, int, std::string> Search::runChunks (std::string_view bytes,
                                                               std::string_view displayPath,
                                                               const Pattern& pattern,
                                                               bool isExplicit,
                                                               bool isShowingPathPrefix) const
{
    const auto chunkEnds { getChunkEnds (bytes) };
    const int chunkCount { static_cast<int> (chunkEnds.size()) };
    jam::Array<size_t> newlineCounts;
    newlineCounts.resize (chunkCount);
    jam::Array<bool> nulFlags;
    nulFlags.resize (chunkCount);

    Jobs::run (chunkCount, [bytes, &chunkEnds, &newlineCounts, &nulFlags] (int index)
    {
        const std::string_view chunk { getChunk (bytes, chunkEnds, index) };
        newlineCounts.at (index) = jam::simd::countByte (chunk.data(), chunk.size(), static_cast<char> (Chars::newline));
        nulFlags.at (index) = TextDocument::isBinary (chunk);
    });

    bool hasNul { false };

    for (int index { 0 }; index < chunkCount; ++index)
        hasNul = hasNul or nulFlags.at (index);

    if ((isExplicit or isCount) and hasNul)
        return getBinaryOutcome (displayPath, isExplicit);

    jam::Array<juce::Result> chunkStatuses;
    chunkStatuses.insertMultiple (0, juce::Result::ok(), chunkCount);
    jam::Array<int> chunkMatchCounts;
    chunkMatchCounts.resize (chunkCount);
    jam::Array<std::string> chunkOutputs;
    chunkOutputs.resize (chunkCount);

    Jobs::run (chunkCount, [this, bytes, displayPath, &pattern, isExplicit, isShowingPathPrefix, &chunkEnds, &newlineCounts, &chunkStatuses, &chunkMatchCounts, &chunkOutputs] (int index)
    {
        jam::RegExpAutomaton automaton;
        auto [chunkStatus, chunkMatches, chunkOutput, chunkSearchedSize] { runLines (getChunk (bytes, chunkEnds, index),
                                                                                     displayPath,
                                                                                     pattern,
                                                                                     automaton,
                                                                                     isExplicit,
                                                                                     isShowingPathPrefix,
                                                                                     getFirstLineNumber (newlineCounts, index)) };
        juce::ignoreUnused (chunkSearchedSize);
        chunkStatuses.at (index) = std::move (chunkStatus);
        chunkMatchCounts.at (index) = chunkMatches;
        chunkOutputs.at (index) = std::move (chunkOutput);
    });

    const juce::Result firstFailure { getFirstFailure (chunkStatuses) };
    int totalMatches { 0 };

    for (int index { 0 }; index < chunkCount; ++index)
        totalMatches += chunkMatchCounts.at (index);

    if (not isExplicit and not isCount and (firstFailure.failed() or totalMatches > 0) and hasNul)
        return getBinaryOutcome (displayPath, false);

    if (firstFailure.failed())
        return { firstFailure, 0, {} };

    if (isCount)
        return getCountOutcome (displayPath, totalMatches, isExplicit, isShowingPathPrefix);

    return { juce::Result::ok(), totalMatches, Stdio::getJoinedOutput (chunkOutputs) };
}

std::tuple<juce::Result, int, std::string> Search::getEmptyFileOutcome (std::string_view displayPath,
                                                                        bool isExplicit,
                                                                        bool isShowingPathPrefix) const
{
    if (isExplicit and isCount and not isQuiet and not isFilesWithMatches)
        return { juce::Result::ok(), 0, getFormattedCountOutput (displayPath, 0, isShowingPathPrefix) };

    return { juce::Result::ok(), 0, {} };
}

bool Search::isChunkedSearch (size_t size) const noexcept
{
    static constexpr size_t chunkedSearchSize { 4 * 1024 * 1024 };

    return size >= chunkedSearchSize
       and beforeContext == 0
       and afterContext == 0
       and not isFirst
       and not isQuiet
       and not isFilesWithMatches
       and afterLine == 0
       and lineRange.getStart() == 1
       and lineRange.getEnd() == Arguments::openRangeEnd;
}

bool Search::isStreamSearch() const noexcept
{
    return beforeContext == 0 and afterContext == 0;
}
