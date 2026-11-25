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

static bool isWordByte (char byte) noexcept
{
    return jam::Document::isLetter (static_cast<unsigned char> (byte)) or jam::Document::isDigit (static_cast<unsigned char> (byte)) or byte == static_cast<char> (Chars::underscore);
}

static std::string getFoldedText (std::string_view sourceText, bool isIgnoreCase)
{
    std::string folded { sourceText };
    if (isIgnoreCase) std::transform (folded.begin(), folded.end(), folded.begin(), getLowerCaseAscii);
    return folded;
}

static std::string getEscapedLiteralRegex (std::string_view literal, bool isIgnoreCase)
{
    std::string regex;

    for (const char byte : literal)
    {
        if (isIgnoreCase and jam::Document::isLetter (static_cast<unsigned char> (byte)))
        {
            const char lowerByte { getLowerCaseAscii (byte) };
            jam::Format::appendUtf8 (regex, Chars::openBracket);
            jam::Format::appendUtf8 (regex, lowerByte);
            jam::Format::appendUtf8 (regex, getUpperCaseAscii (lowerByte));
            jam::Format::appendUtf8 (regex, Chars::closeBracket);
        }
        else
        {
            jam::RegExp::appendEscaped (regex, byte);
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

static juce::Result getTextsValidity (juce::Span<const std::string_view> patternTexts)
{
    for (const std::string_view patternText : patternTexts)
    {
        if (patternText.empty()) return Stdio::fail (text::Diagnostics::failPatternEmpty);

        if (patternText.find (static_cast<char> (Chars::newline)) != std::string_view::npos) return Stdio::fail (text::Diagnostics::failPatternNewline);
    }

    return juce::Result::ok();
}

static std::string getRegexSource (juce::Span<const std::string_view> patternTexts, bool isRegex, bool isIgnoreCase)
{
    if (patternTexts.size() == 1) return std::string { patternTexts.front() };

    std::string source;

    for (size_t index { 0 }; index < patternTexts.size(); ++index)
    {
        const std::string_view patternText { patternTexts[index] };

        if (index > 0) jam::Format::appendUtf8 (source, Chars::pipe);

        jam::Format::appendBytes (source, Id::groupRegexPrefix);
        jam::Format::appendBytes (source, isRegex ? patternText : getEscapedLiteralRegex (patternText, isIgnoreCase));
        jam::Format::appendUtf8 (source, Chars::closeParen);
    }

    return source;
}

static std::string getJoinedTexts (juce::Span<const std::string_view> patternTexts)
{
    std::string joined;

    for (size_t index { 0 }; index < patternTexts.size(); ++index)
    {
        if (index > 0) jam::Format::appendUtf8 (joined, Chars::pipe);

        jam::Format::appendBytes (joined, patternTexts[index]);
    }

    return joined;
}

static std::vector<std::string> getPrefilterLiterals (juce::Span<const std::string_view> patternTexts, bool isIgnoreCase)
{
    std::vector<std::string> literals;

    for (const std::string_view patternText : patternTexts)
        literals.push_back (getFoldedText (patternText, isIgnoreCase));

    return literals;
}

static std::vector<std::string> getPrefilterLiterals (const std::string& compiledPattern, const jam::RegExp& regExp, bool isIgnoreCase)
{
    if (jam::Utf8::isValid (compiledPattern))
        return regExp.getRequiredLiterals (isIgnoreCase);

    return {};
}

static std::unique_ptr<jam::LiteralSet> getLiteralSet (std::vector<std::string> literals, bool isIgnoreCase)
{
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
        jam::Format::appendBytes (compiled, isIgnoreCase ? Id::exactIgnoreCaseRegexPrefix : Id::exactRegexPrefix);
        jam::Format::appendBytes (compiled, rawPattern);
        jam::Format::appendBytes (compiled, Id::exactRegexSuffix);
    }
    else if (isIgnoreCase)
    {
        jam::Format::appendBytes (compiled, Id::ignoreCaseRegexPrefix);
        jam::Format::appendBytes (compiled, rawPattern);
        jam::Format::appendBytes (compiled, Id::ignoreCaseRegexSuffix);
    }
    else
    {
        jam::Format::appendBytes (compiled, rawPattern);
    }

    if (not isWord)
        return compiled;

    std::string wordCompiled;
    jam::Format::appendBytes (wordCompiled, Id::wordRegexPrefix);
    jam::Format::appendBytes (wordCompiled, compiled);
    jam::Format::appendBytes (wordCompiled, Id::wordRegexSuffix);
    return wordCompiled;
}

Pattern::Pattern (Access,
                  std::unique_ptr<jam::RegExp> newRegExp,
                  std::unique_ptr<jam::RegExpProgram> newProgram,
                  size_t newLiteralSize,
                  std::unique_ptr<jam::LiteralSet> newLiteralSet,
                  std::string newPatternText,
                  bool newIsCandidateMatch,
                  bool newIsExactLine,
                  bool newIsWord)
    : regExp (std::move (newRegExp)),
      program (std::move (newProgram)),
      literalSize (newLiteralSize),
      literalSet (std::move (newLiteralSet)),
      patternText (std::move (newPatternText)),
      isCandidateMatch (newIsCandidateMatch),
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
        return isCandidateMatch and not isExactLine and not isWord;

    return program != nullptr;
}

std::pair<juce::Result, std::unique_ptr<Pattern>> Pattern::create (juce::Span<const std::string_view> patternTexts,
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
        const size_t literalSize { patternTexts.front().size() };
        return { juce::Result::ok(), std::make_unique<Pattern> (Access {}, nullptr, nullptr, literalSize, getLiteralSet (getPrefilterLiterals (patternTexts, isIgnoreCase), isIgnoreCase), patternText, not hasCarriageReturn, isExactLine, isWord) };
    }

    const std::string rawPattern { getRegexSource (patternTexts, isRegex, isIgnoreCase) };
    const std::string compiledPattern { getCompiledRegexPattern (rawPattern, isIgnoreCase and isRegex, isExactLine, isWord) };

    try
    {
        auto compiled { std::make_unique<jam::RegExp> (compiledPattern) };
        auto program { jam::RegExpProgram::create (*compiled) };
        auto literals (isRegex ? getPrefilterLiterals (compiledPattern, *compiled, isIgnoreCase) : getPrefilterLiterals (patternTexts, isIgnoreCase));
        return { juce::Result::ok(), std::make_unique<Pattern> (Access {}, std::move (compiled), std::move (program), size_t {}, getLiteralSet (std::move (literals), isIgnoreCase), patternText, not isRegex and not hasCarriageReturn, isExactLine, isWord) };
    }
    catch (const std::invalid_argument& error)
    {
        return { Stdio::fail (text::Diagnostics::failRegexInvalid, std::string_view { error.what() }), nullptr };
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
        const bool isExactMatch { not isExactLine or (offset == 0 and line.size() == literalSize) };
        Captures captures {};

        if (pos < line.size() and isExactMatch) captures.add (line.substr (pos, literalSize));

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
        return { Stdio::fail (text::Diagnostics::failRegexSteps, std::string_view { patternText }), Captures {} };
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
