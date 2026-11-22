#include "TextDocument.h"
#include "generated/Generated.h"

TextDocument::TextDocument (Access,
                            std::unique_ptr<juce::MemoryMappedFile> newMappedFile,
                            std::string newOwnedBytes,
                            const juce::String& newDisplayPath)
    : mappedFile (std::move (newMappedFile))
    , ownedBytes (std::move (newOwnedBytes))
    , displayPath (newDisplayPath)
{
    if (mappedFile != nullptr)
    {
        data = static_cast<const char*> (mappedFile->getData());
        size = mappedFile->getSize();
    }
    else
    {
        data = ownedBytes.data();
        size = ownedBytes.size();
    }

    binary = size > 0 and jam::simd::findByte (data, size, 0, '\0') < size;

    if (size > 0 and not binary)
    {
        size_t cursor { 0 };

        while (cursor < size)
        {
            const auto newlineIndex { jam::simd::findByte (data, size, cursor, static_cast<char> (Chars::newline)) };
            const auto hasCarriageReturn { newlineIndex < size
                                           and newlineIndex > cursor
                                           and getBytes().at (newlineIndex - 1) == static_cast<char> (Chars::carriageReturn) };
            const auto lineEnd { hasCarriageReturn ? newlineIndex - 1 : newlineIndex };

            if (lineStarts.isEmpty() and hasCarriageReturn)
                dominantLineEnding = "\r\n";

            lineStarts.add (cursor);
            lineLengths.add (lineEnd - cursor);
            cursor = newlineIndex < size ? newlineIndex + 1 : size;
        }
    }
}

std::pair<juce::Result, std::unique_ptr<TextDocument>> TextDocument::fromFile (const juce::File& file,
                                                                                const juce::String& displayPath)
{
    if (not file.existsAsFile())
        return { juce::Result::fail (juce::String::formatted (text::Diagnostics::failNotFound, displayPath.toRawUTF8())), nullptr };

    if (file.getSize() == 0)
        return { juce::Result::ok(), std::make_unique<TextDocument> (Access {}, nullptr, std::string {}, displayPath) };

    auto mapped { std::make_unique<juce::MemoryMappedFile> (file, juce::MemoryMappedFile::readOnly) };

    if (mapped->getData() == nullptr)
        return { juce::Result::fail (juce::String::formatted (text::Diagnostics::failNotFound, displayPath.toRawUTF8())), nullptr };

    return { juce::Result::ok(), std::make_unique<TextDocument> (Access {}, std::move (mapped), std::string {}, displayPath) };
}

std::pair<juce::Result, std::unique_ptr<TextDocument>> TextDocument::fromTextFile (const juce::File& file,
                                                                                    const juce::String& displayPath)
{
    auto [status, document] { fromFile (file, displayPath) };

    if (status.failed())
        return { status, nullptr };

    if (document->isBinary())
        return { juce::Result::fail (juce::String::formatted (text::Diagnostics::failBinaryFile,
                                                              displayPath.toRawUTF8())),
                 nullptr };

    return { juce::Result::ok(), std::move (document) };
}

std::unique_ptr<TextDocument> TextDocument::fromString (std::string text,
                                                        const juce::String& displayPath)
{
    return std::make_unique<TextDocument> (Access {}, nullptr, std::move (text), displayPath);
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

std::string_view TextDocument::getLine (int lineNumber) const
{
    assert (lineNumber >= 1 and lineNumber <= getNumLines());
    return std::string_view { data + lineStarts.at (lineNumber - 1), lineLengths.at (lineNumber - 1) };
}

std::string_view TextDocument::getLineEnding() const noexcept
{
    return dominantLineEnding;
}

bool TextDocument::hasTrailingNewline() const noexcept
{
    return size > 0 and getBytes().at (size - 1) == static_cast<char> (Chars::newline);
}

juce::String TextDocument::getLineHash (int lineNumber) const
{
    const auto line { getLine (lineNumber) };
    return juce::SHA256 (line.data(), line.size()).toHexString().substring (0, lineHashLength);
}

juce::String TextDocument::getContentHash() const
{
    return juce::SHA256 (data != nullptr ? data : "", size).toHexString();
}
