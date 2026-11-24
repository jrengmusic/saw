#include "Excerpt.h"
#include "Input.h"
#include "Jobs.h"
#include "Paths.h"
#include "Stdio.h"
#include "TextDocument.h"
#include "generated/Generated.h"

static void appendLine (std::string& output, int lineNumber, std::string_view line)
{
    constexpr unsigned char hexRadix { 16 };
    constexpr size_t maximumUtf8Length { 4 };
    jam::Format::appendDecimal (output, lineNumber);
    output.push_back (static_cast<char> (Chars::tab));

    if (juce::CharPointer_UTF8::isValidString (line.data(), static_cast<int> (line.size())))
    {
        jam::Format::appendBytes (output, line);
    }
    else
    {
        size_t index { 0 };

        while (index < line.size())
        {
            const size_t maximumLength { std::min (maximumUtf8Length, line.size() - index) };
            size_t length { 1 };

            while (length <= maximumLength and not juce::CharPointer_UTF8::isValidString (line.data() + index, static_cast<int> (length)))
                ++length;

            if (length <= maximumLength)
            {
                jam::Format::appendBytes (output, line.substr (index, length));
                index += length;
            }
            else
            {
                const auto byteValue { static_cast<unsigned char> (line.at (index)) };
                jam::Format::appendBytes (output, "\\x");

                if (byteValue < hexRadix)
                    output.push_back (static_cast<char> (Chars::zero));

                jam::Format::appendHex (output, byteValue);
                ++index;
            }
        }
    }

    output.push_back (static_cast<char> (Chars::newline));
}

static void appendBlock (std::string& output,
                         const TextDocument& document,
                         std::string_view path,
                         int targetNumber,
                         int startLine,
                         int endLine)
{
    constexpr int blockHashLength { 16 };
    const auto blockBytes { document.getLineBytes (juce::Range<int> { startLine, endLine + 1 }) };
    const auto blockHash { juce::SHA256 (blockBytes.data(), blockBytes.size()).toHexString().substring (0, blockHashLength) };
    const juce::String escapedMarker { juce::CharPointer_UTF8::isValidString (blockBytes.data(), static_cast<int> (blockBytes.size())) ? "" : text::Diagnostics::excerptEscaped };
    const auto header { Stdio::getFormatted (text::Diagnostics::excerptHeaderFormat,
                                             targetNumber,
                                             jam::Strings::fromView (path).toRawUTF8(),
                                             startLine,
                                             endLine,
                                             document.getNumLines(),
                                             blockHash.toRawUTF8(),
                                             escapedMarker.toRawUTF8()) };
    jam::Format::appendBytes (output, jam::Strings::getView (header));

    for (int lineNumber { startLine }; lineNumber <= endLine; ++lineNumber)
        appendLine (output, lineNumber, document.getLine (lineNumber));
}

static std::pair<bool, std::string> getStateBlock (int targetNumber, std::string_view path, const char* state)
{
    const auto header { Stdio::getFormatted (text::Diagnostics::excerptStateFormat,
                                             targetNumber,
                                             jam::Strings::fromView (path).toRawUTF8(),
                                             state) };
    std::string output;
    jam::Format::appendBytes (output, jam::Strings::getView (header));
    return { false, std::move (output) };
}

static std::pair<const char*, std::unique_ptr<TextDocument>> getLoadedDocument (std::string_view path)
{
    const auto displayPath { jam::Strings::fromView (path) };
    const auto targetFile { Paths::getLinkedTarget (path) };

    if (not targetFile.exists())
        return { text::Diagnostics::excerptNotFound, nullptr };

    auto [documentStatus, document] { TextDocument::fromFile (targetFile, displayPath) };

    if (documentStatus.failed())
        return { text::Diagnostics::excerptUnreadable, nullptr };

    if (document->isBinary())
        return { text::Diagnostics::excerptBinary, nullptr };

    if (document->getBytes().empty())
        return { text::Diagnostics::excerptEmpty, nullptr };

    return { nullptr, std::move (document) };
}

static std::pair<bool, std::string> getLiteralBlock (int targetNumber,
                                                     std::string_view path,
                                                     std::string_view literal,
                                                     int contextLines)
{
    auto [documentState, loadedDocument] { getLoadedDocument (path) };

    if (documentState != nullptr)
        return getStateBlock (targetNumber, path, documentState);

    const TextDocument& document { *loadedDocument };
    const int numLines { document.getNumLines() };
    const int contextSpan { std::min (contextLines, numLines) };
    std::string output;
    int pendingStart { 0 };
    int pendingEnd { 0 };

    for (int lineNumber { 1 }; lineNumber <= numLines; ++lineNumber)
    {
        if (not literal.empty() and document.getLine (lineNumber).find (literal) != std::string_view::npos)
        {
            const int start { std::max (1, lineNumber - contextSpan) };
            const int end { std::min (numLines, lineNumber + contextSpan) };

            if (pendingEnd == 0)
            {
                pendingStart = start;
                pendingEnd = end;
            }
            else if (start <= pendingEnd + 1)
            {
                pendingEnd = end;
            }
            else
            {
                appendBlock (output, document, path, targetNumber, pendingStart, pendingEnd);
                pendingStart = start;
                pendingEnd = end;
            }
        }
    }

    if (pendingEnd == 0)
        return getStateBlock (targetNumber, path, text::Diagnostics::excerptNoMatch);

    appendBlock (output, document, path, targetNumber, pendingStart, pendingEnd);
    return { true, std::move (output) };
}

static bool isDigits (std::string_view text)
{
    if (text.empty())
        return false;

    for (const char character : text)
    {
        if (character < static_cast<char> (Chars::zero) or character > static_cast<char> (Chars::nine))
            return false;
    }

    return true;
}

static std::optional<int> getLineNumber (std::string_view digits)
{
    int lineNumber { 0 };
    const auto parseResult { std::from_chars (digits.data(), digits.data() + digits.size(), lineNumber) };

    if (parseResult.ec == std::errc {})
        return lineNumber;

    return std::nullopt;
}

static bool isSpanText (std::string_view suffix)
{
    const auto dashIndex { suffix.find (static_cast<char> (Chars::dash)) };
    return isDigits (suffix.substr (0, dashIndex))
           and (dashIndex == std::string_view::npos or isDigits (suffix.substr (dashIndex + 1)));
}

static std::optional<std::pair<int, int>> getSpan (std::string_view spanText)
{
    if (spanText.empty())
        return std::pair<int, int> { 1, Arguments::openRangeEnd };

    const auto dashIndex { spanText.find (static_cast<char> (Chars::dash)) };
    const auto firstLine { getLineNumber (spanText.substr (0, dashIndex)) };
    const auto lastLine { dashIndex == std::string_view::npos ? firstLine : getLineNumber (spanText.substr (dashIndex + 1)) };

    if (firstLine and lastLine and *firstLine >= 1 and *lastLine >= *firstLine)
        return std::pair<int, int> { *firstLine, *lastLine };

    return std::nullopt;
}

static std::pair<bool, std::string> getRangeBlock (int targetNumber, std::string_view target)
{
    const auto colonIndex { target.rfind (static_cast<char> (Chars::colon)) };
    const auto suffix { colonIndex == std::string_view::npos ? std::string_view {} : target.substr (colonIndex + 1) };
    const auto isSpan { isSpanText (suffix) };
    const auto path { isSpan ? target.substr (0, colonIndex) : target };
    const auto span { getSpan (isSpan ? suffix : std::string_view {}) };

    if (not span)
        return getStateBlock (targetNumber, path, text::Diagnostics::excerptOutOfRange);

    auto [documentState, loadedDocument] { getLoadedDocument (path) };

    if (documentState != nullptr)
        return getStateBlock (targetNumber, path, documentState);

    const TextDocument& document { *loadedDocument };
    const auto [firstLine, lastLine] { *span };
    const int numLines { document.getNumLines() };

    if (firstLine > numLines)
        return getStateBlock (targetNumber, path, text::Diagnostics::excerptOutOfRange);

    std::string output;
    appendBlock (output, document, path, targetNumber, firstLine, std::min (lastLine, numLines));
    return { true, std::move (output) };
}

static std::pair<bool, std::string> getTargetBlock (int targetNumber, std::string_view target, int contextLines)
{
    constexpr std::string_view literalSeparator { " ~ " };
    const auto separatorIndex { target.find (literalSeparator) };

    if (separatorIndex == std::string_view::npos)
        return getRangeBlock (targetNumber, target);

    return getLiteralBlock (targetNumber,
                            target.substr (0, separatorIndex),
                            target.substr (separatorIndex + literalSeparator.size()),
                            contextLines);
}

static std::pair<juce::Result, jam::Strings> getTargets (const Arguments& arguments, const std::optional<std::string>& input)
{
    jam::Strings targets;
    targets.addArray (arguments.getPositionals(), 0, -1);

    if (targets.size() == 0)
    {
        if (Stdio::isTerminalInput (input))
            return { juce::Result::fail (text::Diagnostics::failExcerptRequiresTarget), {} };

        const auto [inputStatus, standardInput] { Input::create (input, Input::allLines) };

        if (inputStatus.failed())
            return { inputStatus, {} };

        const auto inputText { standardInput->getBytes() };
        size_t cursor { 0 };

        while (cursor < inputText.size())
        {
            const auto newlineIndex { jam::simd::findByte (inputText.data(), inputText.size(), cursor, static_cast<char> (Chars::newline)) };
            const auto line { TextDocument::getLine (inputText, cursor, newlineIndex) };

            if (not line.empty())
                targets.add (line);

            cursor = newlineIndex < inputText.size() ? newlineIndex + 1 : inputText.size();
        }
    }

    if (targets.size() == 0)
        return { juce::Result::fail (text::Diagnostics::failExcerptRequiresTarget), {} };

    return { juce::Result::ok(), std::move (targets) };
}

static Stdio::Output getOutput (const jam::Array<std::string>& blocks, const jam::Array<bool>& isFoundFlags)
{
    const int targetCount { blocks.size() };
    int foundCount { 0 };

    for (int index { 0 }; index < targetCount; ++index)
    {
        if (isFoundFlags.at (index))
            ++foundCount;
    }

    std::string output;
    const auto receipt { Stdio::getFormatted (text::Diagnostics::excerptReceiptFormat, targetCount, foundCount, targetCount - foundCount) };
    jam::Format::appendBytes (output, jam::Strings::getView (receipt));

    for (int index { 0 }; index < targetCount; ++index)
        jam::Format::appendBytes (output, blocks.at (index));

    const int exitCode { foundCount == targetCount ? Stdio::exitSuccess : Stdio::exitMismatch };
    return { exitCode, std::move (output), {} };
}

Arguments Excerpt::getArguments (const jam::Strings& argumentList)
{
    const std::vector<std::string_view> flagOptions {};
    const std::vector<std::string_view> valueOptions { Id::contextOption };
    const std::vector<std::string_view> optionalValueOptions {};

    return Arguments { argumentList, flagOptions, valueOptions, optionalValueOptions };
}

Stdio::Output Excerpt::run (const Arguments& arguments, const std::optional<std::string>& input)
{
    if (arguments.getStatus().failed())
        return Stdio::getFailure (arguments.getStatus().getErrorMessage());

    const auto [contextStatus, contextLines] { arguments.getInteger (Id::contextOption, 0, 0) };

    if (contextStatus.failed())
        return Stdio::getFailure (contextStatus.getErrorMessage());

    const auto [targetStatus, targets] { getTargets (arguments, input) };

    if (targetStatus.failed())
        return Stdio::getFailure (targetStatus.getErrorMessage());

    const int targetCount { targets.size() };
    jam::Array<std::string> blocks;
    blocks.resize (targetCount);
    jam::Array<bool> isFoundFlags;
    isFoundFlags.insertMultiple (0, false, targetCount);

    Jobs::run (targetCount, [&targetTexts = targets, contextLines = contextLines, &blocks, &isFoundFlags] (int index)
               {
                   auto [isFound, block] { getTargetBlock (index + 1, targetTexts.getView (index), contextLines) };
                   isFoundFlags.at (index) = isFound;
                   blocks.at (index) = std::move (block);
               });

    return getOutput (blocks, isFoundFlags);
}
