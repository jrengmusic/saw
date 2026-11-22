#include "IgnoreRules.h"
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

static void readQuotedConfigSegment (std::string_view raw, size_t& index, std::string& decoded)
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
            readQuotedConfigSegment (raw, index, decoded);
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
                   .compareIgnoreCase (Id::coreSection) == 0;
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

        if (keyText.compareIgnoreCase (Id::excludesFileKey) == 0)
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

static void appendClassToken (std::string& regex, std::string_view pattern, size_t& index)
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
}

static void appendCharacterClass (std::string& regex, std::string_view pattern, size_t& index)
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
        appendClassToken (regex, pattern, index);

    if (index < pattern.size() and pattern.at (index) == static_cast<char> (Chars::closeBracket))
    {
        regex.push_back (static_cast<char> (Chars::closeBracket));
        ++index;
    }
}

static bool isDoubleStarToken (std::string_view pattern, size_t index) noexcept
{
    return (index + 4 <= pattern.size() and pattern.substr (index, 4).compare ("/**/") == 0)
           or (index + 3 == pattern.size() and pattern.substr (index, 3).compare ("/**") == 0);
}

static void appendDoubleStar (std::string& regex, std::string_view pattern, size_t& index)
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
}

static bool isSpecialGlobToken (std::string_view pattern, size_t index) noexcept
{
    const auto current { pattern.at (index) };
    return (current == static_cast<char> (Chars::backslash) and index + 1 < pattern.size())
           or current == static_cast<char> (Chars::openBracket)
           or current == static_cast<char> (Chars::asterisk);
}

static void appendSpecialGlob (std::string& regex, std::string_view pattern, size_t& index)
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
        appendCharacterClass (regex, pattern, index);
    }
    else
    {
        while (index < pattern.size() and pattern.at (index) == static_cast<char> (Chars::asterisk))
            ++index;

        regex.append ("[^/]*");
    }
}

static void appendGlobStep (std::string& regex, std::string_view pattern, size_t& index)
{
    static const jam::HashMap<char, std::string_view> globTokens
    {
        { '.', "\\." }, { '+', "\\+" }, { '(', "\\(" }, { ')', "\\)" },
        { '{', "\\{" }, { '}', "\\}" }, { '^', "\\^" }, { '$', "\\$" },
        { '|', "\\|" }, { ']', "\\]" }, { '?', "[^/]" }, { '*', "[^/]*" }
    };

    if (isDoubleStarToken (pattern, index))
    {
        appendDoubleStar (regex, pattern, index);
    }
    else if (isSpecialGlobToken (pattern, index))
    {
        appendSpecialGlob (regex, pattern, index);
    }
    else
    {
        const auto current { pattern.at (index) };

        if (globTokens.contains (current))
            regex.append (globTokens.at (current));
        else
            regex.push_back (current);

        ++index;
    }
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

IgnoreRules::IgnoreRules (const juce::File& walkRoot)
    : topLevel (getTopLevelDirectory (walkRoot))
{
    addRulesFromFile (getGlobalExcludesFile(), {});
    addRulesFromFile (topLevel.getChildFile (Id::gitExcludePath), {});

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
        addRulesFromFile (directory.getChildFile (Id::gitignoreFile), getRelativePath (directory));
    }
}

IgnoreRules::IgnoreRules (const juce::File& newTopLevel, jam::Array<Rule> newRules)
    : topLevel (newTopLevel)
    , rules (std::move (newRules))
{
}

IgnoreRules IgnoreRules::getChildRules (const juce::File& directory, std::string_view relativePrefix) const
{
    jam::Array<Rule> childRules;
    childRules.ensureStorageAllocated (rules.size());

    for (const auto& rule : rules)
        childRules.add (rule);

    IgnoreRules child { topLevel, std::move (childRules) };
    child.addRulesFromFile (directory.getChildFile (Id::gitignoreFile), relativePrefix);
    return child;
}

std::string IgnoreRules::getRootRelativePrefix (const juce::File& rootDirectory) const
{
    return getPrefixWithSlash (getRelativePath (rootDirectory));
}

bool IgnoreRules::isIgnored (std::string_view relativePath, bool isDirectory) const
{
    if (not relativePath.empty())
    {
        for (int index { rules.size() - 1 }; index >= 0; --index)
        {
            const auto& rule { rules.at (index) };
            const auto isMatchingBase { relativePath.size() > rule.basePrefix.size()
                                        and relativePath.compare (0, rule.basePrefix.size(), rule.basePrefix) == 0 };
            const auto isTypeMatch { not rule.isDirectoryOnly or isDirectory };

            if (isMatchingBase and isTypeMatch)
            {
                const std::string_view candidate { relativePath.substr (rule.basePrefix.size()) };

                if (rule.regExp->hasMatch (candidate))
                    return not rule.isNegated;
            }
        }
    }

    return false;
}

juce::File IgnoreRules::getTopLevelDirectory (const juce::File& walkRoot)
{
    auto directory { walkRoot };

    while (not directory.getChildFile (Id::gitDirectory).exists())
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
    const auto xdgHome { juce::SystemStats::getEnvironmentVariable (Id::xdgConfigHomeVariable, {}) };
    auto configured { getExcludesFileFromConfig (getXdgFile (xdgHome,
                                                             homeDir,
                                                             Id::gitConfigRelativePath,
                                                             Id::defaultGitConfigRelativePath)) };
    const auto userConfigured { getExcludesFileFromConfig (homeDir.getChildFile (Id::userGitConfigFile)) };

    if (userConfigured.isNotEmpty())
        configured = userConfigured;

    if (configured.startsWith (Id::homePrefix))
        return homeDir.getChildFile (configured.substring (Id::homePrefix.length()));

    if (configured.isNotEmpty())
        return juce::File::getCurrentWorkingDirectory().getChildFile (configured);

    return getXdgFile (xdgHome, homeDir, Id::gitIgnoreRelativePath, Id::defaultGitIgnoreRelativePath);
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

std::optional<IgnoreRules::Rule> IgnoreRules::getParsedRule (std::string_view rawLine,
                                                             const std::string& basePrefixWithSlash)
{
    const auto trimmed { getTrimmedPattern (rawLine) };

    if (trimmed.has_value())
    {
        const auto [isNegated, unescapedPattern] { getNegationStripped (*trimmed) };
        const auto [isDirectoryOnly, patternBody] { getDirectoryMarkerStripped (unescapedPattern) };
        const auto [isAnchored, finalPattern] { getAnchorStripped (patternBody) };

        if (not finalPattern.empty())
        {
            try
            {
                auto compiled { std::make_shared<jam::RegExp> (getTranslatedGlobRegex (finalPattern, isAnchored)) };
                return Rule { std::move (compiled), basePrefixWithSlash, isNegated, isDirectoryOnly };
            }
            catch (const std::invalid_argument&)
            {
                return std::nullopt;
            }
        }
    }

    return std::nullopt;
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
        appendGlobStep (regex, pattern, index);

    regex.push_back (static_cast<char> (Chars::dollarSign));
    return regex;
}

void IgnoreRules::addRulesFromFile (const juce::File& file, std::string_view basePrefix)
{
    if (file.existsAsFile())
    {
        const auto [result, document] { TextDocument::fromFile (file, file.getFullPathName()) };

        if (result.wasOk() and not document->isBinary())
        {
            const auto basePrefixWithSlash { getPrefixWithSlash (basePrefix) };
            const auto numLines { document->getNumLines() };

            for (int lineNumber { 1 }; lineNumber <= numLines; ++lineNumber)
            {
                auto rule { getParsedRule (document->getLine (lineNumber), basePrefixWithSlash) };

                if (rule.has_value())
                    rules.add (std::move (*rule));
            }
        }
    }
}

std::string IgnoreRules::getRelativePath (const juce::File& entry) const
{
    if (entry == topLevel)
        return {};

    return entry.getRelativePathFrom (topLevel).replaceCharacter (Chars::backslash, Chars::slash).toStdString();
}
