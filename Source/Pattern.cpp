#include "Pattern.h"
#include "Stdio.h"
#include "generated/Generated.h"

static char getLowerCaseAscii (char byte) noexcept
{
    if (byte >= static_cast<char> (Chars::upperA) and byte <= static_cast<char> (Chars::upperZ)) return static_cast<char> (byte + (Chars::lowerA - Chars::upperA));

    return byte;
}

static char getUpperCaseAscii (char byte) noexcept
{
    if (byte >= static_cast<char> (Chars::lowerA) and byte <= static_cast<char> (Chars::lowerZ)) return static_cast<char> (byte - (Chars::lowerA - Chars::upperA));

    return byte;
}

static bool isAsciiLetter (char byte) noexcept
{
    const char lowerByte { getLowerCaseAscii (byte) };
    return lowerByte >= static_cast<char> (Chars::lowerA) and lowerByte <= static_cast<char> (Chars::lowerZ);
}

static bool isWordByte (char byte) noexcept
{
    return isAsciiLetter (byte) or (byte >= static_cast<char> (Chars::zero) and byte <= static_cast<char> (Chars::nine)) or byte == static_cast<char> (Chars::underscore);
}

static std::string getFoldedText (std::string_view sourceText, bool isIgnoreCase)
{
    std::string folded { sourceText };
    if (isIgnoreCase) std::transform (folded.begin(), folded.end(), folded.begin(), getLowerCaseAscii);
    return folded;
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

static std::string getLongerLiteral (std::string current, std::string best)
{
    if (current.size() > best.size()) return current;

    return best;
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

static bool isCaseFoldBreak (char byte) noexcept
{
    const char foldedByte { getLowerCaseAscii (byte) };
    return foldedByte == static_cast<char> (Chars::lowerK) or foldedByte == static_cast<char> (Chars::lowerS) or static_cast<unsigned char> (byte) >= Chars::nonAsciiStart;
}

static std::vector<std::string> getRequiredLiterals (std::string_view pattern, bool isIgnoreCase)
{
    static constexpr size_t minPrefilterLength { 2 };
    static constexpr std::string_view metaChars { "()[]{}.^$*+?\\" };
    std::vector<std::string> branches;
    std::string current;
    std::string best;

    for (size_t i { 0 }; i < pattern.size(); ++i)
    {
        if (pattern.at (i) == static_cast<char> (Chars::pipe))
        {
            best = getLongerLiteral (std::move (current), std::move (best));
            current.clear();
            branches.push_back (std::move (best));
            best.clear();
        }
        else if (metaChars.find (pattern.at (i)) != std::string_view::npos)
        {
            best = getLongerLiteral (std::move (current), std::move (best));
            current.clear();
            i = getConstructEnd (pattern, i);
        }
        else
        {
            const size_t codepointEnd { getCodepointEnd (pattern, i) };

            if (isOptionalNext (pattern, codepointEnd) or (isIgnoreCase and isCaseFoldBreak (pattern.at (i))))
            {
                best = getLongerLiteral (std::move (current), std::move (best));
                current.clear();
            }
            else
            {
                current.append (getFoldedText (pattern.substr (i, codepointEnd - i), isIgnoreCase));
            }

            i = codepointEnd - 1;
        }
    }

    best = getLongerLiteral (std::move (current), std::move (best));
    current.clear();
    branches.push_back (std::move (best));

    const bool isEveryBranchLong { std::all_of (branches.begin(), branches.end(), [] (const std::string& branch) noexcept { return branch.size() >= minPrefilterLength; }) };

    if (isEveryBranchLong) return branches;

    return {};
}

static std::string getEscapedLiteralRegex (std::string_view literal, bool isIgnoreCase)
{
    static constexpr std::string_view regexMetacharacters { "\\^$.*+?()[]{}|/" };
    std::string regex;

    for (const char byte : literal)
    {
        if (isIgnoreCase and isAsciiLetter (byte))
        {
            const char lowerByte { getLowerCaseAscii (byte) };
            regex.push_back (static_cast<char> (Chars::openBracket));
            regex.push_back (lowerByte);
            regex.push_back (getUpperCaseAscii (lowerByte));
            regex.push_back (static_cast<char> (Chars::closeBracket));
        }
        else
        {
            if (regexMetacharacters.find (byte) != std::string_view::npos) regex.push_back (static_cast<char> (Chars::backslash));

            regex.push_back (byte);
        }
    }

    return regex;
}

static size_t getMatchOffset (std::string_view line, std::string_view match) noexcept
{
    return static_cast<size_t> (match.data() - line.data());
}

static bool isWordAdjacent (std::string_view line, std::string_view match) noexcept
{
    const size_t start { getMatchOffset (line, match) };
    const size_t end { start + match.size() };
    return (start > 0 and isWordByte (line.at (start - 1))) or (end < line.size() and isWordByte (line.at (end)));
}

static juce::Result getTextsValidity (const jam::Strings& patternTexts)
{
    for (int index { 0 }; index < patternTexts.size(); ++index)
    {
        const std::string_view patternText { patternTexts.getView (index) };

        if (patternText.empty()) return juce::Result::fail (text::Diagnostics::failPatternEmpty);

        if (patternText.find (static_cast<char> (Chars::newline)) != std::string_view::npos) return juce::Result::fail (text::Diagnostics::failPatternNewline);
    }

    return juce::Result::ok();
}

static std::string getRegexSource (const jam::Strings& patternTexts, bool isRegex, bool isIgnoreCase)
{
    if (patternTexts.size() == 1) return std::string { patternTexts.getView (0) };

    std::string source;

    for (int index { 0 }; index < patternTexts.size(); ++index)
    {
        const std::string_view patternText { patternTexts.getView (index) };

        if (index > 0) source.push_back (static_cast<char> (Chars::pipe));

        source.append ("(?:");
        source.append (isRegex ? std::string { patternText } : getEscapedLiteralRegex (patternText, isIgnoreCase));
        source.push_back (static_cast<char> (Chars::closeParen));
    }

    return source;
}

static std::string getJoinedTexts (const jam::Strings& patternTexts)
{
    std::string joined;

    for (int index { 0 }; index < patternTexts.size(); ++index)
    {
        if (index > 0) joined.push_back (static_cast<char> (Chars::pipe));

        joined.append (patternTexts.getView (index));
    }

    return joined;
}

static std::vector<std::string> getPrefilterLiterals (const jam::Strings& patternTexts, bool isRegex, bool isIgnoreCase)
{
    std::vector<std::string> literals;

    for (int index { 0 }; index < patternTexts.size(); ++index)
    {
        const std::string_view patternText { patternTexts.getView (index) };

        if (isRegex)
        {
            const auto required { getRequiredLiterals (patternText, isIgnoreCase) };

            if (required.empty()) return {};

            literals.insert (literals.end(), required.begin(), required.end());
        }
        else
        {
            literals.push_back (getFoldedText (patternText, isIgnoreCase));
        }
    }

    return literals;
}

static std::unique_ptr<jam::LiteralSet> getLiteralSet (const jam::Strings& patternTexts, bool isRegex, bool isIgnoreCase)
{
    auto literals (getPrefilterLiterals (patternTexts, isRegex, isIgnoreCase));

    if (literals.empty()) return nullptr;

    return jam::LiteralSet::create (std::move (literals), isIgnoreCase);
}

static std::string getCompiledRegexPattern (const std::string& rawPattern,
                                            bool isIgnoreCase,
                                            bool isExactLine,
                                            bool isWord)
{
    std::string compiled;

    if (isExactLine)
    {
        compiled.append (isIgnoreCase ? Id::exactIgnoreCaseRegexPrefix : Id::exactRegexPrefix);
        compiled.append (rawPattern);
        compiled.append (Id::exactRegexSuffix);
    }
    else if (isIgnoreCase)
    {
        compiled.append (Id::ignoreCaseRegexPrefix);
        compiled.append (rawPattern);
        compiled.append (Id::ignoreCaseRegexSuffix);
    }
    else
    {
        compiled.append (rawPattern);
    }

    if (not isWord)
        return compiled;

    std::string wordCompiled;
    wordCompiled.append (Id::wordRegexPrefix);
    wordCompiled.append (compiled);
    wordCompiled.append (Id::wordRegexSuffix);
    return wordCompiled;
}

Pattern::Pattern (Access,
                  std::unique_ptr<jam::RegExp> newRegExp,
                  std::unique_ptr<jam::RegExpProgram> newProgram,
                  std::string newLiteral,
                  std::unique_ptr<jam::LiteralSet> newLiteralSet,
                  std::string newPatternText,
                  bool newIsLiteral,
                  bool newIsIgnoreCase,
                  bool newIsExactLine,
                  bool newIsWord)
    : regExp (std::move (newRegExp)),
      program (std::move (newProgram)),
      literal (std::move (newLiteral)),
      literalSet (std::move (newLiteralSet)),
      patternText (std::move (newPatternText)),
      isLiteral (newIsLiteral),
      isIgnoreCase (newIsIgnoreCase),
      isExactLine (newIsExactLine),
      isWord (newIsWord)
{
}

size_t Pattern::findCandidate (std::string_view bytes, size_t offset, jam::RegExpAutomaton& automaton) const
{
    assert (offset <= bytes.size());

    if (literalSet != nullptr)
        return literalSet->findNext (bytes, offset);

    if (program != nullptr)
        return automaton.findMatchingLine (*regExp, *program, bytes, offset);

    return offset;
}

bool Pattern::isConfirmed() const noexcept
{
    if (literalSet != nullptr)
        return isLiteral and not isExactLine and not isWord;

    return program != nullptr;
}

std::pair<juce::Result, std::unique_ptr<Pattern>> Pattern::create (const jam::Strings& patternTexts,
                                                                   bool isRegex,
                                                                   bool isIgnoreCase,
                                                                   bool isExactLine,
                                                                   bool isWord)
{
    assert (patternTexts.size() > 0);
    const juce::Result validity { getTextsValidity (patternTexts) };

    if (validity.failed()) return { validity, nullptr };

    const std::string patternText { getJoinedTexts (patternTexts) };
    const bool hasCarriageReturn { patternText.find (static_cast<char> (Chars::carriageReturn)) != std::string::npos };

    if (patternTexts.size() == 1 and not isRegex)
    {
        const std::string literal { getFoldedText (patternTexts.getView (0), isIgnoreCase) };
        return { juce::Result::ok(), std::make_unique<Pattern> (Access {}, nullptr, nullptr, literal, getLiteralSet (patternTexts, isRegex, isIgnoreCase), patternText, not hasCarriageReturn, isIgnoreCase, isExactLine, isWord) };
    }

    const std::string rawPattern { getRegexSource (patternTexts, isRegex, isIgnoreCase) };
    const std::string compiledPattern { getCompiledRegexPattern (rawPattern, isIgnoreCase and isRegex, isExactLine, isWord) };

    try
    {
        auto compiled { std::make_unique<jam::RegExp> (compiledPattern) };
        auto program { jam::RegExpProgram::create (*compiled) };
        return { juce::Result::ok(), std::make_unique<Pattern> (Access {}, std::move (compiled), std::move (program), std::string {}, getLiteralSet (patternTexts, isRegex, isIgnoreCase), patternText, not isRegex and not hasCarriageReturn, isIgnoreCase, isExactLine, isWord) };
    }
    catch (const std::invalid_argument& error)
    {
        return { juce::Result::fail (Stdio::getFormatted (text::Diagnostics::failRegexInvalid, error.what())), nullptr };
    }
}

std::pair<juce::Result, Pattern::Captures> Pattern::findNext (std::string_view line, size_t offset) const
{
    if (regExp != nullptr)
        return findSpan (line, offset);

    size_t searchOffset { offset };

    while (true)
    {
        auto [result, captures] { findSpan (line, searchOffset) };

        if (result.failed() or not isWord or captures.isEmpty() or not isWordAdjacent (line, *captures.at (0)))
            return { std::move (result), std::move (captures) };

        searchOffset = getMatchOffset (line, *captures.at (0)) + 1;
    }
}

std::pair<juce::Result, Pattern::Captures> Pattern::findSpan (std::string_view line, size_t offset) const
{
    if (offset > line.size()) return { juce::Result::ok(), Captures {} };

    if (regExp == nullptr)
    {
        const size_t pos { literalSet->findNext (line, offset) };
        const bool isExactMatch { not isExactLine or (offset == 0 and line.size() == literal.size()) };
        Captures captures {};

        if (pos < line.size() and isExactMatch) captures.add (line.substr (pos, literal.size()));

        return { juce::Result::ok(), std::move (captures) };
    }

    if (program != nullptr and not program->hasEmptyRepetition())
        return { juce::Result::ok(), program->findNext (*regExp, line, offset) };

    return findBacktracked (line, offset);
}

std::pair<juce::Result, Pattern::Captures> Pattern::findBacktracked (std::string_view line, size_t offset) const
{
    try
    {
        return { juce::Result::ok(), regExp->findNext (line, offset) };
    }
    catch (const std::overflow_error&)
    {
        return { juce::Result::fail (Stdio::getFormatted (text::Diagnostics::failRegexSteps, patternText.c_str())), Captures {} };
    }
}

std::pair<juce::Result, bool> Pattern::hasMatch (std::string_view line, jam::RegExpAutomaton& automaton) const
{
    if (program != nullptr)
        return { juce::Result::ok(), automaton.hasMatch (*regExp, *program, line) };

    if (regExp == nullptr)
    {
        const auto [result, captures] { findNext (line, 0) };
        return { result, result.wasOk() and captures.size() > 0 };
    }

    const bool isPossible { literalSet == nullptr or literalSet->findNext (line, 0) < line.size() };

    if (isPossible)
    {
        const auto [result, captures] { findBacktracked (line, 0) };
        return { result, result.wasOk() and captures.size() > 0 };
    }

    return { juce::Result::ok(), false };
}

int Pattern::getNumCaptureGroups() const noexcept
{
    if (regExp == nullptr) return 0;

    return regExp->getNumCaptures() - 1;
}
