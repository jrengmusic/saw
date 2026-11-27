#pragma once
#include <JuceHeader.h>

/** @brief Converts bytes of text to the forms that a listing shows: the visible notation, sanitized text, expanded tabs, squeezed lines, and decoded UTF-16. */
class NonPrintable
{
public:
    /** @brief The notation of a control byte: Unicode control pictures, or caret notation such as `^A`. */
    enum class Notation { unicode, caret };

    /** @brief Returns the notation that a notation name selects.
     *
     *  @param notationName The notation name, as the user typed it.
     *
     *  @return The notation, or an empty optional when no notation has that name.
     */
    static std::optional<Notation> getNotation (std::string_view notationName);

    /** @brief Returns the bytes with each control, space, delete, and high byte shown in its visible form.
     *
     *  A tab is shown as spaces up to the next tab stop of four columns. A space is shown as its notation. A control byte is shown
     *  in the given notation, and a line feed is kept after it. A high byte is shown as an escape.
     *
     *  @param bytes    The bytes to show.
     *  @param notation The notation of the control bytes.
     *
     *  @return The visible text.
     */
    static std::string getVisible (std::string_view bytes, Notation notation);

    /** @brief Returns the bytes with each unsafe byte replaced by the replacement codepoint.
     *
     *  A carriage return and line feed pair, a tab, a line feed, and a form feed are kept. Other ASCII controls and delete are replaced.
     *  The high codepoints in the sanitized ranges are replaced too.
     *
     *  @param bytes The bytes to sanitize.
     *
     *  @return The sanitized text.
     */
    static std::string getSanitized (std::string_view bytes);

    /** @brief Returns the bytes with each ANSI escape sequence removed.
     *
     *  Text and control tokens are kept. An unterminated sequence is kept to the end of the bytes.
     *
     *  @param bytes The bytes to filter.
     *
     *  @return The text without the escape sequences.
     */
    static std::string getWithoutSequences (std::string_view bytes);

    /** @brief Returns the bytes with each tab expanded to the next tab stop, and each line feed kept.
     *
     *  The column restarts after each line feed.
     *
     *  @param bytes    The bytes to expand.
     *  @param tabWidth The width of a tab stop, in columns. It must be greater than zero.
     *
     *  @return The text with the tabs expanded.
     */
    static std::string getExpanded (std::string_view bytes, size_t tabWidth);

    /** @brief Returns the one-based numbers of the lines that a squeeze keeps.
     *
     *  A run of empty lines keeps up to limit lines. An empty line is a line with no bytes, or with one carriage return.
     *
     *  @param bytes The bytes to squeeze.
     *  @param limit The number of empty lines that each run keeps.
     *
     *  @return The kept line numbers, in ascending order.
     */
    static jam::Array<int> getSqueezedLines (std::string_view bytes, int limit);

    /** @brief Returns the text of the lines with the given one-based numbers, each with its line ending.
     *
     *  @param bytes       The bytes that hold the lines.
     *  @param lineNumbers The line numbers to keep, in ascending order.
     *
     *  @return The text of the kept lines.
     */
    static std::string getLinesText (std::string_view bytes, const jam::Array<int>& lineNumbers);

    /** @brief Tests whether the bytes start with a UTF-16 byte order mark.
     *
     *  @param bytes The bytes to test.
     *
     *  @return True when the bytes start with a little-endian or a big-endian byte order mark.
     */
    static bool isUtf16 (std::string_view bytes);

    /** @brief Returns the UTF-8 text of UTF-16 bytes.
     *
     *  The bytes must start with a byte order mark, as isUtf16 tests.
     *
     *  @param bytes The UTF-16 bytes.
     *
     *  @return The UTF-8 text.
     */
    static std::string getDecoded (std::string_view bytes);

    /** @brief Appends one byte as a two-digit hex escape, `\\x` followed by the digits.
     *
     *  @param output The text that receives the escape.
     *  @param byte   The byte to escape.
     */
    static void appendByteEscape (std::string& output, unsigned char byte);
};
