#include "NonPrintable.h"
#include "generated/Generated.h"

static constexpr unsigned char tabByte { 0x09 };
static constexpr unsigned char lineFeedByte { 0x0a };
static constexpr unsigned char formFeedByte { 0x0c };
static constexpr unsigned char carriageReturnByte { 0x0d };
static constexpr unsigned char spaceByte { 0x20 };
static constexpr unsigned char deleteByte { 0x7f };
static constexpr unsigned char asciiLimit { 0x80 };
static constexpr unsigned char asciiControlLimit { 0x20 };
static constexpr unsigned char caretMask { 0x40 };
static constexpr unsigned char hexRadix { 0x10 };
static constexpr char caretPrefix { static_cast<char> (Chars::caret) };
static constexpr char escapeCharacter { static_cast<char> (Chars::backslash) };
static constexpr char byteEscapeMarker { static_cast<char> (Chars::lowerX) };
static constexpr char codepointEscapeMarker { static_cast<char> (Chars::lowerU) };
static constexpr char braceOpen { static_cast<char> (Chars::openBrace) };
static constexpr char braceClose { static_cast<char> (Chars::closeBrace) };
static constexpr char newlineChar { static_cast<char> (Chars::newline) };
static constexpr int firstLineNumber { 1 };
static constexpr char zeroChar { static_cast<char> (Chars::zero) };
static constexpr char32_t controlPicturesStart { 0x2400 };
static constexpr char32_t deletePicture { 0x2421 };
static constexpr char32_t replacementCodepoint { 0xfffd };
static constexpr size_t singleByteLength { 1 };
static constexpr size_t singleColumn { 1 };
static constexpr size_t byteEscapeColumns { 4 };
static constexpr size_t tabStopWidth { 4 };
static constexpr size_t singleFingerprint { 1 };
static constexpr std::array tabFills { text::Formats::tabFillOne, text::Formats::tabFillTwo,
                                       text::Formats::tabFillThree, text::Formats::tabFillFour };

static constexpr jam::simd::NibbleSet controlClass { 0xffff, 0x0003 };
static constexpr jam::simd::NibbleSet spaceClass { 0x0001, 0x0004 };
static constexpr jam::simd::NibbleSet deleteClass { 0x8000, 0x0080 };
static constexpr jam::simd::NibbleSet highClass { 0xffff, 0xff00 };
static constexpr jam::simd::NibbleSet asciiClass { 0xffff, 0x00ff };
static constexpr std::array specialClasses { controlClass, spaceClass, deleteClass, highClass };
static constexpr std::array sanitizeClasses { controlClass, deleteClass, highClass };
static constexpr std::array asciiClasses { asciiClass };
static constexpr auto specialLowMasks { jam::simd::getLowMasks (specialClasses) };
static constexpr auto specialHighMasks { jam::simd::getHighMasks (specialClasses) };
static constexpr auto sanitizeLowMasks { jam::simd::getLowMasks (sanitizeClasses) };
static constexpr auto sanitizeHighMasks { jam::simd::getHighMasks (sanitizeClasses) };
static constexpr auto asciiLowMasks { jam::simd::getLowMasks (asciiClasses) };
static constexpr auto asciiHighMasks { jam::simd::getHighMasks (asciiClasses) };
static constexpr jam::simd::NibbleSet tabOrNewlineClass { 0x0600, 0x0001 };
static constexpr std::array tabOrNewlineClasses { tabOrNewlineClass };
static constexpr auto tabOrNewlineLowMasks { jam::simd::getLowMasks (tabOrNewlineClasses) };
static constexpr auto tabOrNewlineHighMasks { jam::simd::getHighMasks (tabOrNewlineClasses) };

static constexpr std::array<std::pair<char32_t, char32_t>, 7> sanitizedRanges {{
    { 0x0080, 0x009f },
    { 0x061c, 0x061c },
    { 0x200b, 0x200f },
    { 0x202a, 0x202e },
    { 0x2060, 0x2064 },
    { 0x2066, 0x206f },
    { 0xfeff, 0xfeff }
}};

static void appendUnicodeControl (std::string& output, unsigned char byte)
{
    if (byte == deleteByte)
        jam::Format::appendUtf8 (output, static_cast<juce::juce_wchar> (deletePicture));
    else
        jam::Format::appendUtf8 (output, static_cast<juce::juce_wchar> (controlPicturesStart + byte));
}

static void appendCaretControl (std::string& output, unsigned char byte)
{
    jam::Format::appendByte (output, caretPrefix);
    jam::Format::appendByte (output, static_cast<char> (byte ^ caretMask));
}

static constexpr std::array controlAppenders { &appendUnicodeControl, &appendCaretControl };

static constexpr std::array<std::pair<std::string_view, NonPrintable::Notation>, 2> notationNames {{
    { Id::unicodeNotation, NonPrintable::Notation::unicode },
    { Id::caretNotation, NonPrintable::Notation::caret }
}};

static void appendControlByte (std::string& output, unsigned char byte, NonPrintable::Notation notation)
{
    controlAppenders.at (static_cast<size_t> (notation)) (output, byte);

    if (byte == lineFeedByte)
        jam::Format::appendByte (output, newlineChar);
}

static void appendCodepointEscape (std::string& output, char32_t codepoint)
{
    jam::Format::appendByte (output, escapeCharacter);
    jam::Format::appendByte (output, codepointEscapeMarker);
    jam::Format::appendByte (output, braceOpen);
    jam::Format::appendHex (output, static_cast<std::uint64_t> (codepoint));
    jam::Format::appendByte (output, braceClose);
}

static jam::Array<char32_t> getCodepoints (std::string_view sequence)
{
    jam::Array<char32_t> codepoints;
    codepoints.resize (static_cast<int> (jam::Utf8::getCodepointCount (sequence)));
    jam::Utf8::setCodepoints (sequence, juce::Span<char32_t> { codepoints.data(), static_cast<size_t> (codepoints.size()) });
    return codepoints;
}

static std::pair<size_t, size_t> appendAsciiSpecial (std::string& output, unsigned char byte, NonPrintable::Notation notation, size_t column)
{
    if (byte == tabByte)
    {
        const size_t tabWidth { tabStopWidth - column % tabStopWidth };
        jam::Format::appendBytes (output, tabFills.at (tabWidth - singleColumn));
        return { singleByteLength, column + tabWidth };
    }

    if (byte == spaceByte)
    {
        jam::Format::appendBytes (output, text::Formats::spaceNotation);
        return { singleByteLength, column + singleColumn };
    }

    appendControlByte (output, byte, notation);
    const size_t nextColumn { byte == lineFeedByte ? 0 : column + singleColumn };
    return { singleByteLength, nextColumn };
}

static size_t findClassPosition (std::string_view bytes,
                                 size_t cursor,
                                 const jam::simd::FingerprintMasks& lowMasks,
                                 const jam::simd::FingerprintMasks& highMasks)
{
    const auto [position, classBits] { jam::simd::findFingerprint (bytes.data(), bytes.size(), cursor, lowMasks, highMasks, singleFingerprint) };
    juce::ignoreUnused (classBits);
    return position;
}

static size_t getSpecialPosition (std::string_view bytes, size_t cursor)
{
    return findClassPosition (bytes, cursor, specialLowMasks, specialHighMasks);
}

static size_t getRunEnd (std::string_view bytes, size_t position)
{
    return findClassPosition (bytes, position, asciiLowMasks, asciiHighMasks);
}

static size_t appendVisibleCodepoints (std::string& output, std::string_view sequence, size_t column)
{
    const auto codepoints (getCodepoints (sequence));

    for (const auto codepoint : codepoints)
        appendCodepointEscape (output, codepoint);

    return column + jam::Utf8::getCodepointCount (sequence);
}

static size_t appendVisibleByte (std::string& output, unsigned char byte, size_t column)
{
    NonPrintable::appendByteEscape (output, byte);
    return column + byteEscapeColumns;
}

static bool isSanitizedCodepoint (char32_t codepoint)
{
    for (const auto& [firstCodepoint, lastCodepoint] : sanitizedRanges)
        if (codepoint >= firstCodepoint and codepoint <= lastCodepoint) return true;

    return false;
}

static void appendReplacement (std::string& output)
{
    jam::Format::appendUtf8 (output, static_cast<juce::juce_wchar> (replacementCodepoint));
}

static size_t appendSanitizedCodepoints (std::string& output, std::string_view sequence, size_t column)
{
    const auto codepoints (getCodepoints (sequence));

    for (const auto codepoint : codepoints)
    {
        if (isSanitizedCodepoint (codepoint))
            appendReplacement (output);
        else
            jam::Format::appendUtf8 (output, static_cast<juce::juce_wchar> (codepoint));
    }

    return column;
}

static size_t appendSanitizedByte (std::string& output, unsigned char byte, size_t column)
{
    if (isSanitizedCodepoint (static_cast<char32_t> (byte)))
        appendReplacement (output);
    else
        jam::Format::appendByte (output, static_cast<char> (byte));

    return column;
}

template <typename ValidWriter, typename InvalidWriter>
static size_t appendHighRun (std::string& output,
                             std::string_view run,
                             size_t column,
                             const ValidWriter& writeValid,
                             const InvalidWriter& writeInvalid)
{
    std::string_view rest { run };
    size_t nextColumn { column };

    while (not rest.empty())
    {
        const auto prefix { jam::Utf8::getValidPrefix (rest) };

        if (prefix.empty())
        {
            nextColumn = writeInvalid (output, static_cast<unsigned char> (rest.front()), nextColumn);
            rest.remove_prefix (singleByteLength);
        }
        else
        {
            nextColumn = writeValid (output, prefix, nextColumn);
            rest.remove_prefix (prefix.size());
        }
    }

    return nextColumn;
}

static std::pair<size_t, size_t> appendHighSpecial (std::string& output, std::string_view bytes, size_t position, size_t column)
{
    const size_t runEnd { getRunEnd (bytes, position) };
    const auto run { bytes.substr (position, runEnd - position) };
    return { run.size(), appendHighRun (output, run, column, appendVisibleCodepoints, appendVisibleByte) };
}

static std::pair<size_t, size_t> appendSpecial (std::string& output, std::string_view bytes, size_t position, NonPrintable::Notation notation, size_t column)
{
    const auto byte { static_cast<unsigned char> (bytes.at (position)) };

    if (byte < asciiLimit)
        return appendAsciiSpecial (output, byte, notation, column);

    return appendHighSpecial (output, bytes, position, column);
}

std::optional<NonPrintable::Notation> NonPrintable::getNotation (std::string_view notationName)
{
    for (const auto& [notationText, notation] : notationNames)
        if (notationText.compare (notationName) == 0) return notation;

    return std::nullopt;
}

std::string NonPrintable::getVisible (std::string_view bytes, Notation notation)
{
    std::string output;
    size_t cursor { 0 };
    size_t column { 0 };

    while (cursor < bytes.size())
    {
        const size_t specialPosition { getSpecialPosition (bytes, cursor) };
        const size_t plainLength { specialPosition - cursor };
        jam::Format::appendBytes (output, bytes.substr (cursor, plainLength));
        const size_t columnAfterPlain { column + plainLength };
        const auto [specialLength, specialColumn] { specialPosition < bytes.size()
                                                        ? appendSpecial (output, bytes, specialPosition, notation, columnAfterPlain)
                                                        : std::pair<size_t, size_t> { 0, columnAfterPlain } };
        cursor = specialPosition + specialLength;
        column = specialColumn;
    }

    return output;
}

static bool isUnsafeAscii (std::string_view bytes, size_t position)
{
    const auto byte { static_cast<unsigned char> (bytes.at (position)) };

    if (byte == deleteByte) return true;

    const bool isCarriageReturnLineFeed { byte == carriageReturnByte and position + singleByteLength < bytes.size()
                                          and bytes.at (position + singleByteLength) == newlineChar };

    if (isCarriageReturnLineFeed) return false;

    return byte < asciiControlLimit and byte != tabByte and byte != lineFeedByte and byte != formFeedByte;
}

static size_t appendSanitized (std::string& output, std::string_view bytes, size_t position)
{
    const auto byte { static_cast<unsigned char> (bytes.at (position)) };

    if (byte < asciiLimit)
    {
        if (isUnsafeAscii (bytes, position))
            appendReplacement (output);
        else
            jam::Format::appendByte (output, bytes.at (position));

        return position + singleByteLength;
    }

    const size_t runEnd { getRunEnd (bytes, position) };
    const auto run { bytes.substr (position, runEnd - position) };
    appendHighRun (output, run, 0, appendSanitizedCodepoints, appendSanitizedByte);
    return runEnd;
}

std::string NonPrintable::getSanitized (std::string_view bytes)
{
    std::string output;
    size_t cursor { 0 };

    while (cursor < bytes.size())
    {
        const size_t candidate { findClassPosition (bytes, cursor, sanitizeLowMasks, sanitizeHighMasks) };
        jam::Format::appendBytes (output, bytes.substr (cursor, candidate - cursor));
        cursor = candidate;

        if (cursor < bytes.size())
            cursor = appendSanitized (output, bytes, cursor);
    }

    return output;
}

static size_t appendToken (std::string& output, std::string_view bytes, const jam::Document::Token& token)
{
    if (token.type == map::AnsiTokenType::unterminated)
    {
        jam::Format::appendBytes (output, bytes.substr (token.offset()));
        return bytes.size();
    }

    if (token.type == map::AnsiTokenType::text or token.type == map::AnsiTokenType::control)
        jam::Format::appendBytes (output, bytes.substr (token.offset(), token.length()));

    return token.span.getEnd();
}

std::string NonPrintable::getWithoutSequences (std::string_view bytes)
{
    std::string output;
    size_t offset { 0 };

    while (offset < bytes.size())
        offset = appendToken (output, bytes, jam::AnsiSequence::getToken (jam::Document::Cursor { bytes, offset }));

    return output;
}

static void appendSpaces (std::string& output, size_t count)
{
    size_t remaining { count };

    while (remaining > Id::spaceChunk.size())
    {
        jam::Format::appendBytes (output, Id::spaceChunk);
        remaining -= Id::spaceChunk.size();
    }

    jam::Format::appendBytes (output, Id::spaceChunk.substr (0, remaining));
}

static size_t getTabOrNewlinePosition (std::string_view bytes, size_t cursor)
{
    return findClassPosition (bytes, cursor, tabOrNewlineLowMasks, tabOrNewlineHighMasks);
}

static size_t appendStop (std::string& output, char stopByte, size_t column, size_t tabWidth)
{
    if (stopByte == newlineChar)
    {
        jam::Format::appendByte (output, newlineChar);
        return 0;
    }

    const size_t spaceCount { tabWidth - column % tabWidth };
    appendSpaces (output, spaceCount);
    return column + spaceCount;
}

std::string NonPrintable::getExpanded (std::string_view bytes, size_t tabWidth)
{
    jassert (tabWidth > 0);
    std::string output;
    size_t cursor { 0 };
    size_t column { 0 };

    while (cursor < bytes.size())
    {
        const size_t stop { getTabOrNewlinePosition (bytes, cursor) };
        const auto run { bytes.substr (cursor, stop - cursor) };
        jam::Format::appendBytes (output, run);
        column += jam::Utf8::getCodepointCount (run);

        if (stop < bytes.size())
            column = appendStop (output, bytes.at (stop), column, tabWidth);

        cursor = stop + singleByteLength;
    }

    return output;
}

static size_t getLineEnd (std::string_view bytes, size_t lineStart)
{
    return jam::simd::findByte (bytes.data(), bytes.size(), lineStart, newlineChar);
}

static size_t getNextLineStart (std::string_view bytes, size_t lineEnd)
{
    return lineEnd < bytes.size() ? lineEnd + singleByteLength : bytes.size();
}

static bool isEmptyLine (std::string_view line)
{
    return line.empty() or (line.size() == singleByteLength and static_cast<unsigned char> (line.front()) == carriageReturnByte);
}

jam::Array<int> NonPrintable::getSqueezedLines (std::string_view bytes, int limit)
{
    jam::Array<int> lineNumbers;
    size_t lineStart { 0 };
    int lineNumber { firstLineNumber };
    int emptyRun { 0 };

    while (lineStart < bytes.size())
    {
        const size_t lineEnd { getLineEnd (bytes, lineStart) };
        emptyRun = isEmptyLine (bytes.substr (lineStart, lineEnd - lineStart)) ? emptyRun + 1 : 0;

        if (emptyRun <= limit)
            lineNumbers.add (lineNumber);

        lineStart = getNextLineStart (bytes, lineEnd);
        ++lineNumber;
    }

    return lineNumbers;
}

std::string NonPrintable::getLinesText (std::string_view bytes, const jam::Array<int>& lineNumbers)
{
    std::string text;
    size_t lineStart { 0 };
    int lineNumber { firstLineNumber };
    int index { 0 };

    while (lineStart < bytes.size() and index < lineNumbers.size())
    {
        const size_t lineEnd { getLineEnd (bytes, lineStart) };
        const size_t nextStart { getNextLineStart (bytes, lineEnd) };

        if (lineNumbers.at (index) == lineNumber)
        {
            jam::Format::appendBytes (text, bytes.substr (lineStart, nextStart - lineStart));
            ++index;
        }

        lineStart = nextStart;
        ++lineNumber;
    }

    return text;
}

bool NonPrintable::isUtf16 (std::string_view bytes)
{
    return bytes.size() >= sizeof (juce::CharPointer_UTF16::CharType) and (juce::CharPointer_UTF16::isByteOrderMarkLittleEndian (bytes.data()) or juce::CharPointer_UTF16::isByteOrderMarkBigEndian (bytes.data()));
}

std::string NonPrintable::getDecoded (std::string_view bytes)
{
    jassert (isUtf16 (bytes));
    const juce::String decoded { juce::String::createStringFromData (bytes.data(), static_cast<int> (bytes.size())) };
    return std::string { jam::Strings::getView (decoded) };
}

void NonPrintable::appendByteEscape (std::string& output, unsigned char byte)
{
    jam::Format::appendByte (output, escapeCharacter);
    jam::Format::appendByte (output, byteEscapeMarker);

    if (byte < hexRadix)
        jam::Format::appendByte (output, zeroChar);

    jam::Format::appendHex (output, byte);
}
