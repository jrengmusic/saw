#pragma once
#include <JuceHeader.h>

/** @brief Immutable view over a memory-mapped file or owned buffer with a line index.
 */
class TextDocument
{
    struct Access {};

public:
    /** @brief Leading hex character count taken from a line's SHA-256 digest.
     */
    static constexpr int lineHashLength { 4 };

    /** @brief Builds a document from a memory-mapped file or owned byte buffer.
     *
     *  Indexes line boundaries and detects NUL bytes and dominant line endings in one pass.
     *
     *  @param newMappedFile  Read-only mapped file, or null when using @p newOwnedBytes.
     *  @param newOwnedBytes  Owned buffer used when @p newMappedFile is null.
     *  @param newDisplayPath Path string preserved for diagnostics and diff headers.
     */
    TextDocument (Access,
                  std::unique_ptr<juce::MemoryMappedFile> newMappedFile,
                  std::string newOwnedBytes,
                  const juce::String& newDisplayPath);

    /** @brief Opens and indexes an existing file, mapping non-empty files read-only.
     *
     *  @param file        File on disk to read.
     *  @param displayPath Path string stored on the document for diagnostics and output.
     *
     *  @return Status result paired with the loaded document, or failure with null.
     */
    static std::pair<juce::Result, std::unique_ptr<TextDocument>> fromFile (const juce::File& file,
                                                                            const juce::String& displayPath);

    /** @brief Opens an existing file and fails if the buffer contains any NUL byte.
     *
     *  @param file        File on disk to read and verify as text.
     *  @param displayPath Path string stored on the document for diagnostics and output.
     *
     *  @return Status result paired with the text document, or failure with null.
     */
    static std::pair<juce::Result, std::unique_ptr<TextDocument>> fromTextFile (const juce::File& file,
                                                                                const juce::String& displayPath);

    /** @brief Creates a document owning an in-memory byte string.
     *
     *  @param text        Raw bytes moved into the document.
     *  @param displayPath Path string stored on the document for diagnostics and output.
     *
     *  @return Newly constructed document owning @p text.
     */
    static std::unique_ptr<TextDocument> fromString (std::string text,
                                                     const juce::String& displayPath);

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
    std::unique_ptr<juce::MemoryMappedFile> mappedFile;
    std::string ownedBytes;
    juce::String displayPath;
    const char* data { nullptr };
    size_t size { 0 };
    bool binary { false };
    std::string_view dominantLineEnding { "\n" };
    jam::Array<size_t> lineStarts;
    jam::Array<size_t> lineLengths;
};
