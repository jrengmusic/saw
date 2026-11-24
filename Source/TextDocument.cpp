#include "TextDocument.h"
#include "Stdio.h"
#include "generated/Generated.h"

TextDocument::TextDocument (Access,
                            std::unique_ptr<Input> newInput,
                            std::string newOwnedBytes,
                            const juce::String& newDisplayPath)
    : input (std::move (newInput))
    , ownedBytes (std::move (newOwnedBytes))
    , displayPath (newDisplayPath)
{
    if (input != nullptr)
    {
        data = input->getBytes().data();
        size = input->getBytes().size();
    }
    else
    {
        data = ownedBytes.data();
        size = ownedBytes.size();
    }

    binary = isBinary (getBytes());

    if (size > 0 and not binary)
    {
        size_t cursor { 0 };

        while (cursor < size)
        {
            const auto newlineIndex { jam::simd::findByte (data, size, cursor, static_cast<char> (Chars::newline)) };
            const auto line { getLine (getBytes(), cursor, newlineIndex) };
            const bool hasCarriageReturn { line.size() < newlineIndex - cursor };

            if (lineStarts.isEmpty() and hasCarriageReturn)
                dominantLineEnding = "\r\n";

            lineStarts.add (cursor);
            lineLengths.add (line.size());
            cursor = newlineIndex < size ? newlineIndex + 1 : size;
        }
    }
}

juce::Result TextDocument::getReadFailure (const juce::File& file, const juce::String& displayPath)
{
    if (file.exists())
        return juce::Result::fail (Stdio::getFormatted (text::Diagnostics::failRead, displayPath.toRawUTF8()));

    return juce::Result::fail (Stdio::getFormatted (text::Diagnostics::failNotFound, displayPath.toRawUTF8()));
}

juce::Result TextDocument::getBinaryFailure (const juce::String& displayPath)
{
    return juce::Result::fail (Stdio::getFormatted (text::Diagnostics::failBinaryFile, displayPath.toRawUTF8()));
}

bool TextDocument::isBinary (std::string_view bytes)
{
    return jam::simd::findByte (bytes.data(), bytes.size(), 0, '\0') < bytes.size();
}

juce::Result TextDocument::getBytesStatus (const juce::File& file,
                                           const juce::String& displayPath,
                                           std::string_view expectedBytes,
                                           std::string_view failure)
{
    const auto [readStatus, fileInput] { getInput (file, displayPath, Input::maxBufferedReadSize) };

    if (readStatus.failed())
        return readStatus;

    if (fileInput->getBytes().compare (expectedBytes) != 0)
        return juce::Result::fail (jam::Strings::fromView (failure));

    return juce::Result::ok();
}

std::pair<juce::Result, std::unique_ptr<Input>> TextDocument::getInput (const juce::File& file,
                                                                        const juce::String& displayPath,
                                                                        size_t maxReadSize)
{
    auto fileInput { Input::fromFile (file, maxReadSize) };

    if (fileInput == nullptr)
        return { getReadFailure (file, displayPath), nullptr };

    return { juce::Result::ok(), std::move (fileInput) };
}

std::pair<juce::Result, std::unique_ptr<TextDocument>> TextDocument::fromFile (const juce::File& file,
                                                                                const juce::String& displayPath)
{
    auto [inputStatus, fileInput] { getInput (file, displayPath, Input::maxBufferedReadSize) };

    if (inputStatus.failed())
        return { inputStatus, nullptr };

    return { juce::Result::ok(), std::make_unique<TextDocument> (Access {}, std::move (fileInput), std::string {}, displayPath) };
}

std::pair<juce::Result, std::unique_ptr<TextDocument>> TextDocument::fromTextFile (const juce::File& file,
                                                                                    const juce::String& displayPath,
                                                                                    size_t maxReadSize)
{
    auto [inputStatus, fileInput] { getInput (file, displayPath, maxReadSize) };

    if (inputStatus.failed())
        return { inputStatus, nullptr };

    auto document { fromInput (std::move (fileInput), displayPath) };

    if (document->isBinary())
        return { getBinaryFailure (displayPath), nullptr };

    return { juce::Result::ok(), std::move (document) };
}

std::pair<juce::Result, std::unique_ptr<TextDocument>> TextDocument::fromOwnedFile (const juce::File& file,
                                                                                    const juce::String& displayPath)
{
    auto [inputStatus, fileInput] { getInput (file, displayPath, Input::allBytes) };

    if (inputStatus.failed())
        return { inputStatus, nullptr };

    return { juce::Result::ok(), fromInput (std::move (fileInput), displayPath) };
}

std::unique_ptr<TextDocument> TextDocument::fromString (std::string text,
                                                        const juce::String& displayPath)
{
    return std::make_unique<TextDocument> (Access {}, nullptr, std::move (text), displayPath);
}

std::unique_ptr<TextDocument> TextDocument::fromInput (std::unique_ptr<Input> newInput,
                                                       const juce::String& displayPath)
{
    return std::make_unique<TextDocument> (Access {}, std::move (newInput), std::string {}, displayPath);
}

const juce::String& TextDocument::getDisplayPath() const noexcept
{
    return displayPath;
}

std::string_view TextDocument::getBytes() const noexcept
{
    return size > 0 ? std::string_view { data, size } : std::string_view {};
}

bool TextDocument::isBinary() const noexcept
{
    return binary;
}

int TextDocument::getNumLines() const noexcept
{
    return lineStarts.size();
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
    size_t cursor { 0 };

    for (int line { 0 }; line < numLines and cursor < bytes.size(); ++line)
    {
        const size_t newlinePos { jam::simd::findByte (bytes.data(), bytes.size(), cursor, static_cast<char> (Chars::newline)) };
        cursor = newlinePos < bytes.size() ? newlinePos + 1 : bytes.size();
    }

    return bytes.substr (0, cursor);
}

std::string_view TextDocument::getLine (int lineNumber) const
{
    assert (lineNumber >= 1 and lineNumber <= getNumLines());
    return std::string_view { data + lineStarts.at (lineNumber - 1), lineLengths.at (lineNumber - 1) };
}

int TextDocument::getLineNumber (size_t offset) const noexcept
{
    assert (offset < size);
    const auto firstLineStartingAfter { std::upper_bound (lineStarts.begin(), lineStarts.end(), offset) };
    return static_cast<int> (firstLineStartingAfter - lineStarts.begin());
}

std::string_view TextDocument::getLineBytes (juce::Range<int> lineRange) const
{
    assert (lineRange.getStart() >= 1 and lineRange.getEnd() <= getNumLines() + 1);
    assert (not lineRange.isEmpty());
    const auto startOffset { lineStarts.at (lineRange.getStart() - 1) };
    const auto endOffset { lineRange.getEnd() <= getNumLines() ? lineStarts.at (lineRange.getEnd() - 1) : size };
    return std::string_view { data + startOffset, endOffset - startOffset };
}

std::string_view TextDocument::getLineEnding() const noexcept
{
    return dominantLineEnding;
}

bool TextDocument::hasTrailingNewline() const noexcept
{
    return size > 0 and getBytes().at (size - 1) == static_cast<char> (Chars::newline);
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
