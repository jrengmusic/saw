#include "IgnoreRules.h"
#include "Paths.h"
#include "TextDocument.h"
#include "generated/Generated.h"

static bool isAsciiSpaceOrTab (char character) noexcept
{
    return character == static_cast<char> (Chars::space)
           or character == static_cast<char> (Chars::tab);
}

static std::string_view getTrimmedAscii (std::string_view text) noexcept
{
    size_t start { 0 };
    auto end { text.size() };

    while (start < end and isAsciiSpaceOrTab (text.at (start)))
        ++start;

    while (end > start and isAsciiSpaceOrTab (text.at (end - 1)))
        --end;

    return text.substr (start, end - start);
}

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

    decoded.push_back (configEscapes.contains (escaped) ? configEscapes.at (escaped) : escaped);
}

static size_t readQuotedConfigSegment (std::string_view raw, size_t index, std::string& decoded)
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
            decoded.push_back (current);
        }
    }

    if (index < raw.size())
        ++index;

    return index;
}

static std::string parseConfigValue (std::string_view raw)
{
    std::string decoded;
    size_t index { 0 };

    while (index < raw.size() and isAsciiSpaceOrTab (raw.at (index)))
        ++index;

    auto lastNonSpaceSize { decoded.size() };

    while (index < raw.size())
    {
        const auto current { raw.at (index) };

        if (current == static_cast<char> (Chars::hash) or current == static_cast<char> (Chars::semicolon))
            break;

        if (current == static_cast<char> (Chars::doubleQuote))
        {
            index = readQuotedConfigSegment (raw, index, decoded);
            lastNonSpaceSize = decoded.size();
        }
        else
        {
            decoded.push_back (current);
            ++index;

            if (not isAsciiSpaceOrTab (current))
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

static bool isCoreSectionHeader (std::string_view trimmed)
{
    const auto closePos { trimmed.find (static_cast<char> (Chars::closeBracket), 1) };

    if (closePos != std::string_view::npos)
    {
        const auto sectionName { getTrimmedAscii (trimmed.substr (1, closePos - 1)) };
        return juce::String::fromUTF8 (sectionName.data(), static_cast<int> (sectionName.size()))
                   .compareIgnoreCase (Id::coreSection.data()) == 0;
    }

    return false;
}

static std::optional<juce::String> getExcludesFileValueFromLine (std::string_view trimmed)
{
    const auto equalsPos { trimmed.find (static_cast<char> (Chars::equals)) };

    if (equalsPos != std::string_view::npos)
    {
        const auto key { getTrimmedAscii (trimmed.substr (0, equalsPos)) };
        const auto keyText { juce::String::fromUTF8 (key.data(), static_cast<int> (key.size())) };

        if (keyText.compareIgnoreCase (Id::excludesFileKey.data()) == 0)
        {
            const auto value { parseConfigValue (trimmed.substr (equalsPos + 1)) };
            return juce::String::fromUTF8 (value.data(), static_cast<int> (value.size()));
        }
    }

    return std::nullopt;
}

static juce::String parseExcludesFile (const TextDocument& document)
{
    juce::String foundValue;
    bool inCoreSection { false };
    std::string logicalLine;
    const auto numLines { document.getNumLines() };

    for (int lineNumber { 1 }; lineNumber <= numLines; ++lineNumber)
    {
        const auto rawLine { document.getLine (lineNumber) };
        const auto isContinuation { getTrailingBackslashCount (rawLine, rawLine.size()) % 2 != 0 };

        if (isContinuation and lineNumber < numLines)
        {
            logicalLine.append (rawLine.substr (0, rawLine.size() - 1));
        }
        else
        {
            logicalLine.append (rawLine);
            const auto trimmed { getTrimmedAscii (logicalLine) };
            logicalLine.clear();

            if (isSectionHeader (trimmed))
                inCoreSection = isCoreSectionHeader (trimmed);
            else if (inCoreSection)
                foundValue = getExcludesFileValueFromLine (trimmed).value_or (foundValue);
        }
    }

    return foundValue;
}

static juce::File getXdgFile (const juce::String& xdgHome,
                              const juce::File& homeDir,
                              const juce::String& xdgRelativePath,
                              const juce::String& defaultRelativePath)
{
    if (xdgHome.isNotEmpty())
        return juce::File (xdgHome).getChildFile (xdgRelativePath);

    return homeDir.getChildFile (defaultRelativePath);
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
    return index + 3 <= pattern.size() and pattern.substr (index, 3).compare ("**/") == 0;
}

static void appendEscapedRegexLiteral (std::string& regex, char character)
{
    static constexpr std::string_view regexMetacharacters { "^$\\.*+?()[]{}|/" };

    if (regexMetacharacters.find (character) != std::string_view::npos)
        regex.push_back (static_cast<char> (Chars::backslash));

    regex.push_back (character);
}

static size_t appendClassToken (std::string& regex, std::string_view pattern, size_t index)
{
    static constexpr std::string_view classMetacharacters { "\\]-^[" };

    if (pattern.at (index) == static_cast<char> (Chars::backslash) and index + 1 < pattern.size())
    {
        const auto nextChar { pattern.at (index + 1) };
        index += 2;

        if (classMetacharacters.find (nextChar) != std::string_view::npos)
            regex.push_back (static_cast<char> (Chars::backslash));

        regex.push_back (nextChar);
    }
    else
    {
        regex.push_back (pattern.at (index));
        ++index;
    }

    return index;
}

static size_t appendCharacterClass (std::string& regex, std::string_view pattern, size_t index)
{
    ++index;
    regex.append ("(?!/)");
    regex.push_back (static_cast<char> (Chars::openBracket));

    if (index < pattern.size()
        and (pattern.at (index) == static_cast<char> (Chars::exclamation)
             or pattern.at (index) == static_cast<char> (Chars::caret)))
    {
        regex.push_back (static_cast<char> (Chars::caret));
        ++index;
    }

    if (index < pattern.size() and pattern.at (index) == static_cast<char> (Chars::closeBracket))
    {
        regex.append ("\\]");
        ++index;
    }

    while (index < pattern.size() and pattern.at (index) != static_cast<char> (Chars::closeBracket))
        index = appendClassToken (regex, pattern, index);

    if (index < pattern.size() and pattern.at (index) == static_cast<char> (Chars::closeBracket))
    {
        regex.push_back (static_cast<char> (Chars::closeBracket));
        ++index;
    }

    return index;
}

static bool isDoubleStarToken (std::string_view pattern, size_t index) noexcept
{
    return (index + 4 <= pattern.size() and pattern.substr (index, 4).compare ("/**/") == 0)
           or (index + 3 == pattern.size() and pattern.substr (index, 3).compare ("/**") == 0);
}

static size_t appendDoubleStar (std::string& regex, std::string_view pattern, size_t index)
{
    assert (isDoubleStarToken (pattern, index));

    if (index + 4 <= pattern.size() and pattern.substr (index, 4).compare ("/**/") == 0)
    {
        regex.append ("(?:/|/.*/)");
        index += 4;

        while (isDoubleStarPrefix (pattern, index))
            index += 3;
    }
    else
    {
        regex.append ("/.*");
        index += 3;
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
        appendEscapedRegexLiteral (regex, pattern.at (index + 1));
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

        regex.append ("[^/]*");
    }

    return index;
}

static size_t appendGlobStep (std::string& regex, std::string_view pattern, size_t index)
{
    static const jam::HashMap<char, std::string_view> globTokens
    {
        { '.', "\\." }, { '+', "\\+" }, { '(', "\\(" }, { ')', "\\)" },
        { '{', "\\{" }, { '}', "\\}" }, { '^', "\\^" }, { '$', "\\$" },
        { '|', "\\|" }, { ']', "\\]" }, { '?', "[^/]" }, { '*', "[^/]*" }
    };

    if (isDoubleStarToken (pattern, index))
    {
        return appendDoubleStar (regex, pattern, index);
    }

    if (isSpecialGlobToken (pattern, index))
    {
        return appendSpecialGlob (regex, pattern, index);
    }

    const auto current { pattern.at (index) };

    if (globTokens.contains (current))
        regex.append (globTokens.at (current));
    else
        regex.push_back (current);

    return index + 1;
}

static std::string getPrefixWithSlash (std::string_view basePrefix)
{
    if (not basePrefix.empty())
    {
        std::string result { basePrefix };

        if (result.back() != static_cast<char> (Chars::slash))
            result.push_back (static_cast<char> (Chars::slash));

        return result;
    }

    return {};
}

static bool isRuleMatch (const jam::RegExp& regExp, const jam::RegExpProgram* program, std::string_view candidate)
{
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

    jam::Array<juce::File> ancestors;
    auto currentDirectory { walkRoot };

    for (;;)
    {
        ancestors.add (currentDirectory);
        const auto parentDirectory { currentDirectory.getParentDirectory() };

        if (currentDirectory == topLevel or parentDirectory == currentDirectory)
            break;

        currentDirectory = parentDirectory;
    }

    for (int index { ancestors.size() - 1 }; index >= 0; --index)
    {
        const auto& directory { ancestors.at (index) };

        if (directory.getChildFile (Id::gitignoreFile.data()).existsAsFile())
            chain = getChildRules (chain, directory, chain->getRelativePath (directory));
    }

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
        const auto lastComponent { candidate.substr (candidate.rfind (static_cast<char> (Chars::slash)) + 1) };
        const auto dotPosition { lastComponent.rfind (static_cast<char> (Chars::dot)) };
        auto winningIndex { std::max (getLastIndex (basenames, lastComponent, isDirectory),
                                      getLastIndex (relativePaths, candidate, isDirectory)) };

        if (dotPosition != std::string_view::npos)
            winningIndex = std::max (winningIndex, getLastIndex (extensions, lastComponent.substr (dotPosition + 1), isDirectory));

        for (auto position { others.size() }; position > 0 and others.at (position - 1) > winningIndex; --position)
        {
            const auto index { others.at (position - 1) };
            const auto& rule { rules.at (index) };

            if ((not rule.isDirectoryOnly or isDirectory) and isRuleMatch (*rule.regExp, rule.program.get(), candidate))
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
    const auto xdgHome { juce::SystemStats::getEnvironmentVariable (jam::Strings::fromView (Id::xdgConfigHomeVariable), {}) };
    auto configured { getExcludesFileFromConfig (getXdgFile (xdgHome,
                                                             homeDir,
                                                             jam::Strings::fromView (Id::gitConfigRelativePath),
                                                             jam::Strings::fromView (Id::defaultGitConfigRelativePath))) };
    const auto userConfigured { getExcludesFileFromConfig (homeDir.getChildFile (Id::userGitConfigFile.data())) };

    if (userConfigured.isNotEmpty())
        configured = userConfigured;

    if (configured.startsWith (Id::homePrefix.data()))
        return homeDir.getChildFile (configured.substring (static_cast<int> (Id::homePrefix.length())));

    if (configured.isNotEmpty())
        return juce::File::getCurrentWorkingDirectory().getChildFile (configured);

    return getXdgFile (xdgHome,
                       homeDir,
                       jam::Strings::fromView (Id::gitIgnoreRelativePath),
                       jam::Strings::fromView (Id::defaultGitIgnoreRelativePath));
}

juce::String IgnoreRules::getExcludesFileFromConfig (const juce::File& configFile)
{
    juce::String foundValue;

    if (configFile.existsAsFile())
    {
        const auto [result, document] { TextDocument::fromFile (configFile, configFile.getFullPathName()) };

        if (result.wasOk() and not document->isBinary())
            foundValue = parseExcludesFile (*document);
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
            const auto isLiteral { finalPattern.find_first_of (globMetacharacters) == std::string_view::npos };
            const auto hasExtensionPrefix { finalPattern.compare (0, extensionPrefix.size(), extensionPrefix) == 0 };
            const auto extensionText { hasExtensionPrefix ? finalPattern.substr (extensionPrefix.size()) : std::string_view {} };
            const auto isExtension { hasExtensionPrefix and not isAnchored
                                     and extensionText.find_first_of (globMetacharacters) == std::string_view::npos
                                     and extensionText.find (static_cast<char> (Chars::dot)) == std::string_view::npos };

            if (isLiteral or isExtension)
            {
                auto& indexes { isExtension ? extensions : (isAnchored ? relativePaths : basenames) };
                const auto key { isExtension ? extensionText : finalPattern };
                const auto [indexEntry, wasInserted] { indexes.try_emplace (std::string { key }) };
                auto& [indexKey, ruleIndexes] { *indexEntry };
                juce::ignoreUnused (indexKey, wasInserted);
                ruleIndexes.push_back (ruleIndex);
                rules.add (Rule { nullptr, nullptr, isNegated, isDirectoryOnly });
            }
            else
            {
                try
                {
                    auto compiled { std::make_shared<jam::RegExp> (getTranslatedGlobRegex (finalPattern, isAnchored)) };
                    auto program { jam::RegExpProgram::create (*compiled) };
                    others.push_back (ruleIndex);
                    rules.add (Rule { std::move (compiled), std::move (program), isNegated, isDirectoryOnly });
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
    std::string regex { isAnchored ? "^" : "(?:^|/)" };
    size_t index { 0 };

    if (isDoubleStarPrefix (pattern, 0))
    {
        regex.append ("(?:.*/)?");

        while (isDoubleStarPrefix (pattern, index))
            index += 3;
    }

    while (index < pattern.size())
        index = appendGlobStep (regex, pattern, index);

    regex.push_back (static_cast<char> (Chars::dollarSign));
    return regex;
}

void IgnoreRules::addRulesFromFile (const juce::File& file)
{
    if (file.existsAsFile())
    {
        const auto [result, document] { TextDocument::fromFile (file, file.getFullPathName()) };

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
