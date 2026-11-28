#pragma once
#include <JuceHeader.h>
#include "Input.h"

/** @brief The text of one file, of standard input, or of one piece of output, as a jam::Document.
 *
 *  One document type holds the input and the output, so that no text is copied between a verb and its
 *  writer. The display path is stored as a non-owning std::string_view member. The bytes are owned by the
 *  Input, owned by the MemoryBlock, owned by a viewedDocument, owned by the arena of jam::Document, or viewed.
 *  fromTextFile, and fromInput without an output build it, because output is not read by line. Plain
 *  text has no structure, so getToken takes the whole text as one token. Each line is a view into the bytes.
 */
class TextDocument : public jam::Document
{
    /** @brief Key that restricts the constructor to the factory functions of this class. */
    struct Access {};

public:
    /** @brief Leading hex character count taken from a line's SHA-256 digest. */
    static constexpr int lineHashLength { 4 };

    /** @brief Stores the input and the display path. The factory that calls this sets the bytes.
     *
     *  @param newInput       Input that holds the bytes, or nullptr when the bytes are supplied by the factory.
     *  @param newDisplayPath Non-owning display path view for diagnostics and diff headers.
     */
    TextDocument (Access,
                  std::unique_ptr<Input> newInput,
                  std::string_view newDisplayPath);

    /** @brief Opens and indexes an existing file. A file larger than maxReadSize is mapped.
     *
     *  @param file        File on disk to read.
     *  @param displayPath Path string stored on the document for diagnostics and output.
     *  @param maxReadSize The largest file size that is read into a buffer. Larger files are mapped.
     *  @param isText      True when the file is text. A NUL byte then does not make the file binary, and its lines are indexed.
     *
     *  @return Status result paired with the loaded document, or failure with null.
     */
    static std::pair<juce::Result, std::unique_ptr<TextDocument>> fromFile (const juce::File& file,
                                                                            std::string_view displayPath,
                                                                            size_t maxReadSize,
                                                                            bool isText);

    /** @brief Opens an existing file and fails if the bytes contain any NUL byte.
     *
     *  @param file        File on disk to read and verify as text.
     *  @param displayPath Path string stored on the document for diagnostics and output.
     *  @param maxReadSize The largest file size that is read into a buffer. Larger files are mapped.
     *
     *  @return Status result paired with the text document, or failure with null.
     */
    static std::pair<juce::Result, std::unique_ptr<TextDocument>> fromTextFile (const juce::File& file,
                                                                                std::string_view displayPath,
                                                                                size_t maxReadSize);

    /** @brief Creates a document over an Input, and indexes the bytes of the Input.
     *
     *  The Input owns the bytes, so the document reads them in place. A null Input gives the empty document.
     *
     *  @param newInput    The Input that holds the bytes. The document takes ownership.
     *  @param displayPath Path string stored on the document for diagnostics and output.
     *
     *  @return Newly constructed document.
     */
    static std::unique_ptr<TextDocument> fromInput (std::unique_ptr<Input> newInput,
                                                    std::string_view displayPath);

    /** @brief Creates a document that owns text, moved into the arena of jam::Document.
     *
     *  The text is moved, so no byte is copied. The line index is not built, because the text is output only.
     *
     *  @param text        The text, moved into the document.
     *  @param displayPath Path string stored on the document for diagnostics and output.
     *
     *  @return Newly constructed document.
     */
    static std::unique_ptr<TextDocument> fromText (std::string&& text, std::string_view displayPath);

    /** @brief Creates a document that views bytes owned elsewhere.
     *
     *  The document does not own the bytes. The line index is not built, because the text is output only.
     *
     *  @param text        The bytes to view. They must outlive the document.
     *  @param displayPath Non-owning display path view for diagnostics and output.
     *
     *  @return Newly constructed document.
     */
    static std::unique_ptr<TextDocument> fromView (std::string_view text, std::string_view displayPath);

    /** @brief Creates a document that owns a juce::MemoryBlock, moved in without copying.
     *
     *  The line index is not built, because the block is output only.
     *
     *  @param newBlock    Memory block holding the UTF-8 output bytes.
     *  @param displayPath Non-owning display path view for diagnostics and output.
     *
     *  @return Newly constructed document.
     */
    static std::unique_ptr<TextDocument> fromBlock (juce::MemoryBlock&& newBlock, std::string_view displayPath);

    /** @brief Creates a document that owns another TextDocument and views a subrange of its bytes.
     *
     *  Used by output limiting to slice a document without copying its bytes.
     *
     *  @param newViewedDocument The owning document whose storage outlives the view.
     *  @param output            Subrange view into newViewedDocument's bytes.
     *
     *  @return Newly constructed document.
     */
    static std::unique_ptr<TextDocument> fromDocument (std::unique_ptr<TextDocument> newViewedDocument, std::string_view output);

    /** @brief Creates a document that owns an Input, and views a range of its bytes as the output.
     *
     *  The document owns the Input, so output is a view into the bytes that the Input owns, and
     *  getBytes returns that view. The line index is not built, because the text is output only.
     *
     *  @param newInput    The Input that owns the bytes. The document takes ownership.
     *  @param displayPath Path string stored on the document for diagnostics and output.
     *  @param output      A view into the bytes of newInput, which becomes the text of the document.
     *
     *  @return Newly constructed document.
     */
    static std::unique_ptr<TextDocument> fromInput (std::unique_ptr<Input> newInput,
                                                    std::string_view displayPath,
                                                    std::string_view output);

    /** @brief Builds the failure of a file that cannot be read.
     *
     *  @param file        The file that failed to read.
     *  @param displayPath Path string used in the diagnostic.
     *
     *  @return Failure that reports not found when the file does not exist, otherwise unreadable.
     */
    static juce::Result getReadFailure (const juce::File& file, std::string_view displayPath);

    /** @brief Builds the failure of a binary file that is read as text.
     *
     *  @param displayPath Path string used in the diagnostic.
     *
     *  @return Failure that reports a binary file.
     */
    static juce::Result getBinaryFailure (std::string_view displayPath);

    /** @brief Returns the status of a binary input.
     *
     *  An explicit input is named on the command line, so its binary bytes are a failure. An input that
     *  a walk found gives ok, so that a binary file does not stop the command.
     *
     *  @param displayPath The display path of the input. The failure text uses it.
     *  @param isExplicit  True when the input was named as a file on the command line.
     *
     *  @return The binary failure when the input is explicit, otherwise ok.
     */
    static juce::Result getBinaryStatus (std::string_view displayPath, bool isExplicit);

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
                                        std::string_view displayPath,
                                        std::string_view expectedBytes,
                                        std::string_view failure);

    /** @brief Checks the hash of one line against an expected hash.
     *
     *  A line number below 1 has no line, so the found hash is empty and the check fails.
     *
     *  @param lineNumber   One-based line index. A number below 1 has no line.
     *  @param expectedHash The expected hash text, compared as it is.
     *
     *  @return Ok when the line exists and its hash equals expectedHash. Otherwise the hash mismatch failure,
     *          which names the display path, the line number, the expected hash, and the found hash.
     */
    juce::Result getLineHashStatus (int lineNumber, std::string_view expectedHash) const;

    /** @brief Opens a file as an Input, with a failure when the file cannot be read.
     *
     *  @param file        File on disk to open.
     *  @param displayPath Path string used in the read failure.
     *  @param maxReadSize The largest file size that is read into a buffer. Larger files are mapped.
     *
     *  @return Status result paired with the Input, or failure with null.
     */
    static std::pair<juce::Result, std::unique_ptr<Input>> getInput (const juce::File& file,
                                                                     std::string_view displayPath,
                                                                     size_t maxReadSize);

    /** @brief Opens a file as an Input, or reads the source when the file is the empty file.
     *
     *  When the file is the empty juce::File, the source is read by Input::create with allLines, and displayPath is not used.
     *
     *  @param file        The file to open, or the empty file.
     *  @param displayPath Path string used in the read failure.
     *  @param maxReadSize The largest file size that is read into a buffer. Larger files are mapped.
     *  @param source      The source that is read when the file is the empty file.
     *
     *  @return Status result paired with the Input, or failure with null.
     */
    static std::pair<juce::Result, std::unique_ptr<Input>> getInput (const juce::File& file,
                                                                     std::string_view displayPath,
                                                                     size_t maxReadSize,
                                                                     const Input::Source& source);

    /** @brief Opens the supplied text, or the standard input, as an Input.
     *
     *  @param source   The standard input, the null device, or a supplied text.
     *  @param numLines The number of newlines to read from standard input that is read. See Input::create.
     *
     *  @return Status result paired with the Input, or failure with null when standard input cannot be read. See Input::create.
     */
    static std::pair<juce::Result, std::unique_ptr<Input>> getInput (const Input::Source& source, int numLines);

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
    /** @brief Returns the line ending that follows a line in the bytes.
     *
     *  @param bytes The bytes that hold the line.
     *  @param line  The line, as a view into bytes, without its line ending.
     *
     *  @return A newline, a CRLF pair, or an empty view when no line ending follows the line.
     */
    static std::string_view getLineEnding (std::string_view bytes, std::string_view line);

    /** @brief Returns the first lines of bytes, with their line endings.
     *
     *  @param bytes    The bytes to read.
     *  @param numLines The number of lines to return.
     *
     *  @return View from the start of bytes to the end of the last returned line.
     *          The view is shorter when bytes has fewer lines.
     */
    static std::string_view getLeadingLines (std::string_view bytes, int numLines);

    /** @brief Calls a function for each line of a line range, until the function returns false.
     *
     *  The lines before the range are read, but the function is not called for them. Each line is passed
     *  as a view without its line ending. The cursor moves past a line after the function returns for it,
     *  so the returned count includes the line that stopped the loop.
     *
     *  @tparam Function Callable accepting `(int lineNumber, std::string_view line)` and returning bool. True continues with the next line.
     *  @param bytes     The bytes to read.
     *  @param lineRange One-based half-open range of lines, `[start, end)`, to pass to the function.
     *  @param function  The callable for each line of the range.
     *
     *  @return The number of bytes that were read, from the start of bytes through the last line that was read.
     */
    template <typename Function>
    static size_t applyToLines (std::string_view bytes, juce::Range<int> lineRange, const Function& function)
    {
        size_t cursor { 0 };
        int lineNumber { 1 };
        bool isContinuing { true };

        while (isContinuing and cursor < bytes.size() and lineNumber < lineRange.getEnd() and not lineRange.isEmpty())
        {
            const size_t newlinePos { jam::simd::findByte (bytes.data(), bytes.size(), cursor, static_cast<char> (Chars::newline)) };

            if (lineNumber >= lineRange.getStart())
                isContinuing = function (lineNumber, getLine (bytes, cursor, newlinePos));

            cursor = newlinePos < bytes.size() ? newlinePos + 1 : bytes.size();
            ++lineNumber;
        }

        return cursor;
    }

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

    /** @brief Appends the line number and the hash that prefix one output line.
     *
     *  The line number and the separator are appended when the line number or the hash is shown. The hash
     *  and a pipe follow when the hash is shown. The caller appends the text and the newline.
     *
     *  @param output              The text that receives the prefix.
     *  @param lineNumber          The one-based line number. It is written when the number or the hash is shown.
     *  @param line                The line bytes, without the line ending. They are hashed only when the hash is shown.
     *  @param separator           The character written after the line number when the number or the hash is shown.
     *  @param isShowingLineNumber True to show the line number.
     *  @param isShowingHash       True to show the hash of the line.
     */
    static void appendLinePrefix (std::string& output,
                                  int lineNumber,
                                  std::string_view line,
                                  char separator,
                                  bool isShowingLineNumber,
                                  bool isShowingHash);

    /** @brief Returns the caller-supplied display path.
     *
     *  @return A view of the display path stored at construction. The view is valid while this document lives.
     */
    std::string_view getDisplayPath() const noexcept;

    /** @brief Returns the text of the document in place.
     *
     *  @return View of the bytes, or empty when the document has zero bytes. The view is valid while this document lives.
     */
    std::string_view getBytes() const noexcept;

    /** @brief Reports whether the document is binary: not text, and holding a NUL byte. A binary document has no lines.
     *
     *  @return True when the document was built as binary.
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

protected:
    /** @brief Returns the vocabulary of plain text, the only vocabulary of a document.
     *
     *  @return The plain text vocabulary.
     */
    const Vocabulary& getVocabulary() const override;

    /** @brief Takes the whole remaining text as one token, because plain text has no structure.
     *
     *  @param cursor The position of the parse. Its source is the text.
     *
     *  @return The number of bytes of the token, which is every byte from the cursor to the end.
     */
    size_t getToken (Cursor& cursor, int) override;

    /** @brief Adds nothing to the tree, because plain text has no structure. */
    void build() override;

private:
    /** @brief Line ending of Windows text, detected for the first line. */
    static constexpr std::string_view windowsLineEnding { "\r\n" };

    /** @brief Line ending of Unix text, used when the first line has no CR. */
    static constexpr std::string_view unixLineEnding { "\n" };

    /** @brief Input that owns the bytes, or nullptr when a factory owns them. Kept so the bytes outlive the document. */
    std::unique_ptr<Input> input;

    /** @brief Owned memory block when constructed via fromBlock. */
    juce::MemoryBlock block;

    /** @brief Owned backing document when constructed via fromDocument. */
    std::unique_ptr<TextDocument> viewedDocument;

    /** @brief True when the document is not text and its bytes hold a NUL byte, so the document has no lines. */
    bool binary { false };

    /** @brief Non-owning display path view for diagnostics and diff headers. */
    std::string_view displayPath;

    /** @brief The line ending of the first line, returned by getLineEnding. */
    std::string_view dominantLineEnding { unixLineEnding };

    /** @brief Sets the bytes of the document and parses them.
     *
     *  @param text The bytes that the document views.
     */
    void setSource (std::string_view text);

    /** @brief Detects a NUL byte when the document is not text, and indexes the line starts and the dominant line ending.
     *
     *  @param isText True when the document is text, so that a NUL byte does not make it binary.
     */
    void setLineIndex (bool isText);
};
