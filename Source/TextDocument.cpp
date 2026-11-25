#include "TextDocument.h"
#include "Stdio.h"
#include "generated/Generated.h"

TextDocument::TextDocument (Access,
                            std::unique_ptr<Input> newInput,
                            std::string_view newDisplayPath)
    : input (std::move (newInput))
{
    getRoot()->add<juce::String> (Id::path, jam::Strings::fromView (newDisplayPath));
}

const jam::Document::Vocabulary& TextDocument::getVocabulary() const
{
    return getPlainTextVocabulary();
}

size_t TextDocument::getToken (Cursor& cursor, int)
{
    return cursor.source.size() - cursor.getOffset();
}

void TextDocument::build()
{
}

juce::Result TextDocument::getReadFailure (const juce::File& file, std::string_view displayPath)
{
    if (file.exists())
        return Stdio::fail (text::Diagnostics::failRead, displayPath);

    return Stdio::fail (text::Diagnostics::failNotFound, displayPath);
}

juce::Result TextDocument::getBinaryFailure (std::string_view displayPath)
{
    return Stdio::fail (text::Diagnostics::failBinaryFile, displayPath);
}

juce::Result TextDocument::getBinaryStatus (std::string_view displayPath, bool isExplicit)
{
    if (isExplicit)
        return getBinaryFailure (displayPath);

    return juce::Result::ok();
}

bool TextDocument::isBinary (std::string_view bytes)
{
    return jam::simd::findByte (bytes.data(), bytes.size(), 0, '\0') < bytes.size();
}

juce::Result TextDocument::getBytesStatus (const juce::File& file,
                                           std::string_view displayPath,
                                           std::string_view expectedBytes,
                                           std::string_view failure)
{
    const auto [readStatus, fileInput] { getInput (file, displayPath, Input::maxBufferedReadSize) };

    if (readStatus.failed())
        return readStatus;

    if (fileInput->getBytes().compare (expectedBytes) != 0)
        return Stdio::fail (failure);

    return juce::Result::ok();
}

juce::Result TextDocument::getLineHashStatus (int lineNumber, std::string_view expectedHash) const
{
    const auto actualHash { lineNumber >= 1 ? getLineHash (lineNumber) : juce::String {} };

    if (lineNumber >= 1 and jam::Strings::getView (actualHash).compare (expectedHash) == 0)
        return juce::Result::ok();

    return Stdio::fail (text::Diagnostics::failHashMismatch,
                        getDisplayPath(),
                        Chars::colon,
                        lineNumber,
                        text::Diagnostics::hashExpected,
                        expectedHash,
                        text::Diagnostics::hashFound,
                        jam::Strings::getView (actualHash));
}

std::pair<juce::Result, std::unique_ptr<Input>> TextDocument::getInput (const juce::File& file,
                                                                        std::string_view displayPath,
                                                                        size_t maxReadSize)
{
    auto fileInput { Input::fromFile (file, maxReadSize) };

    if (fileInput == nullptr)
        return { getReadFailure (file, displayPath), nullptr };

    return { juce::Result::ok(), std::move (fileInput) };
}

std::pair<juce::Result, std::unique_ptr<Input>> TextDocument::getInput (const std::optional<std::string>& source, int numLines)
{
    auto [inputStatus, standardInput] { Input::create (source, numLines) };

    if (inputStatus.failed())
        return { inputStatus, nullptr };

    if (standardInput->getBytes().empty())
        return { Stdio::fail (text::Diagnostics::failMissingPath), nullptr };

    return { juce::Result::ok(), std::move (standardInput) };
}

std::pair<juce::Result, std::unique_ptr<TextDocument>> TextDocument::fromFile (const juce::File& file,
                                                                                std::string_view displayPath,
                                                                                size_t maxReadSize)
{
    auto [inputStatus, fileInput] { getInput (file, displayPath, maxReadSize) };

    if (inputStatus.failed())
        return { inputStatus, nullptr };

    auto document { std::make_unique<TextDocument> (Access {}, std::move (fileInput), displayPath) };
    document->setSource (document->input->getBytes());
    document->setLineIndex();
    return { juce::Result::ok(), std::move (document) };
}

std::pair<juce::Result, std::unique_ptr<TextDocument>> TextDocument::fromTextFile (const juce::File& file,
                                                                                    std::string_view displayPath,
                                                                                    size_t maxReadSize)
{
    auto [documentStatus, document] { fromFile (file, displayPath, maxReadSize) };

    if (documentStatus.failed())
        return { documentStatus, nullptr };

    if (document->isBinary())
        return { getBinaryFailure (displayPath), nullptr };

    return { juce::Result::ok(), std::move (document) };
}

std::unique_ptr<TextDocument> TextDocument::fromInput (std::unique_ptr<Input> newInput,
                                                       std::string_view displayPath)
{
    auto document { std::make_unique<TextDocument> (Access {}, std::move (newInput), displayPath) };
    document->setSource (document->input != nullptr ? document->input->getBytes() : std::string_view {});
    document->setLineIndex();
    return document;
}

std::unique_ptr<TextDocument> TextDocument::fromText (std::string&& text, std::string_view displayPath)
{
    auto document { std::make_unique<TextDocument> (Access {}, nullptr, displayPath) };
    *document->arena = std::move (text);
    document->setSource (*document->arena);
    return document;
}

std::unique_ptr<TextDocument> TextDocument::fromView (std::string_view text, std::string_view displayPath)
{
    auto document { std::make_unique<TextDocument> (Access {}, nullptr, displayPath) };
    document->setSource (text);
    return document;
}

std::unique_ptr<TextDocument> TextDocument::fromInput (std::unique_ptr<Input> newInput,
                                                       std::string_view displayPath,
                                                       std::string_view output)
{
    auto document { std::make_unique<TextDocument> (Access {}, std::move (newInput), displayPath) };
    document->setSource (output);
    return document;
}

std::string_view TextDocument::getDisplayPath() const noexcept
{
    return jam::Strings::getView (*getRoot()->get<juce::String> (Id::path));
}

std::string_view TextDocument::getBytes() const noexcept
{
    return source;
}

bool TextDocument::isBinary() const noexcept
{
    return binary;
}

int TextDocument::getNumLines() const noexcept
{
    if (binary)
        return 0;

    return lineOffsets.size() - (source.empty() or hasTrailingNewline() ? 1 : 0);
}

void TextDocument::setSource (std::string_view text)
{
    source = text;
    Document::parse();
}

void TextDocument::setLineIndex()
{
    binary = isBinary (source);

    if (not binary)
    {
        addLineOffsets (0);

        if (lineOffsets.size() > 1 and lineOffsets.at (1) >= 2 and source.at (lineOffsets.at (1) - 2) == static_cast<char> (Chars::carriageReturn))
            dominantLineEnding = windowsLineEnding;
    }
}

std::string_view TextDocument::getLine (std::string_view bytes, size_t lineStart, size_t newlinePos)
{
    if (newlinePos < bytes.size() and newlinePos > lineStart and bytes.at (newlinePos - 1) == static_cast<char> (Chars::carriageReturn))
        return bytes.substr (lineStart, newlinePos - 1 - lineStart);

    return bytes.substr (lineStart, newlinePos - lineStart);
}

std::string_view TextDocument::getLeadingLines (std::string_view bytes, int numLines)
{
    assert (numLines >= 0);
    return bytes.substr (0, applyToLines (bytes, juce::Range<int> (numLines, numLines + 1), [] (int, std::string_view) { return true; }));
}

std::string_view TextDocument::getLine (int lineNumber) const
{
    assert (lineNumber >= 1 and lineNumber <= getNumLines());
    const auto bytes { getBytes() };
    const size_t lineEnd { lineNumber < getNumLines() ? lineOffsets.at (lineNumber) - 1
                                                      : bytes.size() - (hasTrailingNewline() ? 1 : 0) };
    return getLine (bytes, lineOffsets.at (lineNumber - 1), lineEnd);
}

int TextDocument::getLineNumber (size_t offset) const noexcept
{
    assert (offset < getBytes().size());
    return static_cast<int> (Document::getLineNumber (offset));
}

std::string_view TextDocument::getLineBytes (juce::Range<int> lineRange) const
{
    assert (lineRange.getStart() >= 1 and lineRange.getEnd() <= getNumLines() + 1);
    assert (not lineRange.isEmpty());
    const auto startOffset { lineOffsets.at (lineRange.getStart() - 1) };
    const auto endOffset { lineRange.getEnd() <= getNumLines() ? lineOffsets.at (lineRange.getEnd() - 1) : getBytes().size() };
    return getBytes().substr (startOffset, endOffset - startOffset);
}

std::string_view TextDocument::getLineEnding() const noexcept
{
    return dominantLineEnding;
}

bool TextDocument::hasTrailingNewline() const noexcept
{
    return not getBytes().empty() and getBytes().back() == static_cast<char> (Chars::newline);
}

juce::String TextDocument::getLineHash (std::string_view line)
{
    return juce::SHA256 (line.data(), line.size()).toHexString().substring (0, lineHashLength);
}

juce::String TextDocument::getContentHash (std::string_view bytes)
{
    return juce::SHA256 (bytes.data() != nullptr ? bytes.data() : "", bytes.size()).toHexString();
}

juce::String TextDocument::getLineHash (int lineNumber) const
{
    return getLineHash (getLine (lineNumber));
}

juce::String TextDocument::getContentHash() const
{
    return getContentHash (getBytes());
}

void TextDocument::appendLinePrefix (std::string& output,
                                     int lineNumber,
                                     std::string_view line,
                                     char separator,
                                     bool isShowingLineNumber,
                                     bool isShowingHash)
{
    if (isShowingLineNumber or isShowingHash)
    {
        jam::Format::appendDecimal (output, lineNumber);
        jam::Format::appendByte (output, separator);
    }

    if (isShowingHash)
    {
        const auto lineHash { getLineHash (line) };
        jam::Format::appendBytes (output, jam::Strings::getView (lineHash));
        jam::Format::appendUtf8 (output, Chars::pipe);
    }
}
