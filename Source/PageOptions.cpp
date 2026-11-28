#include "PageOptions.h"
#include "Stdio.h"
#include "generated/Generated.h"

static constexpr int unknownColumns { 0 };
static constexpr size_t defaultTabWidth { 4 };
static constexpr int plainCountNoDecorations { 2 };
static constexpr int defaultSqueezeLimit { 1 };
static constexpr int minimumSqueezeLimit { 1 };
static constexpr int defaultDiffContext { 0 };
static constexpr int minimumDiffContext { 0 };
static constexpr char plusSign { static_cast<char> (Chars::plus) };
static constexpr char dashSign { static_cast<char> (Chars::dash) };
static constexpr NonPrintable::Notation defaultNotation { NonPrintable::Notation::unicode };
static constexpr bool defaultIsWrapping { true };
static constexpr int noComponents { 0 };
static constexpr int defaultComponents { PageOptions::Component::changes | PageOptions::Component::grid
                                         | PageOptions::Component::headerFileName | PageOptions::Component::numbers
                                         | PageOptions::Component::snip };
static constexpr int allComponents { defaultComponents | PageOptions::Component::rule | PageOptions::Component::headerFileSize };

static constexpr std::array<std::pair<std::string_view, PageOptions::Choice>, 3> choices {{
    { Id::autoValue, PageOptions::Choice::automatic },
    { Id::alwaysValue, PageOptions::Choice::always },
    { Id::neverValue, PageOptions::Choice::never }
}};

static constexpr std::array<std::pair<std::string_view, bool>, 4> wrapModes {{
    { Id::neverValue, false },
    { Id::autoValue, true },
    { Id::characterValue, true },
    { Id::wordValue, true }
}};

static constexpr std::array<std::pair<std::string_view, int>, 12> componentNames {{
    { Id::styleChanges, PageOptions::Component::changes },
    { Id::styleGrid, PageOptions::Component::grid },
    { Id::styleRule, PageOptions::Component::rule },
    { Id::styleHeader, PageOptions::Component::headerFileName },
    { Id::styleHeaderFilename, PageOptions::Component::headerFileName },
    { Id::styleHeaderFilesize, PageOptions::Component::headerFileSize },
    { Id::styleNumbers, PageOptions::Component::numbers },
    { Id::styleSnip, PageOptions::Component::snip },
    { Id::defaultValue, defaultComponents },
    { Id::autoValue, defaultComponents },
    { Id::fullValue, allComponents },
    { Id::plainValue, noComponents }
}};

Arguments PageOptions::getArguments (juce::Span<const std::string_view> argumentList)
{
    return Arguments { argumentList,
                       { { Id::showAllOption, Arguments::OptionType::flag }, { Id::plainOption, Arguments::OptionType::flag },
                         { Id::nonprintableNotationOption, Arguments::OptionType::value }, { Id::binaryOption, Arguments::OptionType::value },
                         { Id::lineRangeOption, Arguments::OptionType::value }, { Id::tabsOption, Arguments::OptionType::value },
                         { Id::wrapOption, Arguments::OptionType::value }, { Id::chopOption, Arguments::OptionType::flag },
                         { Id::terminalWidthOption, Arguments::OptionType::value }, { Id::numberOption, Arguments::OptionType::flag },
                         { Id::numberNonblankOption, Arguments::OptionType::flag },
                         { Id::styleOption, Arguments::OptionType::value }, { Id::decorationsOption, Arguments::OptionType::value },
                         { Id::fileNameOption, Arguments::OptionType::value }, { Id::squeezeBlankOption, Arguments::OptionType::flag },
                         { Id::squeezeLimitOption, Arguments::OptionType::value }, { Id::quietEmptyOption, Arguments::OptionType::flag },
                         { Id::stripAnsiOption, Arguments::OptionType::value },
                         { Id::sanitizeOption, Arguments::OptionType::value }, { Id::languageOption, Arguments::OptionType::value },
                         { Id::listLanguagesOption, Arguments::OptionType::flag }, { Id::mapSyntaxOption, Arguments::OptionType::value },
                         { Id::ignoredSuffixOption, Arguments::OptionType::value }, { Id::fallbackSyntaxOption, Arguments::OptionType::value },
                         { Id::themeOption, Arguments::OptionType::value },
                         { Id::themeLightOption, Arguments::OptionType::value }, { Id::themeDarkOption, Arguments::OptionType::value },
                         { Id::listThemesOption, Arguments::OptionType::flag }, { Id::colorOption, Arguments::OptionType::value },
                         { Id::forceColorOption, Arguments::OptionType::flag }, { Id::italicTextOption, Arguments::OptionType::value },
                         { Id::highlightLineOption, Arguments::OptionType::value }, { Id::diffOption, Arguments::OptionType::flag },
                         { Id::diffContextOption, Arguments::OptionType::value }, { Id::jsonUnbufferedOption, Arguments::OptionType::flag },
                         { Id::terminalTitleOption, Arguments::OptionType::flag }, { Id::completionOption, Arguments::OptionType::value },
                         { Id::diagnosticOption, Arguments::OptionType::flag }, { Id::configFileOption, Arguments::OptionType::flag },
                         { Id::configDirOption, Arguments::OptionType::flag }, { Id::generateConfigFileOption, Arguments::OptionType::flag },
                         { Id::pagingOption, Arguments::OptionType::value }, { Id::pagerOption, Arguments::OptionType::value },
                         { Id::noPagingOption, Arguments::OptionType::flag }, { Id::versionOption, Arguments::OptionType::flag },
                         { Id::acknowledgementsOption, Arguments::OptionType::flag }, { Id::followOption, Arguments::OptionType::flag }, { Id::followNameOption, Arguments::OptionType::flag } },
                       Arguments::Grammar::attached };
}

static const Arguments& getAnsweringArguments (const Arguments& commandLine, const Arguments& configuration, std::string_view option)
{
    if (commandLine.contains (option))
        return commandLine;

    return configuration;
}

static std::string_view getValueText (const Arguments& commandLine,
                                      const Arguments& configuration,
                                      std::string_view option,
                                      std::string_view defaultText)
{
    const auto& answer { getAnsweringArguments (commandLine, configuration, option) };

    if (answer.contains (option))
        return answer.getValueView (option);

    return defaultText;
}

static juce::Result getInvalidValueFailure (std::string_view option, std::string_view value)
{
    return Stdio::fail (text::Diagnostics::failChoiceInvalid,
                        Arguments::getOptionName (option),
                        jam::Strings::getView (Id::diagnosticSeparator),
                        value);
}

static std::optional<PageOptions::Choice> getChoice (std::string_view choiceText)
{
    for (const auto& [choiceName, choice] : choices)
        if (choiceName.compare (choiceText) == 0) return choice;

    return std::nullopt;
}

static bool isForced (const Arguments& commandLine, const Arguments& configuration)
{
    return getAnsweringArguments (commandLine, configuration, Id::forceColorOption).contains (Id::forceColorOption);
}

static bool isChoiceEnabled (PageOptions::Choice choice, bool isTerminal)
{
    if (choice == PageOptions::Choice::automatic)
        return isTerminal;

    return choice == PageOptions::Choice::always;
}

static bool isNumberFlag (std::string_view flag)
{
    return flag.compare (Id::numberOption) == 0 or flag.compare (Id::numberNonblankOption) == 0;
}

static bool isNumbering (const Arguments& commandLine, const Arguments& configuration)
{
    const auto& numberAnswer { getAnsweringArguments (commandLine, configuration, Id::numberOption) };
    const auto& nonblankAnswer { getAnsweringArguments (commandLine, configuration, Id::numberNonblankOption) };
    return numberAnswer.contains (Id::numberOption) or nonblankAnswer.contains (Id::numberNonblankOption);
}

static bool isNumberingCommandLine (const Arguments& commandLine)
{
    return isNumberFlag (commandLine.getLastFlag ({ Id::numberOption, Id::numberNonblankOption, Id::plainOption }));
}

static bool isPlainTwice (const Arguments& commandLine, const Arguments& configuration)
{
    const auto& plainAnswer { getAnsweringArguments (commandLine, configuration, Id::plainOption) };
    return not isNumbering (commandLine, configuration) and plainAnswer.getCount (Id::plainOption) >= plainCountNoDecorations;
}

static std::string_view getStyleFlag (const Arguments& commandLine, const Arguments& configuration)
{
    const auto commandLineFlag { commandLine.getLastFlag ({ Id::numberOption, Id::numberNonblankOption, Id::plainOption }) };

    if (commandLineFlag.size() > 0)
        return commandLineFlag;

    return configuration.getLastFlag ({ Id::numberOption, Id::numberNonblankOption, Id::plainOption });
}

static bool getNumberingNonblank (const Arguments& commandLine, const Arguments& configuration)
{
    const auto& nonblankAnswer { getAnsweringArguments (commandLine, configuration, Id::numberNonblankOption) };
    return nonblankAnswer.contains (Id::numberNonblankOption) and getStyleFlag (commandLine, configuration).compare (Id::plainOption) != 0;
}

static std::string_view getStyleText (const Arguments& commandLine, const Arguments& configuration)
{
    const auto styleFlag { getStyleFlag (commandLine, configuration) };

    if (isNumberFlag (styleFlag))
        return Id::styleNumbers;

    if (styleFlag.compare (Id::plainOption) == 0)
        return Id::plainValue;

    return getValueText (commandLine, configuration, Id::styleOption, Id::defaultValue);
}

static std::optional<bool> parseBinaryText (const Arguments& commandLine, const Arguments& configuration)
{
    const auto& answer { getAnsweringArguments (commandLine, configuration, Id::binaryOption) };

    if (answer.contains (Id::binaryOption))
    {
        const auto binaryText { answer.getValueView (Id::binaryOption) };

        if (binaryText.compare (Id::asTextBinary) == 0)
            return true;

        if (binaryText.compare (Id::noPrintingBinary) == 0)
            return false;

        return std::nullopt;
    }

    return false;
}

static std::optional<NonPrintable::Notation> parseNotation (const Arguments& commandLine, const Arguments& configuration)
{
    const auto& answer { getAnsweringArguments (commandLine, configuration, Id::nonprintableNotationOption) };

    if (answer.contains (Id::nonprintableNotationOption))
        return NonPrintable::getNotation (answer.getValueView (Id::nonprintableNotationOption));

    return defaultNotation;
}

static std::optional<bool> parseDecorated (const Arguments& commandLine, const Arguments& configuration)
{
    const auto choice { getChoice (getValueText (commandLine, configuration, Id::decorationsOption, Id::autoValue)) };

    const bool isTerminal { Stdio::isTerminalOutput() or isNumberingCommandLine (commandLine) };

    if (choice.has_value())
        return not isPlainTwice (commandLine, configuration) and (isForced (commandLine, configuration) or isChoiceEnabled (*choice, isTerminal));

    return std::nullopt;
}

static bool isSignedPart (std::string_view part)
{
    return not part.empty() and (part.front() == plusSign or part.front() == dashSign);
}

static bool isEverySigned (const jam::Strings& parts)
{
    for (int index { 0 }; index < parts.size(); ++index)
        if (not isSignedPart (jam::Format::trim (parts.getView (index)))) return false;

    return true;
}

static std::optional<int> getComponentValue (std::string_view componentName)
{
    for (const auto& [name, components] : componentNames)
        if (name.compare (componentName) == 0) return components;

    return std::nullopt;
}

static std::optional<int> getChangedComponents (int components, std::string_view part)
{
    const auto value { getComponentValue (isSignedPart (part) ? part.substr (1) : part) };

    if (not value.has_value()) return std::nullopt;

    if (isSignedPart (part) and part.front() == dashSign) return components & ~ *value;

    return components | *value;
}

static std::optional<int> getComponents (std::string_view styleText)
{
    const auto parts (jam::Strings::fromTokens (jam::Strings::fromView (styleText), juce::String::charToString (Chars::comma), {}));
    int components { isEverySigned (parts) ? defaultComponents : noComponents };

    for (int index { 0 }; index < parts.size(); ++index)
    {
        const auto updated { getChangedComponents (components, jam::Format::trim (parts.getView (index))) };

        if (not updated.has_value()) return std::nullopt;

        components = *updated;
    }

    return components;
}

static std::optional<int> parseComponents (const Arguments& commandLine, const Arguments& configuration)
{
    return getComponents (getStyleText (commandLine, configuration));
}

static std::optional<size_t> parseTabWidth (const Arguments& commandLine, const Arguments& configuration)
{
    const auto& answer { getAnsweringArguments (commandLine, configuration, Id::tabsOption) };

    if (not answer.contains (Id::tabsOption))
        return defaultTabWidth;

    const auto tabWidth { Arguments::getInteger (answer.getValueView (Id::tabsOption)) };

    if (tabWidth.has_value() and *tabWidth >= 0)
        return static_cast<size_t> (*tabWidth);

    return std::nullopt;
}

static std::optional<bool> getWrapping (std::string_view wrapText)
{
    for (const auto& [modeName, isWrapping] : wrapModes)
        if (modeName.compare (wrapText) == 0) return isWrapping;

    return std::nullopt;
}

static std::optional<bool> parseWrapping (const Arguments& commandLine, const Arguments& configuration)
{
    const auto& chopAnswer { getAnsweringArguments (commandLine, configuration, Id::chopOption) };

    if (chopAnswer.contains (Id::chopOption))
        return false;

    return getWrapping (getValueText (commandLine, configuration, Id::wrapOption, Id::autoValue));
}

static std::optional<int> parseColumns (const Arguments& commandLine, const Arguments& configuration)
{
    const auto& answer { getAnsweringArguments (commandLine, configuration, Id::terminalWidthOption) };

    if (not answer.contains (Id::terminalWidthOption))
        return unknownColumns;

    const auto widthText { answer.getValueView (Id::terminalWidthOption) };
    const bool isOffset { not widthText.empty() and (widthText.front() == plusSign or widthText.front() == dashSign) };
    const bool isPlusOffset { isOffset and widthText.front() == plusSign };
    const auto number { Arguments::getInteger (isPlusOffset ? widthText.substr (1) : widthText) };

    if (number.has_value())
    {
        if (isOffset) return unknownColumns;

        if (*number > 0) return *number;
    }

    return std::nullopt;
}

static std::optional<int> parseSqueezeLimit (const Arguments& commandLine, const Arguments& configuration)
{
    const auto& answer { getAnsweringArguments (commandLine, configuration, Id::squeezeLimitOption) };

    if (not answer.contains (Id::squeezeLimitOption))
        return defaultSqueezeLimit;

    const auto limit { Arguments::getInteger (answer.getValueView (Id::squeezeLimitOption)) };

    if (limit.has_value() and *limit >= minimumSqueezeLimit)
        return limit;

    return std::nullopt;
}

static std::optional<bool> parseAlwaysChoice (const Arguments& commandLine, const Arguments& configuration, std::string_view option)
{
    const auto choice { getChoice (getValueText (commandLine, configuration, option, Id::neverValue)) };

    if (choice.has_value())
        return *choice == PageOptions::Choice::always;

    return std::nullopt;
}

static std::optional<bool> parseSanitize (const Arguments& commandLine, const Arguments& configuration)
{
    const auto choice { getChoice (getValueText (commandLine, configuration, Id::sanitizeOption, Id::neverValue)) };

    if (choice.has_value())
        return isChoiceEnabled (*choice, Stdio::isTerminalOutput());

    return std::nullopt;
}

static std::optional<PageOptions::Choice> parseColourMode (const Arguments& commandLine, const Arguments& configuration)
{
    if (isForced (commandLine, configuration))
        return PageOptions::Choice::always;

    return getChoice (getValueText (commandLine, configuration, Id::colorOption, Id::autoValue));
}

static std::optional<PageOptions::Choice> parsePagingMode (const Arguments& commandLine, const Arguments& configuration)
{
    const auto& noPagingAnswer { getAnsweringArguments (commandLine, configuration, Id::noPagingOption) };

    if (noPagingAnswer.contains (Id::noPagingOption) or isPlainTwice (commandLine, configuration))
        return PageOptions::Choice::never;

    return getChoice (getValueText (commandLine, configuration, Id::pagingOption, Id::autoValue));
}

static std::optional<int> parseDiffContext (const Arguments& commandLine, const Arguments& configuration)
{
    const auto& answer { getAnsweringArguments (commandLine, configuration, Id::diffContextOption) };

    if (not answer.contains (Id::diffContextOption))
        return defaultDiffContext;

    const auto context { Arguments::getInteger (answer.getValueView (Id::diffContextOption)) };

    if (context.has_value() and *context >= minimumDiffContext)
        return *context;

    return std::nullopt;
}

static std::string_view getThemeName (const Arguments& commandLine, const Arguments& configuration)
{
    const auto themeText { getValueText (commandLine, configuration, Id::themeOption, Id::autoValue) };

    if (themeText.compare (Id::darkValue) == 0)
        return getValueText (commandLine, configuration, Id::themeDarkOption, {});

    if (themeText.compare (Id::lightValue) == 0)
        return getValueText (commandLine, configuration, Id::themeLightOption, {});

    return themeText;
}

template <typename Value>
static juce::Result getOptionStatus (const std::optional<Value>& value, std::string_view option, std::string_view valueText)
{
    if (value.has_value())
        return juce::Result::ok();

    return getInvalidValueFailure (option, valueText);
}

static juce::Result getOptionsStatus (const Arguments& commandLine, const Arguments& configuration)
{
    jam::Array<juce::Result> statuses;
    statuses.add (commandLine.getStatus());
    statuses.add (configuration.getStatus());
    statuses.add (getOptionStatus (parseNotation (commandLine, configuration), Id::nonprintableNotationOption, getValueText (commandLine, configuration, Id::nonprintableNotationOption, {})));
    statuses.add (getOptionStatus (parseBinaryText (commandLine, configuration), Id::binaryOption, getValueText (commandLine, configuration, Id::binaryOption, {})));
    statuses.add (getOptionStatus (parseDecorated (commandLine, configuration), Id::decorationsOption, getValueText (commandLine, configuration, Id::decorationsOption, {})));
    statuses.add (getOptionStatus (parseComponents (commandLine, configuration), Id::styleOption, getStyleText (commandLine, configuration)));
    statuses.add (getOptionStatus (parseTabWidth (commandLine, configuration), Id::tabsOption, getValueText (commandLine, configuration, Id::tabsOption, {})));
    statuses.add (getOptionStatus (parseWrapping (commandLine, configuration), Id::wrapOption, getValueText (commandLine, configuration, Id::wrapOption, {})));
    statuses.add (getOptionStatus (parseColumns (commandLine, configuration), Id::terminalWidthOption, getValueText (commandLine, configuration, Id::terminalWidthOption, {})));
    statuses.add (getOptionStatus (parseSqueezeLimit (commandLine, configuration), Id::squeezeLimitOption, getValueText (commandLine, configuration, Id::squeezeLimitOption, {})));
    statuses.add (getOptionStatus (parseAlwaysChoice (commandLine, configuration, Id::stripAnsiOption), Id::stripAnsiOption, getValueText (commandLine, configuration, Id::stripAnsiOption, {})));
    statuses.add (getOptionStatus (parseSanitize (commandLine, configuration), Id::sanitizeOption, getValueText (commandLine, configuration, Id::sanitizeOption, {})));
    statuses.add (getOptionStatus (parseColourMode (commandLine, configuration), Id::colorOption, getValueText (commandLine, configuration, Id::colorOption, {})));
    statuses.add (getOptionStatus (parseAlwaysChoice (commandLine, configuration, Id::italicTextOption), Id::italicTextOption, getValueText (commandLine, configuration, Id::italicTextOption, {})));
    statuses.add (getOptionStatus (parseDiffContext (commandLine, configuration), Id::diffContextOption, getValueText (commandLine, configuration, Id::diffContextOption, {})));
    statuses.add (getOptionStatus (parsePagingMode (commandLine, configuration), Id::pagingOption, getValueText (commandLine, configuration, Id::pagingOption, {})));
    return Stdio::getFirstFailure (statuses);
}

PageOptions::PageOptions (const Arguments& commandLine, const Arguments& configuration)
    : notation { parseNotation (commandLine, configuration).value_or (defaultNotation) },
      isShowingAll { getAnsweringArguments (commandLine, configuration, Id::showAllOption).contains (Id::showAllOption) },
      isBinaryText { parseBinaryText (commandLine, configuration).value_or (false) },
      isDecorated { parseDecorated (commandLine, configuration).value_or (false) },
      isNumberingNonblank { getNumberingNonblank (commandLine, configuration) },
      components { parseComponents (commandLine, configuration).value_or (noComponents) },
      tabWidth { parseTabWidth (commandLine, configuration).value_or (defaultTabWidth) },
      isWrapping { parseWrapping (commandLine, configuration).value_or (defaultIsWrapping) },
      columns { parseColumns (commandLine, configuration).value_or (unknownColumns) },
      isSqueezing { getAnsweringArguments (commandLine, configuration, Id::squeezeBlankOption).contains (Id::squeezeBlankOption) },
      squeezeLimit { parseSqueezeLimit (commandLine, configuration).value_or (defaultSqueezeLimit) },
      isQuietEmpty { getAnsweringArguments (commandLine, configuration, Id::quietEmptyOption).contains (Id::quietEmptyOption) },
      isStrippingAnsi { parseAlwaysChoice (commandLine, configuration, Id::stripAnsiOption).value_or (false) },
      isSanitizing { parseSanitize (commandLine, configuration).value_or (false) },
      lineRangeTexts { getAnsweringArguments (commandLine, configuration, Id::lineRangeOption).getValues (Id::lineRangeOption) },
      fileName { getValueText (commandLine, configuration, Id::fileNameOption, {}) },
      languageName { getValueText (commandLine, configuration, Id::languageOption, {}) },
      fallbackLanguageName { getValueText (commandLine, configuration, Id::fallbackSyntaxOption, {}) },
      themeName { getThemeName (commandLine, configuration) },
      colourMode { parseColourMode (commandLine, configuration).value_or (PageOptions::Choice::automatic) },
      highlightRangeTexts { getAnsweringArguments (commandLine, configuration, Id::highlightLineOption).getValues (Id::highlightLineOption) },
      isDiff { getAnsweringArguments (commandLine, configuration, Id::diffOption).contains (Id::diffOption) },
      diffContext { parseDiffContext (commandLine, configuration).value_or (defaultDiffContext) },
      pagingMode { parsePagingMode (commandLine, configuration).value_or (PageOptions::Choice::automatic) },
      ignoredSuffixes { getAnsweringArguments (commandLine, configuration, Id::ignoredSuffixOption).getValues (Id::ignoredSuffixOption) },
      syntaxMappings { getAnsweringArguments (commandLine, configuration, Id::mapSyntaxOption).getValues (Id::mapSyntaxOption) },
      status { getOptionsStatus (commandLine, configuration) }
{
}

const juce::Result& PageOptions::getStatus() const noexcept
{
    return status;
}
