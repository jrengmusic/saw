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
    jam::Format::appendUtf8 (output, Chars::tab);

    if (jam::Utf8::isValid (line))
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

            while (length <= maximumLength and not jam::Utf8::isValid (line.substr (index, length)))
                ++length;

            if (length <= maximumLength)
            {
                jam::Format::appendBytes (output, line.substr (index, length));
                index += length;
            }
            else
            {
                const auto byteValue { static_cast<unsigned char> (line.at (index)) };
                jam::Format::appendBytes (output, jam::Strings::getView (Id::hexEscapePrefix.toString()));

                if (byteValue < hexRadix)
                    jam::Format::appendUtf8 (output, Chars::zero);

                jam::Format::appendHex (output, byteValue);
                ++index;
            }
        }
    }

    jam::Format::appendUtf8 (output, Chars::newline);
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
    const std::string_view escapedMarker { jam::Utf8::isValid (blockBytes) ? std::string_view {} : std::string_view { text::Diagnostics::excerptEscaped } };
    jam::Format::appendBytes (output, text::Formats::excerptHeader);
    jam::Format::appendDecimal (output, targetNumber);
    jam::Format::appendUtf8 (output, Chars::space);
    jam::Format::appendBytes (output, path);
    jam::Format::appendUtf8 (output, Chars::colon);
    jam::Format::appendDecimal (output, startLine);
    jam::Format::appendUtf8 (output, Chars::dash);
    jam::Format::appendDecimal (output, endLine);
    jam::Format::appendBytes (output, text::Formats::excerptOf);
    jam::Format::appendDecimal (output, document.getNumLines());
    jam::Format::appendBytes (output, text::Formats::excerptHash);
    jam::Format::appendBytes (output, jam::Strings::getView (TextDocument::getContentHash (blockBytes).substring (0, blockHashLength)));
    jam::Format::appendBytes (output, escapedMarker);
    jam::Format::appendUtf8 (output, Chars::newline);

    for (int lineNumber { startLine }; lineNumber <= endLine; ++lineNumber)
        appendLine (output, lineNumber, document.getLine (lineNumber));
}

static std::pair<bool, Stdio::Documents> getStateBlock (int targetNumber, std::string_view path, std::string_view state)
{
    std::string output;
    jam::Format::appendBytes (output, text::Formats::excerptHeader);
    jam::Format::appendDecimal (output, targetNumber);
    jam::Format::appendUtf8 (output, Chars::space);
    jam::Format::appendBytes (output, path);
    jam::Format::appendUtf8 (output, Chars::space);
    jam::Format::appendBytes (output, state);
    jam::Format::appendUtf8 (output, Chars::newline);
    return { false, Stdio::getDocuments (std::move (output), path) };
}

static std::pair<std::string_view, std::unique_ptr<TextDocument>> getLoadedDocument (std::string_view path)
{
    const auto targetFile { Paths::getLinkedTarget (path) };

    if (not targetFile.exists())
        return { text::Diagnostics::excerptNotFound, nullptr };

    auto [documentStatus, document] { TextDocument::fromFile (targetFile, path, Input::maxBufferedReadSize) };

    if (documentStatus.failed())
        return { text::Diagnostics::excerptUnreadable, nullptr };

    if (document->isBinary())
        return { text::Diagnostics::excerptBinary, nullptr };

    if (document->getBytes().empty())
        return { text::Diagnostics::excerptEmpty, nullptr };

    return { std::string_view {}, std::move (document) };
}

static std::pair<bool, Stdio::Documents> getLiteralBlock (int targetNumber,
                                                          std::string_view path,
                                                          std::string_view literal,
                                                          int contextLines)
{
    auto [documentState, loadedDocument] { getLoadedDocument (path) };

    if (not documentState.empty())
        return getStateBlock (targetNumber, path, documentState);

    const TextDocument& document { *loadedDocument };
    const std::string_view bytes { document.getBytes() };
    const int numLines { document.getNumLines() };
    const int contextSpan { std::min (contextLines, numLines) };
    const auto literalSet { literal.empty() ? nullptr : jam::LiteralSet::create (std::vector<std::string> { std::string { literal } }, false) };
    std::string output;
    int pendingStart { 0 };
    int pendingEnd { 0 };
    size_t matchOffset { literalSet == nullptr ? bytes.size() : literalSet->findNext (bytes, 0) };

    while (matchOffset < bytes.size())
    {
        const int lineNumber { document.getLineNumber (matchOffset) };

        if (document.getLine (lineNumber).find (literal) != std::string_view::npos)
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

        const size_t nextLineOffset { lineNumber < numLines
                                          ? static_cast<size_t> (document.getLine (lineNumber + 1).data() - bytes.data())
                                          : bytes.size() };
        matchOffset = literalSet->findNext (bytes, nextLineOffset);
    }

    if (pendingEnd == 0)
        return getStateBlock (targetNumber, path, text::Diagnostics::excerptNoMatch);

    appendBlock (output, document, path, targetNumber, pendingStart, pendingEnd);
    return { true, Stdio::getDocuments (std::move (output), path) };
}

static bool isDigits (std::string_view text)
{
    if (text.empty())
        return false;

    for (const char character : text)
    {
        if (not jam::Document::isDigit (static_cast<unsigned char> (character)))
            return false;
    }

    return true;
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
    const auto firstLine { Arguments::getInteger (spanText.substr (0, dashIndex)) };
    const auto lastLine { dashIndex == std::string_view::npos ? firstLine : Arguments::getInteger (spanText.substr (dashIndex + 1)) };

    if (firstLine and lastLine and *firstLine >= 1 and *lastLine >= *firstLine)
        return std::pair<int, int> { *firstLine, *lastLine };

    return std::nullopt;
}

static std::pair<bool, Stdio::Documents> getRangeBlock (int targetNumber, std::string_view target)
{
    const auto colonIndex { target.rfind (static_cast<char> (Chars::colon)) };
    const auto suffix { colonIndex == std::string_view::npos ? std::string_view {} : target.substr (colonIndex + 1) };
    const auto isSpan { isSpanText (suffix) };
    const auto path { isSpan ? target.substr (0, colonIndex) : target };
    const auto span { getSpan (isSpan ? suffix : std::string_view {}) };

    if (not span)
        return getStateBlock (targetNumber, path, text::Diagnostics::excerptOutOfRange);

    auto [documentState, loadedDocument] { getLoadedDocument (path) };

    if (not documentState.empty())
        return getStateBlock (targetNumber, path, documentState);

    const TextDocument& document { *loadedDocument };
    const auto [firstLine, lastLine] { *span };
    const int numLines { document.getNumLines() };

    if (firstLine > numLines)
        return getStateBlock (targetNumber, path, text::Diagnostics::excerptOutOfRange);

    std::string output;
    appendBlock (output, document, path, targetNumber, firstLine, std::min (lastLine, numLines));
    return { true, Stdio::getDocuments (std::move (output), path) };
}

static std::pair<bool, Stdio::Documents> getTargetBlock (int targetNumber, std::string_view target, int contextLines)
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

static std::tuple<juce::Result, std::unique_ptr<Input>, jam::Array<std::string_view>> getTargets (const std::optional<std::string>& input)
{
    if (Stdio::isTerminalInput (input))
        return { Stdio::fail (text::Diagnostics::failExcerptRequiresTarget), nullptr, jam::Array<std::string_view> {} };

    auto [inputStatus, standardInput] { Input::create (input, Input::allLines) };

    if (inputStatus.failed())
        return { inputStatus, nullptr, jam::Array<std::string_view> {} };

    jam::Array<std::string_view> targets;
    const auto inputText { standardInput->getBytes() };

    TextDocument::applyToLines (inputText, juce::Range<int> (1, Input::allLines), [&targets] (int, std::string_view line)
    {
        if (not line.empty())
            targets.add (line);

        return true;
    });

    if (targets.isEmpty())
        return { Stdio::fail (text::Diagnostics::failExcerptRequiresTarget), nullptr, jam::Array<std::string_view> {} };

    return { juce::Result::ok(), std::move (standardInput), std::move (targets) };
}

static Stdio::Output getOutput (juce::Span<const std::string_view> targets, int contextLines)
{
    const int targetCount { static_cast<int> (targets.size()) };
    jam::Array<bool> foundFlags;
    foundFlags.insertMultiple (0, false, targetCount);
    jam::Array<Stdio::Documents> blockDocuments;
    blockDocuments.resize (targetCount);

    Jobs::run (targetCount, [targets, contextLines, &foundFlags, &blockDocuments] (int index)
               {
                   auto [isFound, documents] { getTargetBlock (index + 1, targets[static_cast<size_t> (index)], contextLines) };
                   foundFlags.at (index) = isFound;
                   blockDocuments.at (index) = std::move (documents);
               });

    int foundCount { 0 };

    for (int index { 0 }; index < targetCount; ++index)
    {
        if (foundFlags.at (index))
            ++foundCount;
    }

    std::string receipt;
    jam::Format::appendBytes (receipt, text::Formats::excerptReceipt);
    jam::Format::appendDecimal (receipt, targetCount);
    jam::Format::appendBytes (receipt, text::Formats::excerptTargets);
    jam::Format::appendDecimal (receipt, foundCount);
    jam::Format::appendBytes (receipt, text::Formats::excerptFound);
    jam::Format::appendDecimal (receipt, targetCount - foundCount);
    jam::Format::appendBytes (receipt, text::Formats::excerptMissing);

    Stdio::Documents documents (Stdio::getDocuments (std::move (receipt)));

    for (auto& targetDocuments : blockDocuments)
        Stdio::addDocuments (documents, std::move (targetDocuments));

    const int exitCode { foundCount == targetCount ? Stdio::exitSuccess : Stdio::exitMismatch };
    return Stdio::getOutput (exitCode, std::move (documents));
}

Arguments Excerpt::getArguments (juce::Span<const std::string_view> argumentList)
{
    return Arguments { argumentList, {}, { Id::contextOption }, {} };
}

Stdio::Output Excerpt::run (const Arguments& arguments, const std::optional<std::string>& input)
{
    if (arguments.getStatus().failed())
        return Stdio::getFailure (arguments.getStatus());

    const auto [contextStatus, contextLines] { arguments.getInteger (Id::contextOption, 0, 0) };

    if (contextStatus.failed())
        return Stdio::getFailure (contextStatus);

    if (arguments.getPositionals (0).empty())
    {
        const auto [targetStatus, standardInput, standardTargets] { getTargets (input) };

        if (targetStatus.failed())
            return Stdio::getFailure (targetStatus);

        return getOutput (standardTargets, contextLines);
    }

    return getOutput (arguments.getPositionals (0), contextLines);
}
