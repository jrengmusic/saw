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

static std::tuple<size_t, size_t, size_t> getCounts (std::string_view bytes, const std::tuple<bool, bool, bool>& selection)
{
    const auto& [isLines, isWords, isBytes] { selection };
    juce::ignoreUnused (isBytes);
    return { isLines ? jam::simd::countByte (bytes.data(), bytes.size(), static_cast<char> (Chars::newline)) : size_t { 0 }, isWords ? getWordCount (bytes) : size_t { 0 }, bytes.size() };
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

static constexpr size_t maxCountDigits { 24 };

static int getNumberWidth (size_t regularBytes, bool isStream, bool isSingleOutput)
{
    static constexpr int nonRegularMinimumWidth { 7 };

    if (isSingleOutput)
        return 1;

    std::array<char, maxCountDigits> digits {};
    [[maybe_unused]] const auto [end, error] { std::to_chars (digits.data(), std::next (digits.data(), static_cast<std::ptrdiff_t> (digits.size())), regularBytes) };
    const int digitCount { static_cast<int> (end - digits.data()) };
    return isStream ? std::max (digitCount, nonRegularMinimumWidth) : digitCount;
}

static void appendCountLine (std::string& output, const std::tuple<size_t, size_t, size_t>& counts, const std::tuple<bool, bool, bool>& selection, int width, std::string_view name)
{
    const auto& [lineCount, wordCount, byteCount] { counts };
    const auto& [isLines, isWords, isBytes] { selection };
    const size_t lineStart { output.size() };
    const auto appendField { [&output, lineStart, width] (size_t count, bool isSelected)
    {
        if (isSelected)
        {
            std::array<char, maxCountDigits> digits {};
            [[maybe_unused]] const auto [end, error] { std::to_chars (digits.data(), std::next (digits.data(), static_cast<std::ptrdiff_t> (digits.size())), count) };
            const std::string_view digitView { digits.data(), static_cast<size_t> (end - digits.data()) };

            if (output.size() > lineStart)
                jam::Format::appendUtf8 (output, Chars::space);

            output.append (static_cast<size_t> (std::max (width - static_cast<int> (digitView.size()), 0)), static_cast<char> (Chars::space));
            jam::Format::appendBytes (output, digitView);
        }
    } };
    appendField (lineCount, isLines);
    appendField (wordCount, isWords);
    appendField (byteCount, isBytes);

    if (not name.empty())
    {
        jam::Format::appendUtf8 (output, Chars::space);
        jam::Format::appendBytes (output, name);
    }

    jam::Format::appendBytes (output, Stdio::newline);
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
    std::string output;
    appendCountLine (output, getCounts (standardInput->getBytes(), selection), selection, width, {});
    return Stdio::getOutput (Stdio::exitSuccess, Stdio::getDocuments (std::move (output)));
}

static Stdio::Output runFiles (const Arguments& arguments, juce::Span<const std::string_view> rawPaths)
{
    const auto [readStatus, files] { Paths::getReadableFiles (rawPaths, {}, {}, false, false) };

    if (readStatus.failed())
        return Stdio::getFailure (readStatus);

    const auto selection { getSelection (arguments) };
    jam::Array<std::tuple<size_t, size_t, size_t>> fileCounts;
    fileCounts.ensureStorageAllocated (files.size());
    std::tuple<size_t, size_t, size_t> totals { 0, 0, 0 };
    auto& [totalLines, totalWords, totalBytes] { totals };

    for (const auto& entry : files)
    {
        const auto [inputStatus, input] { TextDocument::getInput (Paths::getFile (entry), Paths::getDisplayPath (entry), Input::maxBufferedReadSize) };

        if (inputStatus.failed())
            return Stdio::getFailure (inputStatus);

        const auto counts { getCounts (input->getBytes(), selection) };
        const auto& [lineCount, wordCount, byteCount] { counts };
        totalLines += lineCount;
        totalWords += wordCount;
        totalBytes += byteCount;
        fileCounts.add (counts);
    }

    const auto width { getNumberWidth (totalBytes, false, files.size() == 1 and isSingleCount (selection)) };
    std::string output;

    for (int index { 0 }; index < files.size(); ++index)
        appendCountLine (output, fileCounts.at (index), selection, width, Paths::getDisplayPath (files.at (index)));

    if (files.size() > 1)
        appendCountLine (output, totals, selection, width, Id::totalValue);

    return Stdio::getOutput (Stdio::exitSuccess, Stdio::getDocuments (std::move (output)));
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
