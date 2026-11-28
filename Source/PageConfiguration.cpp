#include "PageConfiguration.h"
#include "PageOptions.h"
#include "Input.h"
#include "Paths.h"
#include "TextDocument.h"
#include "generated/Generated.h"

static constexpr size_t quotePairLength { 2 };
static constexpr size_t singleLength { 1 };
static constexpr int firstLine { 1 };

static juce::String getEnvironment (std::string_view name)
{
    return juce::SystemStats::getEnvironmentVariable (jam::Strings::fromView (name), {});
}

static juce::File getHomeConfigDirectory()
{
    return juce::File::getSpecialLocation (juce::File::userHomeDirectory)
        .getChildFile (jam::Strings::fromView (Id::homeConfigFolderName))
        .getChildFile (jam::Strings::fromView (Id::batFolderName));
}

static juce::File getConfigDirectoryFromEnvironment()
{
    const auto directoryText { getEnvironment (Id::batConfigDirectoryVariable) };
    const auto xdgConfigHome { getEnvironment (Id::xdgConfigHomeVariable) };

    if (directoryText.isNotEmpty())
        return juce::File { directoryText };

    if (xdgConfigHome.isNotEmpty())
        return juce::File { xdgConfigHome }.getChildFile (jam::Strings::fromView (Id::batFolderName));

    return getHomeConfigDirectory();
}

static juce::File getConfigFileFromEnvironment (const juce::File& configDirectory)
{
    if (std::getenv (Id::batConfigPathVariable.data()) != nullptr)
        return juce::File { getEnvironment (Id::batConfigPathVariable) };

    return configDirectory.getChildFile (Id::config.toString());
}

static bool isQuotedValue (std::string_view value)
{
    static constexpr char doubleQuote { '"' };
    static constexpr char singleQuote { '\'' };
    return value.size() >= quotePairLength and (value.front() == doubleQuote or value.front() == singleQuote) and value.back() == value.front();
}

static std::string_view getOptionValue (std::string_view optionText)
{
    static constexpr char optionValueSign { '=' };
    static constexpr size_t noNameLength { 0 };
    const auto separator { jam::simd::findByte (optionText.data(), optionText.size(), 0, optionValueSign) };
    const size_t valueStart { separator < optionText.size() ? separator + singleLength : noNameLength };
    return optionText.substr (valueStart);
}

static std::string getUnquoted (std::string_view optionText)
{
    const auto value { getOptionValue (optionText) };
    const size_t valueStart { optionText.size() - value.size() };
    jassert (isQuotedValue (value));
    std::string unquoted;
    jam::Format::appendBytes (unquoted, optionText.substr (0, valueStart));
    jam::Format::appendBytes (unquoted, value.substr (singleLength, value.size() - quotePairLength));
    return unquoted;
}

static std::deque<std::string> getOptionTextsOf (const jam::Strings& strings)
{
    std::deque<std::string> optionTexts;

    for (int index { 0 }; index < strings.size(); ++index)
    {
        const auto optionText { strings.getView (index) };
        optionTexts.push_back (isQuotedValue (getOptionValue (optionText)) ? getUnquoted (optionText) : std::string { optionText });
    }

    return optionTexts;
}

template <typename Function>
static void applyToFileOptions (std::string_view configText, const Function& function)
{
    static constexpr char commentChar { '#' };

    TextDocument::applyToLines (configText, juce::Range<int> { firstLine, Arguments::openRangeEnd }, [&function] (int, std::string_view line)
    {
        const auto trimmed { jam::Format::trim (line) };

        if (not trimmed.empty() and trimmed.front() != commentChar)
            function (trimmed);

        return true;
    });
}

static std::deque<std::string> getFileOptionTexts (std::string_view configText)
{
    std::deque<std::string> optionTexts;

    applyToFileOptions (configText, [&optionTexts] (std::string_view trimmed)
    {
        if (isQuotedValue (getOptionValue (trimmed)))
            optionTexts.push_back (getUnquoted (trimmed));
    });

    return optionTexts;
}

static std::string getOptionPair (std::string_view optionName, const juce::String& valueText)
{
    std::string optionText;
    jam::Format::appendBytes (optionText, optionName);
    jam::Format::appendByte (optionText, static_cast<char> (Chars::equals));
    jam::Format::appendBytes (optionText, jam::Strings::getView (valueText));
    return optionText;
}

static std::deque<std::string> getEnvironmentVariableTexts()
{
    static constexpr std::array<std::pair<std::string_view, std::string_view>, 7> environmentOptions {{
        { Id::batThemeVariable, Id::themeOption },
        { Id::batThemeLightVariable, Id::themeLightOption },
        { Id::batThemeDarkVariable, Id::themeDarkOption },
        { Id::batStyleVariable, Id::styleOption },
        { Id::batPagerVariable, Id::pagerOption },
        { Id::batPagingVariable, Id::pagingOption },
        { Id::batWidthVariable, Id::terminalWidthOption }
    }};
    std::deque<std::string> optionTexts;

    for (const auto& [variableName, optionName] : environmentOptions)
    {
        const auto valueText { getEnvironment (variableName) };

        if (valueText.isNotEmpty())
            optionTexts.push_back (getOptionPair (optionName, valueText));
    }

    return optionTexts;
}

static std::deque<std::string> getBatOptionTexts()
{
    const auto words (jam::Strings::fromTokens (getEnvironment (Id::batOptionsVariable), Id::wordBreakCharacters.data(), Chars::quotes));
    return getOptionTextsOf (words);
}

static std::deque<std::string> getEnvironmentOptionTexts()
{
    auto optionTexts (getEnvironmentVariableTexts());

    for (auto& optionText : getBatOptionTexts())
        optionTexts.push_back (std::move (optionText));

    return optionTexts;
}

static std::deque<std::string> getOptionTexts (std::string_view configText)
{
    auto optionTexts (getFileOptionTexts (configText));

    for (auto& optionText : getEnvironmentOptionTexts())
        optionTexts.push_back (std::move (optionText));

    return optionTexts;
}

static std::vector<std::string_view> getArgumentList (std::string_view configText, const std::deque<std::string>& optionTexts)
{
    std::vector<std::string_view> argumentList { jam::Strings::getView (Id::page.toString()) };
    size_t ownedIndex { 0 };

    applyToFileOptions (configText, [&argumentList, &optionTexts, &ownedIndex] (std::string_view trimmed)
    {
        if (isQuotedValue (getOptionValue (trimmed)))
        {
            argumentList.push_back (optionTexts.at (ownedIndex));
            ++ownedIndex;
        }
        else
            argumentList.push_back (trimmed);
    });

    for (auto index { ownedIndex }; index < optionTexts.size(); ++index)
        argumentList.push_back (optionTexts.at (index));

    return argumentList;
}

juce::File PageConfiguration::getPageLibraryFile()
{
#if JUCE_WINDOWS
    return Paths::getCarriedFile (files::pageLibraryName, files::pageLibraryWindows);
#else
    return Paths::getCarriedFile (files::pageLibraryName, files::pageLibrary);
#endif
}

static juce::File getPageLibraryFolder()
{
    const auto pageLibraryFile { PageConfiguration::getPageLibraryFile() };

    if (pageLibraryFile.existsAsFile())
        return pageLibraryFile.getParentDirectory();

    return juce::File {};
}

static std::string getStyleSheetFileName (std::string_view themeName)
{
    std::string fileName;
    jam::Format::appendBytes (fileName, themeName);
    jam::Format::appendBytes (fileName, files::styleSheetExtension);
    return fileName;
}

static juce::String getStyleSheetPattern()
{
    std::string pattern;
    jam::Format::appendBytes (pattern, Id::styleSheetWildcard);
    jam::Format::appendBytes (pattern, files::styleSheetExtension);
    return jam::Strings::fromView (pattern);
}

static std::unique_ptr<Input> getStyleSheetText (const juce::File& folder, std::string_view themeName)
{
    const auto fileName { getStyleSheetFileName (themeName) };
    const auto styleSheetFile { folder.getChildFile (jam::Strings::fromView (fileName)) };
    return Input::fromFile (styleSheetFile, Input::allBytes);
}

static bool isNonTheme (std::string_view baseName)
{
    static const std::array nonThemeNames { jam::Strings::getView (Id::style.toString()), files::pageLibraryName };

    for (const auto nonThemeName : nonThemeNames)
        if (nonThemeName.compare (baseName) == 0) return true;

    return false;
}

static void addThemeNames (jam::Strings& themeNames, const juce::File& folder)
{
    for (const auto& styleSheetFile : folder.findChildFiles (juce::File::findFiles, false, getStyleSheetPattern()))
    {
        const auto themeName { styleSheetFile.getFileNameWithoutExtension() };

        if (not isNonTheme (jam::Strings::getView (themeName)))
            themeNames.add (themeName);
    }
}

static std::string_view getConfigText (const std::unique_ptr<Input>& configInput)
{
    if (configInput != nullptr)
        return configInput->getBytes();

    return {};
}

PageConfiguration::PageConfiguration()
    : configDirectory { getConfigDirectoryFromEnvironment() },
      configFile { getConfigFileFromEnvironment (configDirectory) },
      configInput { Input::fromFile (configFile, Input::allBytes) },
      optionTexts { getOptionTexts (getConfigText (configInput)) },
      argumentList { getArgumentList (getConfigText (configInput), optionTexts) },
      arguments { PageOptions::getArguments (argumentList) }
{
}

const Arguments& PageConfiguration::getArguments() const noexcept
{
    return arguments;
}

juce::File PageConfiguration::getConfigFile() const
{
    return configFile;
}

juce::File PageConfiguration::getConfigDirectory() const
{
    return configDirectory;
}

juce::File PageConfiguration::getThemeFolder() const
{
    return configDirectory.getChildFile (jam::Strings::fromView (Id::themeFolderName));
}

std::unique_ptr<Input> PageConfiguration::getStyleSheet (std::string_view themeName) const
{
    auto themeStyleSheet { getStyleSheetText (getThemeFolder(), themeName) };

    if (themeStyleSheet != nullptr)
        return themeStyleSheet;

    return getStyleSheetText (getPageLibraryFolder(), themeName);
}

std::string PageConfiguration::getThemeText() const
{
    static constexpr char newlineChar { static_cast<char> (Chars::newline) };
    jam::Strings themeNames;
    addThemeNames (themeNames, getThemeFolder());
    addThemeNames (themeNames, getPageLibraryFolder());
    themeNames.sort (false);
    themeNames.removeDuplicates (false);

    std::string text;

    for (int index { 0 }; index < themeNames.size(); ++index)
    {
        jam::Format::appendBytes (text, themeNames.getView (index));
        jam::Format::appendByte (text, newlineChar);
    }

    return text;
}
