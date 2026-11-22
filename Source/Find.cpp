#include "Find.h"
#include "Arguments.h"
#include "Jobs.h"
#include "Paths.h"
#include "Pattern.h"
#include "Stdio.h"
#include "TextDocument.h"
#include "generated/Generated.h"

#ifndef _WIN32
#include <cerrno>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

static void appendLinePrefix (std::string& output,
                              std::string_view displayPath,
                              int lineNumber,
                              std::string_view line,
                              char separator,
                              bool isShowingPathPrefix,
                              bool isShowingLineNumber,
                              bool isShowingHash)
{
    if (isShowingPathPrefix)
    {
        jam::Format::appendBytes (output, displayPath);
        output.push_back (separator);
    }

    if (isShowingHash)
    {
        const auto lineHash { juce::SHA256 (line.data(), line.size()).toHexString().substring (0, TextDocument::lineHashLength) };
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

static void appendFormattedLine (std::string& output,
                                 std::string_view displayPath,
                                 std::string_view line,
                                 int lineNumber,
                                 char separator,
                                 bool isShowingPathPrefix,
                                 bool isShowingLineNumber,
                                 bool isShowingHash)
{
    appendLinePrefix (output, displayPath, lineNumber, line, separator, isShowingPathPrefix, isShowingLineNumber, isShowingHash);
    jam::Format::appendBytes (output, line);
    output.push_back (static_cast<char> (Chars::newline));
}

static jam::Array<int> getMatchedLines (const TextDocument& document,
                                        const Pattern& pattern,
                                        int afterLine,
                                        juce::Range<int> lineRange,
                                        bool isInvert,
                                        bool isFirstOrQuiet)
{
    jam::Array<int> matchedLines;
    const auto numLines { document.getNumLines() };

    for (int lineNumber { 1 }; lineNumber <= numLines; ++lineNumber)
    {
        if (lineNumber > afterLine and lineRange.contains (lineNumber))
        {
            const bool matched { pattern.hasMatch (document.getLine (lineNumber)) != isInvert };

            if (matched)
            {
                matchedLines.add (lineNumber);

                if (isFirstOrQuiet)
                    break;
            }
        }
    }

    return matchedLines;
}

static jam::Array<juce::Range<int>> getContextIntervals (const jam::Array<int>& matchedLines,
                                                         int numLines,
                                                         int contextLines)
{
    jam::Array<juce::Range<int>> intervals;

    for (const int matchedLine : matchedLines)
    {
        const auto startLine { contextLines >= matchedLine ? 1 : matchedLine - contextLines };
        const auto maxAfter { numLines - matchedLine };
        const auto endLine { contextLines >= maxAfter ? numLines : matchedLine + contextLines };
        const auto endExclusive { endLine + 1 };

        if (not intervals.isEmpty() and startLine <= intervals.last().getEnd())
            intervals.last().setEnd (std::max (intervals.last().getEnd(), endExclusive));
        else
            intervals.add (juce::Range<int> (startLine, endExclusive));
    }

    return intervals;
}

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

static std::string getFormattedMatchesOutput (const TextDocument& document,
                                              const jam::Array<int>& matchedLines,
                                              bool isShowingPathPrefix,
                                              bool isShowingLineNumber,
                                              bool isShowingHash)
{
    std::string output;
    const auto displayPath { jam::Strings::getView (document.getDisplayPath()) };

    for (const int lineNumber : matchedLines)
        appendFormattedLine (output, displayPath, document.getLine (lineNumber), lineNumber, static_cast<char> (Chars::colon), isShowingPathPrefix, isShowingLineNumber, isShowingHash);

    return output;
}

static std::string getFormattedContextOutput (const TextDocument& document,
                                              const jam::Array<int>& matchedLines,
                                              int contextLines,
                                              bool isShowingPathPrefix,
                                              bool isShowingLineNumber,
                                              bool isShowingHash)
{
    std::string output;
    const auto displayPath { jam::Strings::getView (document.getDisplayPath()) };
    const auto intervals (getContextIntervals (matchedLines, document.getNumLines(), contextLines));
    int matchIndex { 0 };

    for (int intervalIndex { 0 }; intervalIndex < intervals.size(); ++intervalIndex)
    {
        if (intervalIndex > 0)
        {
            jam::Format::appendBytes (output, Chars::doubleDash);
            output.push_back (static_cast<char> (Chars::newline));
        }

        const auto interval { intervals.at (intervalIndex) };

        for (int lineNumber { interval.getStart() }; lineNumber < interval.getEnd(); ++lineNumber)
        {
            while (matchIndex < matchedLines.size() and matchedLines.at (matchIndex) < lineNumber)
                ++matchIndex;

            const bool isMatchedLine { matchIndex < matchedLines.size() and matchedLines.at (matchIndex) == lineNumber };
            const char separator { static_cast<char> (isMatchedLine ? Chars::colon : Chars::dash) };
            appendFormattedLine (output, displayPath, document.getLine (lineNumber), lineNumber, separator, isShowingPathPrefix, isShowingLineNumber, isShowingHash);
        }
    }

    return output;
}

static std::string getFormattedDocumentMatches (const TextDocument& document,
                                                const jam::Array<int>& matchedLines,
                                                const Arguments& arguments,
                                                int contextLines,
                                                bool isShowingPathPrefix)
{
    const auto isShowingLineNumber { not arguments.contains (Id::noLineNumberOption) };
    const auto isShowingHash { arguments.contains (Id::hashOption) };

    if (contextLines == 0)
        return getFormattedMatchesOutput (document, matchedLines, isShowingPathPrefix, isShowingLineNumber, isShowingHash);

    return getFormattedContextOutput (document, matchedLines, contextLines, isShowingPathPrefix, isShowingLineNumber, isShowingHash);
}

static std::pair<int, std::string> searchDocument (const TextDocument& document,
                                                   const Pattern& pattern,
                                                   const Arguments& arguments,
                                                   int contextLines,
                                                   int afterLine,
                                                   juce::Range<int> lineRange,
                                                   bool isShowingPathPrefix)
{
    const auto isQuiet { arguments.contains (Id::quietOption) };
    const auto isCount { arguments.contains (Id::countOption) };
    const auto isFirst { arguments.contains (Id::firstOption) };
    const auto matchedLines (getMatchedLines (document,
                                              pattern,
                                              afterLine,
                                              lineRange,
                                              arguments.contains (Id::invertOption),
                                              isFirst or isQuiet));

    if (isQuiet)
        return { matchedLines.size(), {} };

    if (isCount)
        return { matchedLines.size(), getFormattedCountOutput (jam::Strings::getView (document.getDisplayPath()), matchedLines.size(), isShowingPathPrefix) };

    if (matchedLines.isEmpty())
        return { 0, {} };

    return { matchedLines.size(), getFormattedDocumentMatches (document, matchedLines, arguments, contextLines, isShowingPathPrefix) };
}

static size_t getLineStart (const char* data, size_t pos, size_t minOffset) noexcept
{
    size_t lineStart { pos };

    while (lineStart > minOffset and data[lineStart - 1] != static_cast<char> (Chars::newline))
        --lineStart;

    return lineStart;
}

static size_t getLineEnd (const char* data, size_t lineStart, size_t newlinePos, size_t size) noexcept
{
    if (newlinePos < size and newlinePos > lineStart and data[newlinePos - 1] == static_cast<char> (Chars::carriageReturn))
        return newlinePos - 1;

    return newlinePos;
}

static int getAdvancedLineNumber (const char* data,
                                  size_t countedUpTo,
                                  size_t lineStart,
                                  int currentLineNumber) noexcept
{
    size_t scanPos { countedUpTo };

    while (scanPos < lineStart)
    {
        const size_t nextNewline { jam::simd::findByte (data, lineStart, scanPos, static_cast<char> (Chars::newline)) };

        if (nextNewline >= lineStart)
            break;

        ++currentLineNumber;
        scanPos = nextNewline + 1;
    }

    return currentLineNumber;
}

static std::pair<int, std::string> searchMappedBuffer (const char* data,
                                                       size_t size,
                                                       std::string_view displayPathView,
                                                       const Pattern& pattern,
                                                       const Arguments& arguments,
                                                       int afterLine,
                                                       juce::Range<int> lineRange,
                                                       bool isShowingPathPrefix)
{
    const auto prefilter { pattern.getPrefilterLiteral() };
    const auto isQuiet { arguments.contains (Id::quietOption) };
    const auto isCount { arguments.contains (Id::countOption) };
    const auto isFirst { arguments.contains (Id::firstOption) };
    const auto isShowingLineNumber { not arguments.contains (Id::noLineNumberOption) };
    const auto isShowingHash { arguments.contains (Id::hashOption) };
    const char firstByte { prefilter.at (0) };
    const size_t prefilterSize { prefilter.size() };
    size_t cursor { 0 };
    size_t countedUpTo { 0 };
    int lineNumber { 1 };
    int matchCount { 0 };
    std::string output;

    while (cursor < size)
    {
        const size_t pos { jam::simd::findByte (data, size, cursor, firstByte) };

        if (pos >= size or prefilterSize > size - pos)
            break;

        if (prefilterSize > 1 and std::memcmp (data + pos + 1, prefilter.data() + 1, prefilterSize - 1) != 0)
        {
            cursor = pos + 1;
            continue;
        }

        const size_t lineStart { getLineStart (data, pos, countedUpTo) };
        lineNumber = getAdvancedLineNumber (data, countedUpTo, lineStart, lineNumber);
        countedUpTo = lineStart;

        if (lineNumber >= lineRange.getEnd())
            break;

        const size_t newlinePos { jam::simd::findByte (data, size, pos + prefilterSize, static_cast<char> (Chars::newline)) };
        const size_t lineEnd { getLineEnd (data, lineStart, newlinePos, size) };
        const std::string_view line { data + lineStart, lineEnd - lineStart };

        if (lineNumber > afterLine and lineRange.contains (lineNumber) and pattern.hasMatch (line))
        {
            ++matchCount;

            if (isQuiet)
                return { 1, {} };

            if (not isCount)
                appendFormattedLine (output, displayPathView, line, lineNumber, static_cast<char> (Chars::colon), isShowingPathPrefix, isShowingLineNumber, isShowingHash);

            if (isFirst)
                break;
        }

        cursor = newlinePos < size ? newlinePos + 1 : size;
    }

    if (isQuiet)
        return { 0, {} };

    if (isCount)
        return { matchCount, getFormattedCountOutput (displayPathView, matchCount, isShowingPathPrefix) };

    return { matchCount, std::move (output) };
}

static std::tuple<juce::Result, int, std::string> searchBufferFast (const char* data,
                                                                    size_t size,
                                                                    std::string_view displayPath,
                                                                    bool isExplicit,
                                                                    const Pattern& pattern,
                                                                    const Arguments& arguments,
                                                                    int afterLine,
                                                                    juce::Range<int> lineRange,
                                                                    bool isShowingPathPrefix)
{
    const bool isCountOnly { arguments.contains (Id::countOption) };

    if (isExplicit or isCountOnly)
    {
        if (jam::simd::findByte (data, size, 0, '\0') < size)
        {
            if (isExplicit)
                return { juce::Result::fail (juce::String::formatted (text::Diagnostics::failBinaryFile, jam::Strings::fromView (displayPath).toRawUTF8())),
                         0,
                         {} };

            return { juce::Result::ok(), 0, {} };
        }
    }

    auto [matchCount, output] { searchMappedBuffer (data, size, displayPath, pattern, arguments, afterLine, lineRange, isShowingPathPrefix) };

    if (not isExplicit and not isCountOnly and matchCount > 0)
    {
        if (jam::simd::findByte (data, size, 0, '\0') < size)
            return { juce::Result::ok(), 0, {} };
    }

    return { juce::Result::ok(), matchCount, std::move (output) };
}

#ifndef _WIN32

struct ScopedFileDescriptor
{
    int fd { -1 };

    explicit ScopedFileDescriptor (int newFd) noexcept
        : fd (newFd)
    {
    }

    ~ScopedFileDescriptor() noexcept
    {
        if (fd >= 0)
            ::close (fd);
    }

    ScopedFileDescriptor (const ScopedFileDescriptor&) = delete;
    ScopedFileDescriptor& operator= (const ScopedFileDescriptor&) = delete;
};

struct ScopedMemoryMap
{
    void* address { MAP_FAILED };
    size_t length { 0 };

    ScopedMemoryMap (void* newAddress, size_t newLength) noexcept
        : address (newAddress),
          length (newLength)
    {
    }

    ~ScopedMemoryMap() noexcept
    {
        if (address != MAP_FAILED)
            ::munmap (address, length);
    }

    ScopedMemoryMap (const ScopedMemoryMap&) = delete;
    ScopedMemoryMap& operator= (const ScopedMemoryMap&) = delete;
};

static bool readAllBytes (int fd, char* destination, size_t totalBytes) noexcept
{
    size_t bytesRead { 0 };

    while (bytesRead < totalBytes)
    {
        const ssize_t result { ::read (fd, destination + bytesRead, totalBytes - bytesRead) };

        if (result > 0)
        {
            bytesRead += static_cast<size_t> (result);
        }
        else if (result == 0)
        {
            return false;
        }
        else if (errno != EINTR)
        {
            return false;
        }
    }

    return true;
}

static std::tuple<juce::Result, int, std::string> searchFileFast (const std::string& openPath,
                                                                  std::string_view displayPath,
                                                                  bool isExplicit,
                                                                  const Pattern& pattern,
                                                                  const Arguments& arguments,
                                                                  int afterLine,
                                                                  juce::Range<int> lineRange,
                                                                  bool isShowingPathPrefix)
{
    ScopedFileDescriptor fileDesc { ::open (openPath.c_str(), O_RDONLY) };

    if (fileDesc.fd < 0)
        return { juce::Result::fail (juce::String::formatted (text::Diagnostics::failNotFound, jam::Strings::fromView (displayPath).toRawUTF8())),
                 0,
                 {} };

    struct stat st {};

    if (::fstat (fileDesc.fd, &st) != 0 or S_ISDIR (st.st_mode))
        return { juce::Result::fail (juce::String::formatted (text::Diagnostics::failNotFound, jam::Strings::fromView (displayPath).toRawUTF8())),
                 0,
                 {} };

    const size_t size { static_cast<size_t> (st.st_size) };

    if (size == 0)
    {
        if (not arguments.contains (Id::quietOption) and arguments.contains (Id::countOption))
            return { juce::Result::ok(), 0, getFormattedCountOutput (displayPath, 0, isShowingPathPrefix) };

        return { juce::Result::ok(), 0, {} };
    }

    static constexpr size_t maxBufferedReadSize { 256 * 1024 };

    if (size <= maxBufferedReadSize)
    {
        thread_local std::vector<char> readBuffer;

        if (readBuffer.size() < size)
            readBuffer.resize (std::max (size, maxBufferedReadSize));

        if (not readAllBytes (fileDesc.fd, readBuffer.data(), size))
            return { juce::Result::fail (juce::String::formatted (text::Diagnostics::failNotFound, jam::Strings::fromView (displayPath).toRawUTF8())),
                     0,
                     {} };

        return searchBufferFast (readBuffer.data(),
                                 size,
                                 displayPath,
                                 isExplicit,
                                 pattern,
                                 arguments,
                                 afterLine,
                                 lineRange,
                                 isShowingPathPrefix);
    }

    ScopedMemoryMap mapped { ::mmap (nullptr, size, PROT_READ, MAP_PRIVATE, fileDesc.fd, 0), size };

    if (mapped.address == MAP_FAILED)
        return { juce::Result::fail (juce::String::formatted (text::Diagnostics::failNotFound, jam::Strings::fromView (displayPath).toRawUTF8())),
                 0,
                 {} };

    return searchBufferFast (static_cast<const char*> (mapped.address),
                             size,
                             displayPath,
                             isExplicit,
                             pattern,
                             arguments,
                             afterLine,
                             lineRange,
                             isShowingPathPrefix);
}

#else

static std::tuple<juce::Result, int, std::string> searchFileFast (const std::string& openPath,
                                                                  std::string_view displayPath,
                                                                  bool isExplicit,
                                                                  const Pattern& pattern,
                                                                  const Arguments& arguments,
                                                                  int afterLine,
                                                                  juce::Range<int> lineRange,
                                                                  bool isShowingPathPrefix)
{
    const auto openPathString { jam::Strings::fromView (openPath) };
    const auto childFile { juce::File::getCurrentWorkingDirectory().getChildFile (openPathString) };
    const auto resolved { isExplicit ? childFile.getLinkedTarget() : childFile };

    if (not resolved.existsAsFile())
        return { juce::Result::fail (juce::String::formatted (text::Diagnostics::failNotFound, jam::Strings::fromView (displayPath).toRawUTF8())),
                 0,
                 {} };

    if (resolved.getSize() == 0)
    {
        if (not arguments.contains (Id::quietOption) and arguments.contains (Id::countOption))
            return { juce::Result::ok(), 0, getFormattedCountOutput (displayPath, 0, isShowingPathPrefix) };

        return { juce::Result::ok(), 0, {} };
    }

    const juce::MemoryMappedFile mapped { resolved, juce::MemoryMappedFile::readOnly };

    if (mapped.getData() == nullptr)
        return { juce::Result::fail (juce::String::formatted (text::Diagnostics::failNotFound, jam::Strings::fromView (displayPath).toRawUTF8())),
                 0,
                 {} };

    return searchBufferFast (static_cast<const char*> (mapped.getData()),
                             mapped.getSize(),
                             displayPath,
                             isExplicit,
                             pattern,
                             arguments,
                             afterLine,
                             lineRange,
                             isShowingPathPrefix);
}

#endif

static bool canUseFastSearch (const Pattern& pattern,
                              const Arguments& arguments,
                              int contextLines) noexcept
{
    const auto isInvert { arguments.contains (Id::invertOption) };
    const auto prefilter { pattern.getPrefilterLiteral() };
    return contextLines == 0
       and not isInvert
       and not prefilter.empty()
       and prefilter.find (static_cast<char> (Chars::newline)) == std::string_view::npos;
}

static std::tuple<juce::Result, int, std::string> searchFile (const std::string& displayPath,
                                                              bool isExplicit,
                                                              const juce::File& workingDirectory,
                                                              const Pattern& pattern,
                                                              const Arguments& arguments,
                                                              int contextLines,
                                                              int afterLine,
                                                              juce::Range<int> lineRange,
                                                              bool isShowingPathPrefix)
{
    if (canUseFastSearch (pattern, arguments, contextLines))
    {
        return searchFileFast (displayPath,
                               displayPath,
                               isExplicit,
                               pattern,
                               arguments,
                               afterLine,
                               lineRange,
                               isShowingPathPrefix);
    }

    const auto displayPathString { jam::Strings::fromView (displayPath) };
    const auto childFile { workingDirectory.getChildFile (displayPathString) };
    const auto resolved { isExplicit ? childFile.getLinkedTarget() : childFile };
    const auto [loadResult, document] { TextDocument::fromFile (resolved, displayPathString) };

    if (loadResult.failed())
        return { loadResult, 0, {} };

    if (document->isBinary())
    {
        if (isExplicit)
            return { juce::Result::fail (juce::String::formatted (text::Diagnostics::failBinaryFile, displayPath.c_str())), 0, {} };

        return { juce::Result::ok(), 0, {} };
    }

    auto [matchCount, output] { searchDocument (*document, pattern, arguments, contextLines, afterLine, lineRange, isShowingPathPrefix) };
    return { juce::Result::ok(), matchCount, std::move (output) };
}

static int emitFileResults (const jam::Array<juce::Result>& statuses,
                            const jam::Array<int>& matchCounts,
                            const jam::Array<std::string>& outputs,
                            bool isQuiet)
{
    for (int index { 0 }; index < statuses.size(); ++index)
    {
        if (statuses.at (index).failed())
        {
            Stdio::printError (statuses.at (index).getErrorMessage());
            return Stdio::exitError;
        }
    }

    int totalMatches { 0 };

    for (int index { 0 }; index < outputs.size(); ++index)
    {
        totalMatches += matchCounts.at (index);

        if (not isQuiet and not outputs.at (index).empty())
            Stdio::write (outputs.at (index));
    }

    return totalMatches > 0 ? Stdio::exitSuccess : Stdio::exitMismatch;
}

static int runParallelFilesFind (const std::vector<std::string>& files,
                                 const jam::Array<bool>& isExplicitFlags,
                                 const Pattern& pattern,
                                 const Arguments& arguments,
                                 int contextLines,
                                 int afterLine,
                                 juce::Range<int> lineRange,
                                 bool isShowingPathPrefix)
{
    const auto fileCount { static_cast<int> (files.size()) };
    const auto workingDirectory { juce::File::getCurrentWorkingDirectory() };
    jam::Array<juce::Result> statuses;
    statuses.insertMultiple (0, juce::Result::ok(), fileCount);
    jam::Array<std::string> outputs;
    outputs.resize (fileCount);
    jam::Array<int> matchCounts;
    matchCounts.resize (fileCount);
    const auto& filePaths { files };
    const auto& explicitFlags { isExplicitFlags };

    Jobs::run (fileCount,
               [&filePaths, &explicitFlags, &workingDirectory, &pattern, &arguments, contextLines, afterLine, lineRange, isShowingPathPrefix, &statuses, &matchCounts, &outputs] (int index)
               {
                   auto [status, matchCount, output] { searchFile (filePaths.at (static_cast<size_t> (index)),
                                                                   explicitFlags.at (index),
                                                                   workingDirectory,
                                                                   pattern,
                                                                   arguments,
                                                                   contextLines,
                                                                   afterLine,
                                                                   lineRange,
                                                                   isShowingPathPrefix) };
                   statuses.at (index) = std::move (status);
                   matchCounts.at (index) = matchCount;
                   outputs.at (index) = std::move (output);
               });

    return emitFileResults (statuses, matchCounts, outputs, arguments.contains (Id::quietOption));
}

static int runFusedDirectoryFind (const juce::File& rootDirectory,
                                  std::string_view rawPathView,
                                  const Pattern& pattern,
                                  const Arguments& arguments,
                                  int afterLine,
                                  juce::Range<int> lineRange)
{
    std::vector<std::vector<std::pair<std::string, std::string>>> branchOutputs;
    std::vector<int> branchMatchCounts;
    jam::Array<juce::Result> branchStatuses;
    std::atomic<bool> shouldStop { false };
    const bool isQuiet { arguments.contains (Id::quietOption) };
    const bool isCountOnly { arguments.contains (Id::countOption) };

    Paths::applyToDirectory (rootDirectory,
                             rawPathView,
                             arguments.getValues (Id::includeOption),
                             arguments.getValues (Id::excludeOption),
                             arguments.contains (Id::hiddenOption),
                             arguments.contains (Id::noIgnoreOption),
                             [&branchOutputs, &branchMatchCounts, &branchStatuses] (int branchCount)
                             {
                                 branchOutputs.resize (static_cast<size_t> (branchCount));
                                 branchMatchCounts.resize (static_cast<size_t> (branchCount), 0);
                                 branchStatuses.insertMultiple (0, juce::Result::ok(), branchCount);
                             },
                             [&branchOutputs, &branchMatchCounts, &branchStatuses, &shouldStop, isQuiet, isCountOnly,
                              &pattern, &arguments, afterLine, lineRange] (int branchIndex, const std::string& fullPath, std::string_view displayPath)
                             {
                                 auto [status, matchCount, output] { searchFileFast (fullPath,
                                                                                     displayPath,
                                                                                     false,
                                                                                     pattern,
                                                                                     arguments,
                                                                                     afterLine,
                                                                                     lineRange,
                                                                                     true) };

                                 if (status.failed())
                                 {
                                     if (branchStatuses.at (branchIndex).wasOk())
                                         branchStatuses.at (branchIndex) = std::move (status);

                                     shouldStop.store (true, std::memory_order_relaxed);
                                 }
                                 else if (matchCount > 0)
                                 {
                                     branchMatchCounts.at (static_cast<size_t> (branchIndex)) += matchCount;

                                     if (isQuiet)
                                         shouldStop.store (true, std::memory_order_relaxed);
                                     else if (not output.empty())
                                         branchOutputs.at (static_cast<size_t> (branchIndex)).emplace_back (std::string { displayPath }, std::move (output));
                                 }
                                 else if (isCountOnly and not isQuiet and not output.empty())
                                 {
                                     branchOutputs.at (static_cast<size_t> (branchIndex)).emplace_back (std::string { displayPath }, std::move (output));
                                 }
                             },
                             shouldStop);

    for (int index { 0 }; index < branchStatuses.size(); ++index)
    {
        if (branchStatuses.at (index).failed())
        {
            Stdio::printError (branchStatuses.at (index).getErrorMessage());
            return Stdio::exitError;
        }
    }

    int totalMatches { 0 };

    for (const int count : branchMatchCounts)
        totalMatches += count;

    if (isQuiet)
        return totalMatches > 0 ? Stdio::exitSuccess : Stdio::exitMismatch;

    size_t totalMatchedFiles { 0 };

    for (const auto& branch : branchOutputs)
        totalMatchedFiles += branch.size();

    std::vector<std::pair<std::string, std::string>> matchedOutputs;
    matchedOutputs.reserve (totalMatchedFiles);

    for (auto& branch : branchOutputs)
    {
        for (auto& entry : branch)
            matchedOutputs.emplace_back (std::move (entry));
    }

    std::sort (matchedOutputs.begin(),
               matchedOutputs.end(),
               [] (const auto& firstEntry, const auto& secondEntry)
               {
                   const auto& [firstPath, firstOutput] { firstEntry };
                   const auto& [secondPath, secondOutput] { secondEntry };
                   juce::ignoreUnused (firstOutput, secondOutput);
                   return firstPath < secondPath;
               });

    for (const auto& [displayPath, output] : matchedOutputs)
    {
        juce::ignoreUnused (displayPath);
        Stdio::write (output);
    }

    return totalMatches > 0 ? Stdio::exitSuccess : Stdio::exitMismatch;
}

static int runFilesFind (jam::Strings& rawPaths,
                         const Pattern& pattern,
                         const Arguments& arguments,
                         int contextLines,
                         int afterLine,
                         juce::Range<int> lineRange)
{
    if (rawPaths.size() == 0)
        rawPaths.add (Id::currentDirectory);

    if (rawPaths.size() == 1 and canUseFastSearch (pattern, arguments, contextLines))
    {
        const auto rawPathView { rawPaths.getView (0) };
        const auto rawPathString { jam::Strings::fromView (rawPathView) };
        const auto resolved { juce::File::getCurrentWorkingDirectory().getChildFile (rawPathString).getLinkedTarget() };

        if (not resolved.exists())
        {
            Stdio::printError (juce::String::formatted (text::Diagnostics::failNotFound, rawPathString.toRawUTF8()));
            return Stdio::exitError;
        }

        if (resolved.isDirectory())
            return runFusedDirectoryFind (resolved, rawPathView, pattern, arguments, afterLine, lineRange);
    }

    const auto isShowingPathPrefix { Paths::hasMultipleOrDirectoryInputs (rawPaths) };
    const auto [pathsResult, files, isExplicitFlags] { Paths::getFiles (rawPaths,
                                                                        arguments.getValues (Id::includeOption),
                                                                        arguments.getValues (Id::excludeOption),
                                                                        arguments.contains (Id::hiddenOption),
                                                                        arguments.contains (Id::noIgnoreOption)) };

    if (pathsResult.failed())
    {
        Stdio::printError (pathsResult.getErrorMessage());
        return Stdio::exitError;
    }

    return runParallelFilesFind (files, isExplicitFlags, pattern, arguments, contextLines, afterLine, lineRange, isShowingPathPrefix);
}

static int runStdinFind (const Pattern& pattern,
                         const Arguments& arguments,
                         int contextLines,
                         int afterLine,
                         juce::Range<int> lineRange)
{
    auto [inputResult, stdinBytes] { Stdio::getInput() };

    if (inputResult.failed())
    {
        Stdio::printError (inputResult.getErrorMessage());
        return Stdio::exitError;
    }

    const auto document { TextDocument::fromString (std::move (stdinBytes), Id::stdinPath) };
    const auto [matchCount, output] { searchDocument (*document, pattern, arguments, contextLines, afterLine, lineRange, false) };

    if (not arguments.contains (Id::quietOption) and not output.empty())
        Stdio::write (output);

    return matchCount > 0 ? Stdio::exitSuccess : Stdio::exitMismatch;
}

static std::pair<juce::Result, int> getValidatedIntegerOption (const Arguments& arguments,
                                                               const juce::String& option,
                                                               int minimumValue,
                                                               int defaultValue)
{
    if (arguments.contains (option))
        return arguments.getInteger (option, minimumValue);

    return { juce::Result::ok(), defaultValue };
}

static int runWithValidatedOptions (const Arguments& arguments,
                                    const jam::Strings& positionals,
                                    int contextLines,
                                    int afterLine,
                                    juce::Range<int> lineRange)
{
    const auto [patternResult, pattern] { Pattern::create (positionals.at (0),
                                                           arguments.contains (Id::regexOption),
                                                           arguments.contains (Id::ignoreCaseOption),
                                                           arguments.contains (Id::exactOption)) };

    if (patternResult.failed())
    {
        Stdio::printError (patternResult.getErrorMessage());
        return Stdio::exitError;
    }

    jam::Strings rawPaths;

    for (int index { 1 }; index < positionals.size(); ++index)
        rawPaths.add (positionals.getView (index));

    if (rawPaths.size() == 0 and not Stdio::isTerminalInput())
        return runStdinFind (*pattern, arguments, contextLines, afterLine, lineRange);

    return runFilesFind (rawPaths, *pattern, arguments, contextLines, afterLine, lineRange);
}

static int runWithValidatedArguments (const Arguments& arguments)
{
    const auto& positionals { arguments.getPositionals() };

    if (positionals.size() < 1)
    {
        Stdio::printError (text::Diagnostics::failFindRequiresPattern);
        return Stdio::exitError;
    }

    const auto [contextResult, contextLines] { getValidatedIntegerOption (arguments, Id::contextOption, 0, 0) };
    const auto [afterResult, afterLine] { getValidatedIntegerOption (arguments, Id::afterOption, 0, 0) };
    const auto [linesResult, lineRange] { arguments.getLineRangeFilter() };

    if (contextResult.failed() or afterResult.failed() or linesResult.failed())
    {
        const auto& failedResult { contextResult.failed() ? contextResult : (afterResult.failed() ? afterResult : linesResult) };
        Stdio::printError (failedResult.getErrorMessage());
        return Stdio::exitError;
    }

    return runWithValidatedOptions (arguments, positionals, contextLines, afterLine, lineRange);
}

int Find::run (const jam::Strings& argumentList)
{
    const jam::Strings flagOptions
    {
        Id::regexOption, Id::ignoreCaseOption, Id::exactOption, Id::invertOption,
        Id::countOption, Id::quietOption, Id::lineNumberOption, Id::noLineNumberOption,
        Id::hashOption, Id::firstOption, Id::hiddenOption, Id::noIgnoreOption
    };
    const jam::Strings valueOptions
    {
        Id::contextOption, Id::afterOption, Id::linesOption, Id::includeOption, Id::excludeOption
    };
    const Arguments arguments { argumentList, 1, flagOptions, valueOptions };

    if (arguments.getStatus().failed())
    {
        Stdio::printError (arguments.getStatus().getErrorMessage());
        return Stdio::exitError;
    }

    return runWithValidatedArguments (arguments);
}
