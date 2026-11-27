#include "IgnoreRules.h"
#include "Paths.h"
#include "TextDocument.h"
#include "generated/Generated.h"

static constexpr std::string_view doubleStarPrefix { "**/" };
static constexpr std::string_view innerDoubleStar { "/**/" };
static constexpr std::string_view trailingDoubleStar { "/**" };
static constexpr std::string_view segmentRegex { "[^/]*" };
static constexpr std::string_view segmentCharacterRegex { "[^/]" };

static int getTrailingBackslashCount (std::string_view text, size_t endIndex) noexcept
{
    int count { 0 };

    while (endIndex > 0 and text.at (endIndex - 1) == static_cast<char> (Chars::backslash))
    {
        ++count;
        --endIndex;
    }

    return count;
}

static void appendConfigQuotedEscape (std::string& decoded, char escaped)
{
    static const jam::HashMap<char, char> configEscapes
    {
        { static_cast<char> (Chars::doubleQuote), static_cast<char> (Chars::doubleQuote) },
        { static_cast<char> (Chars::backslash), static_cast<char> (Chars::backslash) },
        { static_cast<char> (Chars::lowerN), static_cast<char> (Chars::newline) },
        { static_cast<char> (Chars::lowerT), static_cast<char> (Chars::tab) },
        { static_cast<char> (Chars::lowerB), static_cast<char> (Chars::backspace) }
    };

    const char decodedByte { configEscapes.contains (escaped) ? configEscapes.at (escaped) : escaped };
    jam::Format::appendByte (decoded, decodedByte);
}

static size_t appendQuotedConfigSegment (std::string& decoded, std::string_view raw, size_t index)
{
    ++index;

    while (index < raw.size() and raw.at (index) != static_cast<char> (Chars::doubleQuote))
    {
        const auto current { raw.at (index) };
        ++index;

        if (current == static_cast<char> (Chars::backslash) and index < raw.size())
        {
            appendConfigQuotedEscape (decoded, raw.at (index));
            ++index;
        }
        else
        {
            jam::Format::appendByte (decoded, current);
        }
    }

    if (index < raw.size())
        ++index;

    return index;
}

static std::string getConfigValue (std::string_view raw)
{
    std::string decoded;
    size_t index { 0 };

    while (index < raw.size() and jam::Document::isWhitespace (static_cast<unsigned char> (raw.at (index))))
        ++index;

    auto lastNonSpaceSize { decoded.size() };

    while (index < raw.size())
    {
        const auto current { raw.at (index) };

        if (current == static_cast<char> (Chars::hash) or current == static_cast<char> (Chars::semicolon))
            break;

        if (current == static_cast<char> (Chars::doubleQuote))
        {
            index = appendQuotedConfigSegment (decoded, raw, index);
            lastNonSpaceSize = decoded.size();
        }
        else
        {
            jam::Format::appendByte (decoded, current);
            ++index;

            if (not jam::Document::isWhitespace (static_cast<unsigned char> (current)))
                lastNonSpaceSize = decoded.size();
        }
    }

    decoded.resize (lastNonSpaceSize);
    return decoded;
}

static bool isSectionHeader (std::string_view trimmed) noexcept
{
    return not trimmed.empty() and trimmed.at (0) == static_cast<char> (Chars::openBracket);
}

static bool isNamedSectionHeader (std::string_view trimmed, std::string_view sectionName)
{
    const auto closePos { trimmed.find (static_cast<char> (Chars::closeBracket), 1) };

    if (closePos != std::string_view::npos)
    {
        const auto headerName { jam::Format::trim (trimmed.substr (1, closePos - 1)) };
        return jam::Strings::fromView (headerName).compareIgnoreCase (jam::Strings::fromView (sectionName)) == 0;
    }

    return false;
}

static std::optional<std::string> getEntryValueFromLine (std::string_view trimmed, std::string_view keyName)
{
    const auto equalsPos { trimmed.find (static_cast<char> (Chars::equals)) };

    if (equalsPos != std::string_view::npos)
    {
        const auto key { jam::Format::trim (trimmed.substr (0, equalsPos)) };

        if (jam::Strings::fromView (key).compareIgnoreCase (jam::Strings::fromView (keyName)) == 0)
            return getConfigValue (trimmed.substr (equalsPos + 1));
    }

    return std::nullopt;
}

static std::string getEntryValue (const TextDocument& document, std::string_view sectionName, std::string_view keyName)
{
    std::string foundValue;
    bool inSection { false };
    std::string logicalLine;
    const auto numLines { document.getNumLines() };

    for (int lineNumber { 1 }; lineNumber <= numLines; ++lineNumber)
    {
        const auto rawLine { document.getLine (lineNumber) };
        const auto isContinuation { getTrailingBackslashCount (rawLine, rawLine.size()) % 2 != 0 };

        if (isContinuation and lineNumber < numLines)
        {
            jam::Format::appendBytes (logicalLine, rawLine.substr (0, rawLine.size() - 1));
        }
        else
        {
            jam::Format::appendBytes (logicalLine, rawLine);
            const auto trimmed { jam::Format::trim (logicalLine) };

            if (isSectionHeader (trimmed))
                inSection = isNamedSectionHeader (trimmed, sectionName);
            else if (inSection)
                foundValue = getEntryValueFromLine (trimmed, keyName).value_or (foundValue);

            logicalLine.clear();
        }
    }

    return foundValue;
}

static juce::File getXdgFile (std::string_view xdgHome,
                              const juce::File& homeDir,
                              std::string_view xdgRelativePath,
                              std::string_view defaultRelativePath)
{
    if (not xdgHome.empty())
        return juce::File (jam::Strings::fromView (xdgHome)).getChildFile (jam::Strings::fromView (xdgRelativePath));

    return homeDir.getChildFile (jam::Strings::fromView (defaultRelativePath));
}

static std::optional<std::string_view> getTrimmedPattern (std::string_view rawLine) noexcept
{
    auto end { rawLine.size() };

    while (end > 0 and rawLine.at (end - 1) == static_cast<char> (Chars::space))
    {
        if (getTrailingBackslashCount (rawLine, end - 1) % 2 != 0)
            break;

        --end;
    }

    const auto trimmed { rawLine.substr (0, end) };

    if (trimmed.empty() or trimmed.at (0) == static_cast<char> (Chars::hash))
        return std::nullopt;

    if (getTrailingBackslashCount (trimmed, trimmed.size()) % 2 != 0)
        return std::nullopt;

    return trimmed;
}

static std::pair<bool, std::string_view> getNegationStripped (std::string_view pattern) noexcept
{
    const auto isNegated { pattern.at (0) == static_cast<char> (Chars::exclamation) };

    if (isNegated)
        pattern.remove_prefix (1);

    if (pattern.size() >= 2
        and pattern.at (0) == static_cast<char> (Chars::backslash)
        and (pattern.at (1) == static_cast<char> (Chars::hash)
             or pattern.at (1) == static_cast<char> (Chars::exclamation)))
    {
        pattern.remove_prefix (1);
    }

    return { isNegated, pattern };
}

static std::pair<bool, std::string_view> getDirectoryMarkerStripped (std::string_view pattern) noexcept
{
    if (not pattern.empty() and pattern.at (pattern.size() - 1) == static_cast<char> (Chars::slash))
        return { true, pattern.substr (0, pattern.size() - 1) };

    return { false, pattern };
}

static std::pair<bool, std::string_view> getAnchorStripped (std::string_view pattern) noexcept
{
    const auto startsWithSlash { not pattern.empty() and pattern.at (0) == static_cast<char> (Chars::slash) };

    if (startsWithSlash)
        pattern.remove_prefix (1);

    const auto isAnchored { startsWithSlash or pattern.find (static_cast<char> (Chars::slash)) != std::string_view::npos };
    return { isAnchored, pattern };
}

static bool isDoubleStarPrefix (std::string_view pattern, size_t index) noexcept
{
    return pattern.compare (index, doubleStarPrefix.size(), doubleStarPrefix) == 0;
}

static std::string_view getDoubleStarStripped (std::string_view pattern) noexcept
{
    while (isDoubleStarPrefix (pattern, 0))
        pattern.remove_prefix (doubleStarPrefix.size());

    return pattern;
}

static size_t appendClassToken (std::string& regex, std::string_view pattern, size_t index)
{
    static constexpr std::string_view classMetacharacters { "\\]-^[" };
    static constexpr std::string_view posixClassOpen { "[:" };
    const size_t closePosition { pattern.find (static_cast<char> (Chars::closeBracket), index + posixClassOpen.size()) };
    const bool isPosixClass { pattern.compare (index, posixClassOpen.size(), posixClassOpen) == 0
                              and closePosition != std::string_view::npos
                              and closePosition > index + posixClassOpen.size()
                              and pattern.at (closePosition - 1) == static_cast<char> (Chars::colon) };

    if (pattern.at (index) == static_cast<char> (Chars::backslash) and index + 1 < pattern.size())
    {
        const auto nextChar { pattern.at (index + 1) };
        index += 2;

        if (classMetacharacters.find (nextChar) != std::string_view::npos)
            jam::Format::appendUtf8 (regex, Chars::backslash);

        jam::Format::appendByte (regex, nextChar);
    }
    else if (isPosixClass)
    {
        jam::Format::appendBytes (regex, pattern.substr (index, closePosition + 1 - index));
        index = closePosition + 1;
    }
    else
    {
        jam::Format::appendByte (regex, pattern.at (index));
        ++index;
    }

    return index;
}

static size_t appendCharacterClass (std::string& regex, std::string_view pattern, size_t index)
{
    static constexpr std::string_view separatorLookahead { "(?!/)" };
    static constexpr std::string_view escapedCloseBracket { "\\]" };

    ++index;
    jam::Format::appendBytes (regex, separatorLookahead);
    jam::Format::appendUtf8 (regex, Chars::openBracket);

    if (index < pattern.size()
        and (pattern.at (index) == static_cast<char> (Chars::exclamation)
             or pattern.at (index) == static_cast<char> (Chars::caret)))
    {
        jam::Format::appendUtf8 (regex, Chars::caret);
        ++index;
    }

    if (index < pattern.size() and pattern.at (index) == static_cast<char> (Chars::closeBracket))
    {
        jam::Format::appendBytes (regex, escapedCloseBracket);
        ++index;
    }

    while (index < pattern.size() and pattern.at (index) != static_cast<char> (Chars::closeBracket))
        index = appendClassToken (regex, pattern, index);

    if (index < pattern.size() and pattern.at (index) == static_cast<char> (Chars::closeBracket))
    {
        jam::Format::appendUtf8 (regex, Chars::closeBracket);
        ++index;
    }

    return index;
}

static bool isDoubleStarToken (std::string_view pattern, size_t index) noexcept
{
    return (pattern.compare (index, innerDoubleStar.size(), innerDoubleStar) == 0)
           or (index + trailingDoubleStar.size() == pattern.size() and pattern.compare (index, trailingDoubleStar.size(), trailingDoubleStar) == 0);
}

static size_t appendDoubleStar (std::string& regex, std::string_view pattern, size_t index)
{
    static constexpr std::string_view innerDoubleStarRegex { "(?:/|/.*/)" };
    static constexpr std::string_view trailingDoubleStarRegex { "/.*" };

    assert (isDoubleStarToken (pattern, index));

    if (pattern.compare (index, innerDoubleStar.size(), innerDoubleStar) == 0)
    {
        jam::Format::appendBytes (regex, innerDoubleStarRegex);
        index += innerDoubleStar.size();
        index = pattern.size() - getDoubleStarStripped (pattern.substr (index)).size();
    }
    else
    {
        jam::Format::appendBytes (regex, trailingDoubleStarRegex);
        index += trailingDoubleStar.size();
    }

    return index;
}

static bool isSpecialGlobToken (std::string_view pattern, size_t index) noexcept
{
    const auto current { pattern.at (index) };
    return (current == static_cast<char> (Chars::backslash) and index + 1 < pattern.size())
           or current == static_cast<char> (Chars::openBracket)
           or current == static_cast<char> (Chars::asterisk);
}

static size_t appendSpecialGlob (std::string& regex, std::string_view pattern, size_t index)
{
    assert (isSpecialGlobToken (pattern, index));
    const auto current { pattern.at (index) };

    if (current == static_cast<char> (Chars::backslash))
    {
        jam::RegExp::appendEscaped (regex, pattern.at (index + 1));
        index += 2;
    }
    else if (current == static_cast<char> (Chars::openBracket))
    {
        index = appendCharacterClass (regex, pattern, index);
    }
    else
    {
        while (index < pattern.size() and pattern.at (index) == static_cast<char> (Chars::asterisk))
            ++index;

        jam::Format::appendBytes (regex, segmentRegex);
    }

    return index;
}

static size_t appendGlobStep (std::string& regex, std::string_view pattern, size_t index)
{
    if (isDoubleStarToken (pattern, index))
    {
        return appendDoubleStar (regex, pattern, index);
    }

    if (isSpecialGlobToken (pattern, index))
    {
        return appendSpecialGlob (regex, pattern, index);
    }

    const auto current { pattern.at (index) };

    if (current == static_cast<char> (Chars::question))
        jam::Format::appendBytes (regex, segmentCharacterRegex);
    else
        jam::RegExp::appendEscaped (regex, current);

    return index + 1;
}

static std::string getPrefixWithSlash (std::string_view basePrefix)
{
    if (not basePrefix.empty())
    {
        std::string result { basePrefix };

        if (result.back() != static_cast<char> (Chars::slash))
            jam::Format::appendUtf8 (result, Chars::slash);

        return result;
    }

    return {};
}

static bool isRuleMatch (const jam::RegExp& regExp, const jam::RegExpProgram* program, const jam::LiteralSet* literalSet, std::string_view candidate)
{
    if (literalSet != nullptr and literalSet->findNext (candidate, 0) >= candidate.size())
        return false;

    try
    {
        return regExp.hasMatch (candidate);
    }
    catch (const std::overflow_error&)
    {
        return program != nullptr and program->findNext (regExp, candidate, 0).size() > 0;
    }
}

IgnoreRules::IgnoreRules (Access, std::shared_ptr<const IgnoreRules> newParent, const juce::File& newTopLevel, std::string newBasePrefix)
    : topLevel (newTopLevel)
    , basePrefix (std::move (newBasePrefix))
    , parent (std::move (newParent))
{
}

std::shared_ptr<const IgnoreRules> IgnoreRules::create (const juce::File& walkRoot)
{
    const auto topLevel { getTopLevelDirectory (walkRoot) };
    auto chain { getLinkedRules (nullptr, getGlobalExcludesFile(), topLevel, {}) };
    const auto excludeFile { topLevel.getChildFile (Id::gitExcludePath.data()) };

    if (excludeFile.existsAsFile())
        chain = getLinkedRules (chain, excludeFile, topLevel, {});

    return getAncestorRules (chain, walkRoot);
}

std::shared_ptr<const IgnoreRules> IgnoreRules::getAncestorRules (const std::shared_ptr<const IgnoreRules>& parentRules,
                                                                  const juce::File& directory)
{
    const auto parentDirectory { directory.getParentDirectory() };
    auto chain { (directory == parentRules->topLevel or parentDirectory == directory)
                     ? parentRules
                     : getAncestorRules (parentRules, parentDirectory) };

    if (directory.getChildFile (Id::gitignoreFile.data()).existsAsFile())
        return getChildRules (chain, directory, chain->getRelativePath (directory));

    return chain;
}

std::shared_ptr<const IgnoreRules> IgnoreRules::getChildRules (const std::shared_ptr<const IgnoreRules>& parentRules,
                                                               const juce::File& directory,
                                                               std::string_view relativePrefix)
{
    return getLinkedRules (parentRules,
                           directory.getChildFile (Id::gitignoreFile.data()),
                           parentRules->topLevel,
                           getPrefixWithSlash (relativePrefix));
}

std::shared_ptr<const IgnoreRules> IgnoreRules::getLinkedRules (const std::shared_ptr<const IgnoreRules>& parentRules,
                                                                const juce::File& file,
                                                                const juce::File& newTopLevel,
                                                                std::string newBasePrefix)
{
    auto link { std::make_shared<IgnoreRules> (Access {}, parentRules, newTopLevel, std::move (newBasePrefix)) };
    link->addRulesFromFile (file);
    return link;
}

std::string IgnoreRules::getRootRelativePrefix (const juce::File& rootDirectory) const
{
    return getPrefixWithSlash (getRelativePath (rootDirectory));
}

bool IgnoreRules::isIgnored (std::string_view relativePath, bool isDirectory) const
{
    for (const IgnoreRules* link { this }; link != nullptr; link = link->parent.get())
    {
        const auto winningIndex { link->getLastMatch (relativePath, isDirectory) };

        if (winningIndex >= 0)
            return not link->rules.at (winningIndex).isNegated;
    }

    return false;
}

int IgnoreRules::getLastMatch (std::string_view relativePath, bool isDirectory) const
{
    if (relativePath.size() > basePrefix.size() and relativePath.compare (0, basePrefix.size(), basePrefix) == 0)
    {
        const std::string_view candidate { relativePath.substr (basePrefix.size()) };
        auto componentStart { candidate.rfind (static_cast<char> (Chars::slash)) + 1 };
        const auto lastComponent { candidate.substr (componentStart) };
        const auto dotPosition { lastComponent.rfind (static_cast<char> (Chars::dot)) };
        auto winningIndex { std::max (getLastIndex (suffixes, lastComponent, isDirectory),
                                      getLastIndex (relativePaths, candidate, isDirectory)) };

        while (componentStart > 0)
        {
            componentStart = candidate.substr (0, componentStart - 1).rfind (static_cast<char> (Chars::slash)) + 1;
            winningIndex = std::max (winningIndex, getLastIndex (suffixes, candidate.substr (componentStart), isDirectory));
        }

        if (dotPosition != std::string_view::npos)
            winningIndex = std::max (winningIndex, getLastIndex (extensions, lastComponent.substr (dotPosition + 1), isDirectory));

        for (auto position { others.size() }; position > 0 and others.at (position - 1) > winningIndex; --position)
        {
            const auto index { others.at (position - 1) };
            const auto& rule { rules.at (index) };

            if ((not rule.isDirectoryOnly or isDirectory) and isRuleMatch (*rule.regExp, rule.program.get(), rule.literalSet.get(), candidate))
            {
                winningIndex = index;
                break;
            }
        }

        return winningIndex;
    }

    return -1;
}

int IgnoreRules::getLastIndex (const RuleIndexes& indexes, std::string_view key, bool isDirectory) const
{
    const auto entry { indexes.find (key) };

    if (entry != indexes.end())
    {
        const auto& [entryKey, candidates] { *entry };
        juce::ignoreUnused (entryKey);

        for (auto position { candidates.size() }; position > 0; --position)
        {
            const auto index { candidates.at (position - 1) };

            if (not rules.at (index).isDirectoryOnly or isDirectory)
                return index;
        }
    }

    return -1;
}

juce::File IgnoreRules::getTopLevelDirectory (const juce::File& walkRoot)
{
    auto directory { walkRoot };

    while (not directory.getChildFile (Id::gitDirectory.data()).exists())
    {
        const auto parent { directory.getParentDirectory() };

        if (parent == directory)
            return walkRoot;

        directory = parent;
    }

    return directory;
}

juce::File IgnoreRules::getGlobalExcludesFile()
{
    const juce::File homeDir { juce::File::getSpecialLocation (juce::File::userHomeDirectory) };
    const std::string xdgHome { jam::Strings::getView (juce::SystemStats::getEnvironmentVariable (jam::Strings::fromView (Id::xdgConfigHomeVariable), {})) };
    auto configured { getConfigEntry (getXdgFile (xdgHome,
                                                  homeDir,
                                                  Id::gitConfigRelativePath,
                                                  Id::defaultGitConfigRelativePath),
                                      Id::coreSection,
                                      Id::excludesFileKey) };
    const auto userConfigured { getConfigEntry (homeDir.getChildFile (Id::userGitConfigFile.data()), Id::coreSection, Id::excludesFileKey) };

    if (not userConfigured.empty())
        configured = userConfigured;

    if (configured.compare (0, Id::homePrefix.size(), Id::homePrefix) == 0)
        return homeDir.getChildFile (jam::Strings::fromView (std::string_view { configured }.substr (Id::homePrefix.size())));

    if (not configured.empty())
        return juce::File::getCurrentWorkingDirectory().getChildFile (jam::Strings::fromView (configured));

    return getXdgFile (xdgHome,
                       homeDir,
                       Id::gitIgnoreRelativePath,
                       Id::defaultGitIgnoreRelativePath);
}

std::string IgnoreRules::getConfigEntry (const juce::File& configFile, std::string_view sectionName, std::string_view keyName)
{
    std::string foundValue;

    if (configFile.existsAsFile())
    {
        const auto [result, document] { TextDocument::fromFile (configFile, jam::Strings::getView (configFile.getFullPathName()), Input::maxBufferedReadSize, false) };

        if (result.wasOk() and not document->isBinary())
            foundValue = getEntryValue (*document, sectionName, keyName);
    }

    return foundValue;
}

void IgnoreRules::addRule (std::string_view rawLine)
{
    static constexpr std::string_view globMetacharacters { "*?[\\" };
    static constexpr std::string_view extensionPrefix { "*." };
    const auto trimmed { getTrimmedPattern (rawLine) };

    if (trimmed.has_value())
    {
        const auto [isNegated, unescapedPattern] { getNegationStripped (*trimmed) };
        const auto [isDirectoryOnly, patternBody] { getDirectoryMarkerStripped (unescapedPattern) };
        const auto [isAnchored, finalPattern] { getAnchorStripped (patternBody) };

        if (not finalPattern.empty())
        {
            const auto ruleIndex { rules.size() };
            const auto floatingPattern { getDoubleStarStripped (finalPattern) };
            const auto isFloating { not isAnchored or floatingPattern.size() < finalPattern.size() };
            const auto isLiteral { not floatingPattern.empty() and floatingPattern.find_first_of (globMetacharacters) == std::string_view::npos };
            const auto hasExtensionPrefix { floatingPattern.compare (0, extensionPrefix.size(), extensionPrefix) == 0 };
            const auto extensionText { hasExtensionPrefix ? floatingPattern.substr (extensionPrefix.size()) : std::string_view {} };
            const auto isExtension { hasExtensionPrefix and isFloating
                                     and extensionText.find_first_of (globMetacharacters) == std::string_view::npos
                                     and extensionText.find (static_cast<char> (Chars::dot)) == std::string_view::npos
                                     and extensionText.find (static_cast<char> (Chars::slash)) == std::string_view::npos };

            if (isLiteral or isExtension)
            {
                auto& indexes { isExtension ? extensions : (isFloating ? suffixes : relativePaths) };
                const auto key { isExtension ? extensionText : floatingPattern };
                const auto [indexEntry, wasInserted] { indexes.try_emplace (std::string { key }) };
                auto& [indexKey, ruleIndexes] { *indexEntry };
                juce::ignoreUnused (indexKey, wasInserted);
                ruleIndexes.add (ruleIndex);
                rules.add (Rule { nullptr, nullptr, nullptr, isNegated, isDirectoryOnly });
            }
            else
            {
                try
                {
                    const auto regexText { getTranslatedGlobRegex (finalPattern, isAnchored) };
                    auto compiled { std::make_unique<jam::RegExp> (regexText) };
                    auto program { jam::RegExpProgram::create (*compiled) };
                    auto literals (jam::Utf8::isValid (regexText) ? compiled->getRequiredLiterals (false) : std::vector<std::string> {});
                    auto literalSet { literals.empty() ? nullptr : jam::LiteralSet::create (std::move (literals), false) };
                    others.add (ruleIndex);
                    rules.add (Rule { std::move (compiled), std::move (program), std::move (literalSet), isNegated, isDirectoryOnly });
                }
                catch (const std::invalid_argument&)
                {
                }
            }
        }
    }
}

std::string IgnoreRules::getTranslatedGlobRegex (std::string_view pattern, bool isAnchored)
{
    static constexpr std::string_view anchoredRegexPrefix { "^" };
    static constexpr std::string_view floatingRegexPrefix { "(?:^|/)" };
    static constexpr std::string_view leadingDoubleStarRegex { "(?:.*/)?" };

    std::string regex { isAnchored ? anchoredRegexPrefix : floatingRegexPrefix };
    size_t index { 0 };

    if (isDoubleStarPrefix (pattern, 0))
    {
        jam::Format::appendBytes (regex, leadingDoubleStarRegex);

        index = pattern.size() - getDoubleStarStripped (pattern).size();
    }

    while (index < pattern.size())
        index = appendGlobStep (regex, pattern, index);

    jam::Format::appendUtf8 (regex, Chars::dollarSign);
    return regex;
}

void IgnoreRules::addRulesFromFile (const juce::File& file)
{
    if (file.existsAsFile())
    {
        const auto [result, document] { TextDocument::fromFile (file, jam::Strings::getView (file.getFullPathName()), Input::maxBufferedReadSize, false) };

        if (result.wasOk() and not document->isBinary())
        {
            const auto numLines { document->getNumLines() };

            for (int lineNumber { 1 }; lineNumber <= numLines; ++lineNumber)
                addRule (document->getLine (lineNumber));
        }
    }
}

std::string IgnoreRules::getRelativePath (const juce::File& entry) const
{
    if (entry == topLevel)
        return {};

    return Paths::getDisplayPath (jam::Strings::getView (entry.getRelativePathFrom (topLevel)));
}
