#pragma once
#include <JuceHeader.h>

/** @brief Evaluates hierarchical `.gitignore`, repository exclude, and global exclude rules.
 */
class IgnoreRules
{
public:
    /** @brief Loads global, repository, and ancestor directory ignore rules up to @p walkRoot.
     *
     *  @param walkRoot Starting directory for filesystem traversal.
     */
    explicit IgnoreRules (const juce::File& walkRoot);

    /** @brief Returns a child rule set adding rules from @p directory `.gitignore`.
     *
     *  @param directory      Subdirectory whose `.gitignore` file is appended.
     *  @param relativePrefix Forward-slash path of @p directory relative to the ignore top level.
     *
     *  @return New IgnoreRules combining parent rules with rules scoped to @p relativePrefix.
     */
    IgnoreRules getChildRules (const juce::File& directory, std::string_view relativePrefix) const;

    /** @brief Returns a slash-terminated path prefix for @p rootDirectory relative to the ignore root.
     *
     *  @param rootDirectory Traversal root directory.
     *
     *  @return Relative prefix ending in `'/'`, or empty when @p rootDirectory is the top level.
     */
    std::string getRootRelativePrefix (const juce::File& rootDirectory) const;

    /** @brief Tests whether an entry is excluded by the loaded ignore rules.
     *
     *  Evaluates rules from last to first so negated patterns override earlier matches.
     *
     *  @param relativePath Entry path relative to the ignore top level using `'/'` separators.
     *  @param isDirectory  True when the candidate entry is a directory.
     *
     *  @return True when the last matching rule ignores the entry.
     */
    bool isIgnored (std::string_view relativePath, bool isDirectory) const;

private:
    struct Rule
    {
        std::shared_ptr<jam::RegExp> regExp;
        std::string basePrefix;
        bool isNegated { false };
        bool isDirectoryOnly { false };
    };

    IgnoreRules (const juce::File& newTopLevel, jam::Array<Rule> newRules);

    static juce::File getTopLevelDirectory (const juce::File& walkRoot);
    static juce::File getGlobalExcludesFile();
    static juce::String getExcludesFileFromConfig (const juce::File& configFile);
    static std::optional<Rule> getParsedRule (std::string_view rawLine,
                                              const std::string& basePrefixWithSlash);
    static std::string getTranslatedGlobRegex (std::string_view pattern, bool isAnchored);
    void addRulesFromFile (const juce::File& file, std::string_view basePrefix);
    std::string getRelativePath (const juce::File& entry) const;

    juce::File topLevel;
    jam::Array<Rule> rules;
};
