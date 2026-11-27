#pragma once
#include <JuceHeader.h>
#include "Arguments.h"
#include "NonPrintable.h"

/** @brief The options of the page verb, resolved from the command line and the configuration, where the command line answers first for each option. */
class PageOptions
{
public:
    /** @brief The three-way choice of an option that takes auto, always, or never. */
    enum class Choice { automatic, always, never };

    /** @brief The page components, which are bit flags that the style option selects. */
    struct Component
    {
        /** @brief The bit values of the components: changes, grid, rule, header file name, header file size, numbers, and snip. */
        enum value : int
        {
            changes = 1,
            grid = 2,
            rule = 4,
            headerFileName = 8,
            headerFileSize = 16,
            numbers = 32,
            snip = 64
        };
    };

    /** @brief Resolves each option from the command line, and from the configuration when the command line does not give it.
     *
     *  @param commandLine   The parsed arguments of the command line.
     *  @param configuration The parsed arguments of the configuration.
     */
    PageOptions (const Arguments& commandLine, const Arguments& configuration);

    /** @brief Returns the parsed arguments of the page options, with the attached grammar, where an option value is written as `--option=value`.
     *
     *  @param argumentList The argument list, with the verb name at index 0.
     *
     *  @return The parsed arguments.
     */
    static Arguments getArguments (juce::Span<const std::string_view> argumentList);

    /** @brief Returns the first failure of the command line, of the configuration, and of the option values, or ok.
     *
     *  @return The status.
     */
    const juce::Result& getStatus() const noexcept;

    /** @brief The notation of the non-printable bytes, from the nonprintable-notation option, or unicode by default. */
    const NonPrintable::Notation notation;
    /** @brief True when the show-all option answers. */
    const bool isShowingAll;
    /** @brief True when the binary option gives the as-text value. False by default. */
    const bool isBinaryText;
    /** @brief True when decorations are shown, from the decorations option and the terminal. False by default. */
    const bool isDecorated;
    /** @brief True when the number-nonblank option answers and the style is not plain. */
    const bool isNumberingNonblank;
    /** @brief The bit set of the page components, from the style option, or no components when the style is invalid. */
    const int components;
    /** @brief The tab width in columns, from the tabs option. The default is four columns. */
    const size_t tabWidth;
    /** @brief True when long lines wrap, from the wrap option. The chop option turns wrapping off. True by default. */
    const bool isWrapping;
    /** @brief The terminal width in columns, from the terminal-width option, or zero when it is unknown or an offset. */
    const int columns;
    /** @brief True when the squeeze-blank option answers. */
    const bool isSqueezing;
    /** @brief The number of empty lines that a run of empty lines keeps, from the squeeze-limit option. The default is one. */
    const int squeezeLimit;
    /** @brief True when the quiet-empty option answers. */
    const bool isQuietEmpty;
    /** @brief True when the strip-ansi choice resolves to always. False by default. */
    const bool isStrippingAnsi;
    /** @brief True when the sanitize choice enables sanitizing for the terminal. False by default. */
    const bool isSanitizing;
    /** @brief The values of the line-range option, in the order given. */
    const juce::Span<const std::string_view> lineRangeTexts;
    /** @brief The file name shown in the header, from the file-name option, or empty. */
    const std::string_view fileName;
    /** @brief The language name, from the language option, or empty. */
    const std::string_view languageName;
    /** @brief The fallback language name, from the fallback-syntax option, or empty. */
    const std::string_view fallbackLanguageName;
    /** @brief The theme name: the dark or the light theme option when the theme is dark or light, otherwise the theme value. */
    const std::string_view themeName;
    /** @brief The colour mode, from the force-colour option, which gives always, or from the colour option. Automatic by default. */
    const Choice colourMode;
    /** @brief The values of the highlight-line option, in the order given. */
    const juce::Span<const std::string_view> highlightRangeTexts;
    /** @brief True when the diff option answers. */
    const bool isDiff;
    /** @brief The number of diff context lines, from the diff-context option. The default is zero. */
    const int diffContext;
    /** @brief The paging mode. The no-paging option, or the plain option given twice, gives never. Automatic by default. */
    const Choice pagingMode;
    /** @brief The values of the ignored-suffix option, in the order given. */
    const juce::Span<const std::string_view> ignoredSuffixes;
    /** @brief The values of the map-syntax option, in the order given. */
    const juce::Span<const std::string_view> syntaxMappings;

private:
    /** @brief The first failure of the command line, of the configuration, and of the option values, or ok. */
    const juce::Result status;
};
