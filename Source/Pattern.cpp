#include "Pattern.h"
#include "generated/Generated.h"

static bool isCaptureGroupStart (std::string_view patternText, size_t index) noexcept
{
    if (index + 1 >= patternText.size() or patternText.at (index + 1) != static_cast<char> (Chars::question))
        return true;

    return index + 3 < patternText.size()
           and patternText.at (index + 1) == static_cast<char> (Chars::question)
           and patternText.at (index + 2) == static_cast<char> (Chars::lessThan)
           and patternText.at (index + 3) != static_cast<char> (Chars::equals)
           and patternText.at (index + 3) != static_cast<char> (Chars::exclamation);
}

static char getLowerCaseAscii (char byte) noexcept
{
    if (byte >= static_cast<char> (Chars::upperA) and byte <= static_cast<char> (Chars::upperZ)) return static_cast<char> (byte + (Chars::lowerA - Chars::upperA));

    return byte;
}

static size_t getDelimitedEnd (std::string_view pattern, size_t start, char terminator) noexcept
{
    const size_t close { pattern.find (terminator, start) };
    return close != std::string_view::npos ? close : pattern.size() - 1;
}

static size_t getCharClassEnd (std::string_view pattern, size_t openIndex) noexcept
{
    for (size_t index { openIndex + 1 }; index < pattern.size(); ++index)
    {
        if (pattern.at (index) == static_cast<char> (Chars::backslash) and index + 1 < pattern.size()) ++index;
        else if (pattern.at (index) == static_cast<char> (Chars::closeBracket)) return index;
    }

    return pattern.size() - 1;
}

static size_t getGroupEnd (std::string_view pattern, size_t openIndex) noexcept
{
    int depth { 1 };

    for (size_t index { openIndex + 1 }; index < pattern.size(); ++index)
    {
        const char byte { pattern.at (index) };

        if (byte == static_cast<char> (Chars::backslash) and index + 1 < pattern.size())
            ++index;
        else if (byte == static_cast<char> (Chars::openBracket))
            index = getCharClassEnd (pattern, index);
        else
            depth += static_cast<int> (byte == static_cast<char> (Chars::openParen)) - static_cast<int> (byte == static_cast<char> (Chars::closeParen));

        if (depth == 0) return index;
    }

    return pattern.size() - 1;
}

static size_t getEscapeEnd (std::string_view pattern, size_t index) noexcept
{
    static constexpr size_t payloadOffset { 2 };
    static constexpr size_t hexSpan { 3 };
    static constexpr size_t controlSpan { 2 };
    static constexpr size_t unicodeSpan { 5 };
    static constexpr std::pair<juce::juce_wchar, size_t> fixedSpans[] { { Chars::lowerX, hexSpan }, { Chars::lowerC, controlSpan }, { Chars::lowerU, unicodeSpan } };

    if (index + 1 >= pattern.size()) return index;

    const char next { pattern.at (index + 1) };

    if (next == static_cast<char> (Chars::lowerP) or next == static_cast<char> (Chars::upperP)
        or (next == static_cast<char> (Chars::lowerU) and index + payloadOffset < pattern.size() and pattern.at (index + payloadOffset) == static_cast<char> (Chars::openBrace)))
        return getDelimitedEnd (pattern, index + payloadOffset, static_cast<char> (Chars::closeBrace));

    if (next == static_cast<char> (Chars::lowerK)) return getDelimitedEnd (pattern, index + payloadOffset, static_cast<char> (Chars::greaterThan));

    for (const auto& [escapeChar, span] : fixedSpans)
        if (next == static_cast<char> (escapeChar)) return std::min (index + span, pattern.size() - 1);

    size_t end { index + 1 };

    while (next >= static_cast<char> (Chars::one) and next <= static_cast<char> (Chars::nine) and end + 1 < pattern.size()
           and pattern.at (end + 1) >= static_cast<char> (Chars::zero) and pattern.at (end + 1) <= static_cast<char> (Chars::nine))
        ++end;

    return end;
}

static size_t getCodepointEnd (std::string_view pattern, size_t index) noexcept
{
    static constexpr unsigned char utf8ContinuationMask { 0xc0u };
    static constexpr unsigned char utf8ContinuationPrefix { 0x80u };
    size_t end { index + 1 };

    if (static_cast<unsigned char> (pattern.at (index)) >= Chars::nonAsciiStart)
        while (end < pattern.size() and (static_cast<unsigned char> (pattern.at (end)) & utf8ContinuationMask) == utf8ContinuationPrefix) ++end;

    return end;
}

static bool isOptionalNext (std::string_view pattern, size_t nextIndex) noexcept
{
    if (nextIndex >= pattern.size()) return false;

    const char next { pattern.at (nextIndex) };

    if (next == static_cast<char> (Chars::question) or next == static_cast<char> (Chars::asterisk)) return true;

    return next == static_cast<char> (Chars::openBrace) and nextIndex + 1 < pattern.size() and pattern.at (nextIndex + 1) == static_cast<char> (Chars::zero);
}

static void flushLiteral (std::string& current, std::string& best)
{
    if (current.size() > best.size()) best = std::move (current);

    current.clear();
}

static size_t getConstructEnd (std::string_view pattern, size_t index) noexcept
{
    using SpanEndFn = size_t (*) (std::string_view, size_t) noexcept;
    static constexpr std::pair<juce::juce_wchar, SpanEndFn> constructEnds[]
    {
        { Chars::openParen, getGroupEnd },
        { Chars::openBracket, getCharClassEnd },
        { Chars::backslash, getEscapeEnd }
    };

    if (pattern.at (index) == static_cast<char> (Chars::openBrace))
        return getDelimitedEnd (pattern, index + 1, static_cast<char> (Chars::closeBrace));

    for (const auto& [opener, endFunction] : constructEnds)
        if (pattern.at (index) == static_cast<char> (opener)) return endFunction (pattern, index);

    return index;
}

static std::string getRequiredLiteral (std::string_view pattern)
{
    static constexpr size_t minPrefilterLength { 2 };
    static constexpr std::string_view metaChars { "()[]{}.^$*+?\\" };
    std::string current;
    std::string best;

    for (size_t i { 0 }; i < pattern.size(); ++i)
    {
        if (pattern.at (i) == static_cast<char> (Chars::pipe)) return {};

        if (metaChars.find (pattern.at (i)) != std::string_view::npos)
        {
            flushLiteral (current, best);
            i = getConstructEnd (pattern, i);
            continue;
        }

        const size_t codepointEnd { getCodepointEnd (pattern, i) };

        if (isOptionalNext (pattern, codepointEnd)) flushLiteral (current, best);
        else current.append (pattern.substr (i, codepointEnd - i));

        i = codepointEnd - 1;
    }

    flushLiteral (current, best);
    return best.size() >= minPrefilterLength ? best : std::string {};
}

static size_t findBytes (std::string_view haystack, size_t offset, std::string_view needle) noexcept
{
    assert (offset <= haystack.size());
    assert (not needle.empty());

    if (needle.size() > haystack.size() or offset > haystack.size() - needle.size()) return haystack.size();

    const char* haystackData { haystack.data() };
    const char* needleData { needle.data() };
    const size_t needleSize { needle.size() };
    const size_t searchEnd { haystack.size() - needleSize + 1 };
    const char firstByte { needle.at (0) };
    size_t cursor { offset };

    while (cursor < searchEnd)
    {
        const size_t pos { jam::simd::findByte (haystackData, searchEnd, cursor, firstByte) };

        if (pos >= searchEnd) return haystack.size();

        if (needleSize == 1 or std::memcmp (haystackData + pos + 1, needleData + 1, needleSize - 1) == 0) return pos;

        cursor = pos + 1;
    }

    return haystack.size();
}

static size_t findIgnoreCaseBytes (std::string_view haystack, size_t offset, std::string_view needle) noexcept
{
    assert (offset <= haystack.size() and not needle.empty());

    if (needle.size() > haystack.size() or offset > haystack.size() - needle.size()) return haystack.size();

    const size_t searchEnd { haystack.size() - needle.size() + 1 };
    const char firstLower { needle.at (0) };
    const bool hasUpper { firstLower >= static_cast<char> (Chars::lowerA) and firstLower <= static_cast<char> (Chars::lowerZ) };
    const char firstUpper { hasUpper ? static_cast<char> (firstLower - (Chars::lowerA - Chars::upperA)) : firstLower };
    size_t cursor { offset };
    size_t lowerPos { jam::simd::findByte (haystack.data(), searchEnd, cursor, firstLower) };
    size_t upperPos { hasUpper ? jam::simd::findByte (haystack.data(), searchEnd, cursor, firstUpper) : searchEnd };

    while (cursor < searchEnd)
    {
        if (lowerPos < cursor) lowerPos = jam::simd::findByte (haystack.data(), searchEnd, cursor, firstLower);
        if (upperPos < cursor) upperPos = jam::simd::findByte (haystack.data(), searchEnd, cursor, firstUpper);

        const size_t pos { std::min (lowerPos, upperPos) };

        if (pos >= searchEnd) return haystack.size();

        if (std::equal (needle.data() + 1, needle.data() + needle.size(), haystack.data() + pos + 1, [] (char expected, char actual) noexcept { return getLowerCaseAscii (actual) == expected; })) return pos;

        cursor = pos + 1;
    }

    return haystack.size();
}

Pattern::Pattern (Access,
                  std::unique_ptr<jam::RegExp> newRegExp,
                  std::string newLiteral,
                  std::string newPrefilterLiteral,
                  int newNumCaptureGroups,
                  bool newIsIgnoreCase,
                  bool newIsExactLine)
    : regExp (std::move (newRegExp)),
      literal (std::move (newLiteral)),
      prefilterLiteral (std::move (newPrefilterLiteral)),
      numCaptureGroups (newNumCaptureGroups),
      isIgnoreCase (newIsIgnoreCase),
      isExactLine (newIsExactLine)
{
}

static std::string getCompiledRegexPattern (const std::string& rawPattern,
                                            bool isIgnoreCase,
                                            bool isExactLine)
{
    if (isExactLine)
    {
        const auto& prefix { isIgnoreCase ? Id::exactIgnoreCaseRegexPrefix : Id::exactRegexPrefix };
        return prefix.toStdString() + rawPattern + Id::exactRegexSuffix.toStdString();
    }

    if (isIgnoreCase)
        return Id::ignoreCaseRegexPrefix.toStdString() + rawPattern + Id::ignoreCaseRegexSuffix.toStdString();

    return rawPattern;
}

std::pair<juce::Result, std::unique_ptr<Pattern>> Pattern::create (const juce::String& patternText,
                                                                   bool isRegex,
                                                                   bool isIgnoreCase,
                                                                   bool isExactLine)
{
    if (patternText.isEmpty()) return { juce::Result::fail (text::Diagnostics::failPatternEmpty), nullptr };

    const std::string rawPattern { patternText.toStdString() };

    if (isRegex)
    {
        const std::string compiledPattern { getCompiledRegexPattern (rawPattern, isIgnoreCase, isExactLine) };
        try
        {
            auto compiled { std::make_unique<jam::RegExp> (compiledPattern) };
            std::string prefilter { isIgnoreCase ? std::string {} : getRequiredLiteral (rawPattern) };
            return { juce::Result::ok(), std::make_unique<Pattern> (Access {}, std::move (compiled), std::string {}, std::move (prefilter), getCaptureGroupCount (rawPattern), isIgnoreCase, isExactLine) };
        }
        catch (const std::invalid_argument& error)
        {
            return { juce::Result::fail (juce::String::formatted (text::Diagnostics::failRegexInvalid, error.what())), nullptr };
        }
    }

    std::string folded { rawPattern };

    if (isIgnoreCase) std::transform (folded.begin(), folded.end(), folded.begin(), getLowerCaseAscii);

    return { juce::Result::ok(), std::make_unique<Pattern> (Access {}, nullptr, std::move (folded), isIgnoreCase ? std::string {} : rawPattern, 0, isIgnoreCase, isExactLine) };
}

jam::Array<std::optional<std::string_view>> Pattern::findNext (std::string_view line, size_t offset) const
{
    if (regExp != nullptr) return regExp->findNext (line, offset);

    const size_t pos { findLiteralOffset (line, offset) };

    if (pos < line.size())
    {
        jam::Array<std::optional<std::string_view>> captures;
        captures.add (line.substr (pos, literal.size()));
        return captures;
    }

    return {};
}

bool Pattern::hasMatch (std::string_view line) const
{
    if (regExp == nullptr)
    {
        if (isExactLine) return line.size() == literal.size() and findLiteralOffset (line, 0) == 0;

        return findLiteralOffset (line, 0) < line.size();
    }

    if (not prefilterLiteral.empty() and findBytes (line, 0, prefilterLiteral) >= line.size()) return false;

    return regExp->hasMatch (line);
}

int Pattern::getNumCaptureGroups() const noexcept
{
    return numCaptureGroups;
}

std::string_view Pattern::getPrefilterLiteral() const noexcept
{
    return prefilterLiteral;
}

int Pattern::getCaptureGroupCount (std::string_view patternText) noexcept
{
    int count { 0 };
    bool inCharClass { false };

    for (size_t index { 0 }; index < patternText.size(); ++index)
    {
        const char currentByte { patternText.at (index) };

        if (currentByte == static_cast<char> (Chars::backslash))
        {
            ++index;
            continue;
        }

        if (not inCharClass and currentByte == static_cast<char> (Chars::openParen) and isCaptureGroupStart (patternText, index))
            ++count;

        inCharClass = (inCharClass and currentByte != static_cast<char> (Chars::closeBracket))
                      or (not inCharClass and currentByte == static_cast<char> (Chars::openBracket));
    }

    return count;
}

size_t Pattern::findLiteralOffset (std::string_view line, size_t offset) const noexcept
{
    if (isIgnoreCase) return findIgnoreCaseBytes (line, offset, literal);

    return findBytes (line, offset, literal);
}
