#pragma once
#include <JuceHeader.h>
#include "Arguments.h"

/** @brief The configuration of the page verb: the config folder and file, the environment options, and the theme folders, read one time. */
class PageConfiguration
{
public:
    /** @brief Reads the configuration from the environment and the config file.
     *
     *  The file options and the environment options form the argument list, in that order.
     */
    PageConfiguration();

    PageConfiguration (const PageConfiguration&) = delete;
    PageConfiguration& operator= (const PageConfiguration&) = delete;

    /** @brief Returns the parsed options of the configuration, with the page verb as the first argument.
     *
     *  @return The parsed arguments.
     */
    const Arguments& getArguments() const noexcept;

    /** @brief Returns the config file, from the config-path variable when it is set, otherwise from the config folder.
     *
     *  @return The config file. It may not exist.
     */
    juce::File getConfigFile() const;

    /** @brief Returns the config folder, from the config-directory variable, else from the XDG config home, else from the home folder.
     *
     *  @return The config folder.
     */
    juce::File getConfigDirectory() const;

    /** @brief Returns the page library file that the carried library build provides for this platform.
     *
     *  @return The library file, or an empty File when none is carried.
     */
    static juce::File getPageLibraryFile();

    /** @brief Returns the theme folder, which is the themes folder of the config folder.
     *
     *  @return The theme folder. It may not exist.
     */
    juce::File getThemeFolder() const;

    /** @brief Returns the style sheet text of a theme. The theme folder is searched first, then the page library folder.
     *
     *  @param themeName The name of the theme.
     *
     *  @return The style sheet text, or an empty optional when neither folder holds the style sheet.
     */
    std::optional<std::string> getStyleSheet (std::string_view themeName) const;

    /** @brief Returns the names of the themes that the theme folder and the page library folder hold, sorted and without duplicates.
     *
     *  @return The theme names, each followed by a line feed.
     */
    std::string getThemeText() const;

private:
    /** @brief The config folder. */
    const juce::File configDirectory;
    /** @brief The config file. */
    const juce::File configFile;
    /** @brief The option texts of the config file and of the environment. They own the strings that argumentList views. */
    const std::deque<std::string> optionTexts;
    /** @brief The page verb, followed by the views of the option texts. */
    const std::vector<std::string_view> argumentList;
    /** @brief The parsed options of argumentList. */
    const Arguments arguments;
};
