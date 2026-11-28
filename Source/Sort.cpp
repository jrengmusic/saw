#include "Sort.h"
#include "Paths.h"
#include "TextDocument.h"
#include "generated/Generated.h"

static size_t getBlankEnd (std::string_view text)
{
    size_t blankEnd { 0 };

    while (blankEnd < text.size() and (text.at (blankEnd) == static_cast<char> (Chars::space) or text.at (blankEnd) == static_cast<char> (Chars::tab)))
        ++blankEnd;

    return blankEnd;
}

static size_t getDigitsEnd (std::string_view text, size_t start)
{
    size_t digitsEnd { start };

    while (digitsEnd < text.size() and juce::CharacterFunctions::isDigit (text.at (digitsEnd)))
        ++digitsEnd;

    return digitsEnd;
}

static std::string_view getWithoutLeadingZeros (std::string_view digits)
{
    while (not digits.empty() and digits.front() == static_cast<char> (Chars::zero))
        digits.remove_prefix (1);

    return digits;
}

static std::string_view getWithoutTrailingZeros (std::string_view digits)
{
    while (not digits.empty() and digits.back() == static_cast<char> (Chars::zero))
        digits.remove_suffix (1);

    return digits;
}

static std::tuple<bool, std::string_view, std::string_view> getNumericParts (std::string_view text)
{
    const size_t blankEnd { getBlankEnd (text) };
    const bool hasMinus { blankEnd < text.size() and text.at (blankEnd) == static_cast<char> (Chars::dash) };
    const size_t integerStart { hasMinus ? blankEnd + 1 : blankEnd };
    const size_t integerEnd { getDigitsEnd (text, integerStart) };
    const bool hasPoint { integerEnd < text.size() and text.at (integerEnd) == static_cast<char> (Chars::dot) };
    const size_t fractionStart { hasPoint ? integerEnd + 1 : integerEnd };
    const size_t fractionEnd { getDigitsEnd (text, fractionStart) };
    const auto integerDigits { getWithoutLeadingZeros (text.substr (integerStart, integerEnd - integerStart)) };
    const auto fractionDigits { getWithoutTrailingZeros (text.substr (fractionStart, fractionEnd - fractionStart)) };
    return { hasMinus and (not integerDigits.empty() or not fractionDigits.empty()), integerDigits, fractionDigits };
}

static int getMagnitudeOrder (std::string_view firstInteger, std::string_view firstFraction, std::string_view secondInteger, std::string_view secondFraction)
{
    if (firstInteger.size() != secondInteger.size())
        return firstInteger.size() < secondInteger.size() ? -1 : 1;

    const int integerOrder { firstInteger.compare (secondInteger) };
    return integerOrder != 0 ? integerOrder : firstFraction.compare (secondFraction);
}

static int compareNumeric (std::string_view first, std::string_view second)
{
    const auto [isFirstNegative, firstInteger, firstFraction] { getNumericParts (first) };
    const auto [isSecondNegative, secondInteger, secondFraction] { getNumericParts (second) };

    if (isFirstNegative != isSecondNegative)
        return isFirstNegative ? -1 : 1;

    const int magnitudeOrder { getMagnitudeOrder (firstInteger, firstFraction, secondInteger, secondFraction) };
    return isFirstNegative ? -magnitudeOrder : magnitudeOrder;
}

static int getOrder (std::string_view first, std::string_view second, bool isNumeric, bool isUnique)
{
    const int numericOrder { isNumeric ? compareNumeric (first, second) : 0 };

    if (isNumeric and (numericOrder != 0 or isUnique))
        return numericOrder;

    return first.compare (second);
}

static int getSortOrder (std::string_view first, std::string_view second, bool isNumeric, bool isReverse, bool isUnique)
{
    const int order { getOrder (first, second, isNumeric, isUnique) };
    return isReverse ? -order : order;
}

static std::string_view getLineBody (std::string_view line)
{
    const bool hasNewline { line.back() == static_cast<char> (Chars::newline) };
    return line.substr (0, line.size() - static_cast<size_t> (hasNewline));
}

static void addLines (jam::Array<std::string_view>& lines, std::string_view bytes)
{
    size_t lineStart { 0 };

    while (lineStart < bytes.size())
    {
        const size_t lineEnd { std::min (jam::simd::findByte (bytes.data(), bytes.size(), lineStart, static_cast<char> (Chars::newline)) + 1, bytes.size()) };
        lines.add (bytes.substr (lineStart, lineEnd - lineStart));
        lineStart = lineEnd;
    }
}

static void addInputLines (Stdio::Documents& documents, jam::Array<std::string_view>& lines, std::unique_ptr<Input> input)
{
    addLines (lines, input->getBytes());
    documents.add (TextDocument::fromInput (std::move (input), {}, {}));
}

static void appendLine (Stdio::Documents& documents, std::string_view line)
{
    documents.add (TextDocument::fromView (line, {}));

    if (line.back() != static_cast<char> (Chars::newline))
        documents.add (TextDocument::fromView (Stdio::newline, {}));
}

static void appendEachLine (Stdio::Documents& documents, const jam::Array<std::string_view>& lines)
{
    for (const auto& line : lines)
    {
        appendLine (documents, line);
    }
}

static std::string getCountPrefix (size_t count)
{
    static constexpr int countWidth { 7 };

    const auto digits { std::to_string (count) };
    std::string prefix;

    prefix.append (static_cast<size_t> (std::max (countWidth - static_cast<int> (digits.size()), 0)), static_cast<char> (Chars::space));
    prefix += digits;
    jam::Format::appendUtf8 (prefix, Chars::space);
    return prefix;
}

static void appendCountedLines (Stdio::Documents& documents, const jam::Array<std::string_view>& lines)
{
    int groupStart { 0 };

    for (int index { 0 }; index < lines.size(); ++index)
    {
        const bool isGroupEnd { index + 1 == lines.size() or getLineBody (lines.at (index + 1)).compare (getLineBody (lines.at (index))) != 0 };

        if (isGroupEnd)
        {
            documents.add (TextDocument::fromText (getCountPrefix (static_cast<size_t> (index + 1 - groupStart)), {}));
            appendLine (documents, lines.at (groupStart));
            groupStart = index + 1;
        }
    }
}

static Stdio::Output getSortedOutput (const Arguments& arguments, Stdio::Documents& documents, jam::Array<std::string_view>& lines)
{
    const bool isNumeric { arguments.contains (Id::sortNumericOption) };
    const bool isReverse { arguments.contains (Id::sortReverseOption) };
    const bool isUnique { arguments.contains (Id::sortUniqueOption) };
    const bool isCounting { arguments.contains (Id::sortCountOption) };

    std::stable_sort (lines.begin(), lines.end(), [isNumeric, isReverse, isUnique] (std::string_view first, std::string_view second)
    {
        return getSortOrder (getLineBody (first), getLineBody (second), isNumeric, isReverse, isUnique) < 0;
    });

    if (isUnique)
    {
        const auto uniqueEnd { std::unique (lines.begin(), lines.end(), [isNumeric] (std::string_view first, std::string_view second)
        {
            return getOrder (getLineBody (first), getLineBody (second), isNumeric, true) == 0;
        }) };
        lines.resize (static_cast<int> (uniqueEnd - lines.begin()));
    }

    if (isCounting)
        appendCountedLines (documents, lines);
    else
        appendEachLine (documents, lines);

    return Stdio::getOutput (Stdio::exitSuccess, std::move (documents));
}

static Stdio::Output runStandardInput (const Arguments& arguments, const Input::Source& input)
{
    if (Stdio::isTerminalInput (input))
        return Stdio::getFailure (text::Diagnostics::failMissingPath);

    auto [inputStatus, standardInput] { Input::create (input, Input::allLines) };

    if (inputStatus.failed())
        return Stdio::getFailure (inputStatus);

    Stdio::Documents documents;
    jam::Array<std::string_view> lines;
    addInputLines (documents, lines, std::move (standardInput));
    return getSortedOutput (arguments, documents, lines);
}

static Stdio::Output runFiles (const Arguments& arguments, juce::Span<const std::string_view> rawPaths)
{
    const auto [readStatus, files] { Paths::getReadableFiles (rawPaths, {}, {}, false, false) };

    if (readStatus.failed())
        return Stdio::getFailure (readStatus);

    Stdio::Documents documents;
    jam::Array<std::string_view> lines;

    for (const auto& entry : files)
    {
        auto [inputStatus, input] { TextDocument::getInput (Paths::getFile (entry), Paths::getDisplayPath (entry), Input::maxBufferedReadSize) };

        if (inputStatus.failed())
            return Stdio::getFailure (inputStatus);

        addInputLines (documents, lines, std::move (input));
    }

    return getSortedOutput (arguments, documents, lines);
}

Arguments Sort::getArguments (juce::Span<const std::string_view> argumentList)
{
    return Arguments { argumentList, { Id::sortNumericOption, Id::sortReverseOption, Id::sortUniqueOption, Id::sortCountOption }, {}, {} };
}

Stdio::Output Sort::run (const Arguments& arguments, const Input::Source& input)
{
    if (arguments.getStatus().failed())
        return Stdio::getFailure (arguments.getStatus());

    if (arguments.getPositionals (0).empty())
        return runStandardInput (arguments, input);

    return runFiles (arguments, arguments.getPositionals (0));
}
