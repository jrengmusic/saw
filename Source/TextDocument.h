#pragma once
#include <JuceHeader.h>
#include "Input.h"

/** @brief Bytes of an Input or an owned string, with a line index.
 *
 *  The document owns its bytes, through the Input or the owned string. The line index holds
 *  the start and the length of each line, so a line is read without a copy.
 */
class TextDocument
{
    struct Access {};

public:
    /** @brief Leading hex character count taken from a line's SHA-256 digest.
     */
    static constexpr int lineHashLength { 4 };

    /** @brief Builds a document from an Input or an owned byte buffer.
     *
     *  Detects NUL bytes first. For text, the constructor indexes the line boundaries and the
     *  dominant line ending.
     *
     *  @param newInput       Input that holds the bytes, or null when newOwnedBytes holds them.
     *  @param newOwnedBytes  Owned buffer used when newInput is null.
     *  @param newDisplayPath Path string preserved for diagnostics and diff headers.
     */
    TextDocument (Access,
                  std::unique_ptr<Input> newInput,
                  std::string newOwnedBytes,
                  const juce::String& newDisplayPath);

    /** @brief Opens and indexes an existing file. A file larger than maxBufferedReadSize is mapped.
     *
     *  @param file        File on disk to read.
     *  @param displayPath Path string stored on the document for diagnostics and output.
     *
     *  @return Status result paired with the loaded document, or failure with null.
     */
    static std::pair<juce::Result, std::unique_ptr<TextDocument>> fromFile (const juce::File& file,
                                                                            const juce::String& displayPath);

    /** @brief Opens an existing file and fails if the bytes contain any NUL byte.
     *
     *  @param file        File on disk to read and verify as text.
     *  @param displayPath Path string stored on the document for diagnostics and output.
     *  @param maxReadSize The largest file size that is read into a buffer. Larger files are mapped.
     *
     *  @return Status result paired with the text document, or failure with null.
     */
    static std::pair<juce::Result, std::unique_ptr<TextDocument>> fromTextFile (const juce::File& file,
                                                                                const juce::String& displayPath,
                                                                                size_t maxReadSize);

    /** @brief Opens an existing file into an owned buffer.
     *
     *  The owned buffer keeps the bytes when the file changes later, so a write to the same
     *  file does not change the document.
     *
     *  @param file        File on disk to read.
     *  @param displayPath Path string stored on the document for diagnostics and output.
     *
     *  @return Status result paired with the document, or failure with null.
     */
    static std::pair<juce::Result, std::unique_ptr<TextDocument>> fromOwnedFile (const juce::File& file,
                                                                                 const juce::String& displayPath);

    /** @brief Creates a document owning an in-memory byte string.
     *
     *  @param text        Raw bytes moved into the document.
     *  @param displayPath Path string stored on the document for diagnostics and output.
     *
     *  @return Newly constructed document owning text.
     */
    static std::unique_ptr<TextDocument> fromString (std::string text,
                                                     const juce::String& displayPath);

    /** @brief Creates a document over an Input.
     *
     *  @param newInput    The Input that holds the bytes. The document takes ownership.
     *  @param displayPath Path string stored on the document for diagnostics and output.
     *
     *  @return Newly constructed document.
     */
    static std::unique_ptr<TextDocument> fromInput (std::unique_ptr<Input> newInput,
                                                    const juce::String& displayPath);

    /** @brief Builds the failure of a file that cannot be read.
     *
     *  @param file        The file that failed to read.
     *  @param displayPath Path string used in the diagnostic.
     *
     *  @return Failure that reports not found when the file does not exist, otherwise unreadable.
     */
    static juce::Result getReadFailure (const juce::File& file, const juce::String& displayPath);

    /** @brief Builds the failure of a binary file that is read as text.
     *
     *  @param displayPath Path string used in the diagnostic.
     *
     *  @return Failure that reports a binary file.
     */
    static juce::Result getBinaryFailure (const juce::String& displayPath);

    /** @brief Checks whether bytes are binary. A NUL byte is the test.
     *
     *  @param bytes The bytes to test.
     *
     *  @return True when at least one NUL byte is present.
     */
    static bool isBinary (std::string_view bytes);

    /** @brief Checks that a file still holds the expected bytes, by comparing the bytes.
     *
     *  The comparison needs no hash, because the expected bytes are at hand.
     *
     *  @param file          The file to read again.
     *  @param displayPath   Path string used in the read failure.
     *  @param expectedBytes The bytes that the file must hold.
     *  @param failure       Failure text returned when the bytes differ.
     *
     *  @return Ok when the bytes match, the read failure when the file cannot be read, or failure with the given text.
     */
    static juce::Result getBytesStatus (const juce::File& file,
                                        const juce::String& displayPath,
                                        std::string_view expectedBytes,
                                        std::string_view failure);

    /** @brief Opens a file as an Input, with a failure when the file cannot be read.
     *
     *  @param file        File on disk to open.
     *  @param displayPath Path string used in the read failure.
     *  @param maxReadSize The largest file size that is read into a buffer. Larger files are mapped.
     *
     *  @return Status result paired with the Input, or failure with null.
     */
    static std::pair<juce::Result, std::unique_ptr<Input>> getInput (const juce::File& file,
                                                                     const juce::String& displayPath,
                                                                     size_t maxReadSize);

    /** @brief Returns one line of bytes without its line ending.
     *
     *  A carriage return before the newline is dropped, so CRLF text gives the same line as LF text.
     *
     *  @param bytes      The bytes that hold the line.
     *  @param lineStart  Offset of the first byte of the line.
     *  @param newlinePos Offset of the newline that ends the line, or bytes.size() at the end.
     *
     *  @return View of the line bytes, excluding the line ending.
     */
    static std::string_view getLine (std::string_view bytes, size_t lineStart, size_t newlinePos);

    /** @brief Returns the first lines of bytes, with their line endings.
     *
     *  @param bytes    The bytes to read.
     *  @param numLines The number of lines to return.
     *
     *  @return View from the start of bytes to the end of the last returned line.
     *          The view is shorter when bytes has fewer lines.
     */
    static std::string_view getLeadingLines (std::string_view bytes, int numLines);

    /** @brief Computes the truncated SHA-256 hex digest of one line.
     *
     *  @param line The line bytes, without the line ending.
     *
     *  @return The first lineHashLength lowercase hex characters of the SHA-256 digest.
     */
    static juce::String getLineHash (std::string_view line);

    /** @brief Computes the full SHA-256 hex digest of bytes.
     *
     *  @param bytes The bytes to hash.
     *
     *  @return Lowercase hex SHA-256 digest of bytes.
     */
    static juce::String getContentHash (std::string_view bytes);

    /** @brief Returns the caller-supplied display path.
     *
     *  @return Display path stored at construction.
     */
    const juce::String& getDisplayPath() const noexcept;

    /** @brief Returns a view of the entire raw byte buffer.
     *
     *  @return Raw byte view, or empty when the document has zero bytes.
     */
    std::string_view getBytes() const noexcept;

    /** @brief Reports whether the buffer contains any NUL byte.
     *
     *  @return True when at least one `\\0` byte is present.
     */
    bool isBinary() const noexcept;

    /** @brief Returns the number of indexed lines.
     *
     *  @return Line count; zero for empty or binary documents.
     */
    int getNumLines() const noexcept;

    /** @brief Returns one line without its trailing CR or LF bytes.
     *
     *  @param lineNumber One-based line index in `[1, getNumLines()]`.
     *
     *  @return View of the line bytes excluding line endings.
     */
    std::string_view getLine (int lineNumber) const;

    /** @brief Returns the line that holds a byte offset, using the line index.
     *
     *  @param offset Byte offset in `[0, getBytes().size())`.
     *
     *  @return One-based number of the line that contains the byte at offset.
     */
    int getLineNumber (size_t offset) const noexcept;

    /** @brief Returns the raw bytes of a line range, with the line endings.
     *
     *  The end of the range is exclusive. The last line of the document ends at the end of the bytes.
     *
     *  @param lineRange One-based half-open range of lines, `[start, end)`, with end at most getNumLines() + 1.
     *
     *  @return View of the bytes of the lines in the range, including their line endings.
     */
    std::string_view getLineBytes (juce::Range<int> lineRange) const;

    /** @brief Returns the line ending detected from the first line.
     *
     *  @return `"\\r\\n"` when the first line ends in CRLF, otherwise `"\\n"`.
     */
    std::string_view getLineEnding() const noexcept;

    /** @brief Reports whether the buffer ends with a newline byte.
     *
     *  @return True when non-empty and the last byte is `'\\n'`.
     */
    bool hasTrailingNewline() const noexcept;

    /** @brief Computes the truncated SHA-256 hex digest of a single line.
     *
     *  @param lineNumber One-based line index in `[1, getNumLines()]`.
     *
     *  @return First @ref lineHashLength lowercase hex characters of the line's SHA-256 digest.
     */
    juce::String getLineHash (int lineNumber) const;

    /** @brief Computes the full SHA-256 hex digest of the raw document bytes.
     *
     *  @return Lowercase hex SHA-256 digest covering the whole buffer.
     */
    juce::String getContentHash() const;

private:
    std::unique_ptr<Input> input;
    std::string ownedBytes;
    juce::String displayPath;
    const char* data { nullptr };
    size_t size { 0 };
    bool binary { false };
    std::string_view dominantLineEnding { "\n" };
    jam::Array<size_t> lineStarts;
    jam::Array<size_t> lineLengths;
};
