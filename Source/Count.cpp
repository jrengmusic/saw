#include "Count.h"
#include "Paths.h"
#include "TextDocument.h"
#include "generated/Generated.h"

static size_t getWordCount (std::string_view bytes)
{
    size_t wordCount { 0 };
    bool isInWord { false };

    for (const auto byte : bytes)
    {
        const bool isSeparator { juce::CharacterFunctions::isWhitespace (byte) };
        wordCount += static_cast<size_t> (not isSeparator and not isInWord);
        isInWord = not isSeparator;
    }

    return wordCount;
}

static std::tuple<size_t, size_t, size_t> getCounts (std::string_view bytes)
{
    return { jam::simd::countByte (bytes.data(), bytes.size(), static_cast<char> (Chars::newline)), getWordCount (bytes), bytes.size() };
}

static std::tuple<bool, bool, bool> getSelection (const Arguments& arguments)
{
    const bool isAll { not arguments.contains (Id::countLinesOption) and not arguments.contains (Id::countWordsOption) and not arguments.contains (Id::countBytesOption) };
    return { isAll or arguments.contains (Id::countLinesOption), isAll or arguments.contains (Id::countWordsOption), isAll or arguments.contains (Id::countBytesOption) };
}

static bool isSingleCount (const std::tuple<bool, bool, bool>& selection)
{
    const auto& [isLines, isWords, isBytes] { selection };
    return static_cast<int> (isLines) + static_cast<int> (isWords) + static_cast<int> (isBytes) == 1;
}

static int getNumberWidth (size_t regularBytes, bool isStream, bool isSingleOutput)
{
    static constexpr int nonRegularMinimumWidth { 7 };

    if (isSingleOutput)
        return 1;

    const int digitCount { static_cast<int> (std::to_string (regularBytes).size()) };
    return isStream ? std::max (digitCount, nonRegularMinimumWidth) : digitCount;
}

static void appendCount (std::string& line, size_t count, int width, bool isSelected)
{
    if (isSelected)
    {
        const auto digits { std::to_string (count) };

        if (not line.empty())
            jam::Format::appendUtf8 (line, Chars::space);

        line.append (static_cast<size_t> (std::max (width - static_cast<int> (digits.size()), 0)), static_cast<char> (Chars::space));
        line += digits;
    }
}

static std::string getCountLine (const std::tuple<size_t, size_t, size_t>& counts, const std::tuple<bool, bool, bool>& selection, int width, std::string_view name)
{
    const auto& [lineCount, wordCount, byteCount] { counts };
    const auto& [isLines, isWords, isBytes] { selection };
    std::string line;
    appendCount (line, lineCount, width, isLines);
    appendCount (line, wordCount, width, isWords);
    appendCount (line, byteCount, width, isBytes);

    if (not name.empty())
    {
        jam::Format::appendUtf8 (line, Chars::space);
        line.append (name);
    }

    line.append (Stdio::newline);
    return line;
}

static Stdio::Output runStandardInput (const Arguments& arguments, const Input::Source& input)
{
    if (Stdio::isTerminalInput (input))
        return Stdio::getFailure (text::Diagnostics::failMissingPath);

    const auto [inputStatus, standardInput] { Input::create (input, Input::allLines) };

    if (inputStatus.failed())
        return Stdio::getFailure (inputStatus);

    const auto selection { getSelection (arguments) };
    const auto width { getNumberWidth (0, true, isSingleCount (selection)) };
    return Stdio::getOutput (Stdio::exitSuccess, Stdio::getDocuments (getCountLine (getCounts (standardInput->getBytes()), selection, width, {})));
}

static size_t getRegularBytes (const jam::Array<Paths::Entry>& files)
{
    size_t regularBytes { 0 };

    for (const auto& entry : files)
    {
        regularBytes += static_cast<size_t> (Paths::getFile (entry).getSize());
    }

    return regularBytes;
}

static Stdio::Output runFiles (const Arguments& arguments, juce::Span<const std::string_view> rawPaths)
{
    const auto [readStatus, files] { Paths::getReadableFiles (rawPaths, {}, {}, false, false) };

    if (readStatus.failed())
        return Stdio::getFailure (readStatus);

    const auto selection { getSelection (arguments) };
    const auto width { getNumberWidth (getRegularBytes (files), false, files.size() == 1 and isSingleCount (selection)) };
    Stdio::Documents documents;
    std::tuple<size_t, size_t, size_t> totals { 0, 0, 0 };
    auto& [totalLines, totalWords, totalBytes] { totals };

    for (const auto& entry : files)
    {
        const auto [inputStatus, input] { TextDocument::getInput (Paths::getFile (entry), Paths::getDisplayPath (entry), Input::maxBufferedReadSize) };

        if (inputStatus.failed())
            return Stdio::getFailure (inputStatus);

        const auto counts { getCounts (input->getBytes()) };
        const auto& [lineCount, wordCount, byteCount] { counts };
        totalLines += lineCount;
        totalWords += wordCount;
        totalBytes += byteCount;
        documents.add (TextDocument::fromText (getCountLine (counts, selection, width, Paths::getDisplayPath (entry)), {}));
    }

    if (files.size() > 1)
        documents.add (TextDocument::fromText (getCountLine (totals, selection, width, Id::totalValue), {}));

    return Stdio::getOutput (Stdio::exitSuccess, std::move (documents));
}

Arguments Count::getArguments (juce::Span<const std::string_view> argumentList)
{
    return Arguments { argumentList, { Id::countLinesOption, Id::countWordsOption, Id::countBytesOption }, {}, {} };
}

Stdio::Output Count::run (const Arguments& arguments, const Input::Source& input)
{
    if (arguments.getStatus().failed())
        return Stdio::getFailure (arguments.getStatus());

    if (arguments.getPositionals (0).empty())
        return runStandardInput (arguments, input);

    return runFiles (arguments, arguments.getPositionals (0));
}
