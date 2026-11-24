#pragma once
#include <JuceHeader.h>
#include "TransparentStringHash.h"

/** @brief Evaluates hierarchical `.gitignore`, repository exclude, and global exclude rules.
 *
 *  The rules form a chain of links. Each link holds the rules of one file and points to its
 *  parent link. A child directory therefore copies none of the rules of its parents.
 */
class IgnoreRules
{
    struct Access {};

public:
    /** @brief Creates one link of the rules chain.
     *
     *  @param newParent   The link of the enclosing rules file, or null for the first link.
     *  @param newTopLevel The directory that the patterns of this link are relative to.
     *  @param newBasePrefix The relative prefix that the patterns of this link carry, with a trailing slash, or empty.
     */
    IgnoreRules (Access, std::shared_ptr<const IgnoreRules> newParent, const juce::File& newTopLevel, std::string newBasePrefix);

    /** @brief Creates the rules chain that applies to a walk.
     *
     *  The chain is built in this order. The global excludes file comes first, as the first link.
     *  The repository exclude file comes next, as a link, when it exists. Then the `.gitignore`
     *  of each directory from the top level down to the walk root is added as a link, when it exists.
     *
     *  @param walkRoot The directory where the walk starts.
     *
     *  @return The last link of the chain.
     */
    static std::shared_ptr<const IgnoreRules> create (const juce::File& walkRoot);

    /** @brief Creates the link for the `.gitignore` of a directory, on top of the parent rules.
     *
     *  The new link points to parentRules, so no rule is copied.
     *
     *  @param parentRules   The rules of the enclosing directory.
     *  @param directory     The directory whose `.gitignore` is read.
     *  @param relativePrefix The path of the directory relative to the top level. Empty for the top level.
     *
     *  @return The new link.
     */
    static std::shared_ptr<const IgnoreRules> getChildRules (const std::shared_ptr<const IgnoreRules>& parentRules,
                                                             const juce::File& directory,
                                                             std::string_view relativePrefix);

    /** @brief Returns a slash-terminated path prefix for @p rootDirectory relative to the ignore root.
     *
     *  @param rootDirectory Traversal root directory.
     *
     *  @return Relative prefix ending in `'/'`, or empty when @p rootDirectory is the top level.
     */
    std::string getRootRelativePrefix (const juce::File& rootDirectory) const;

    /** @brief Tests whether an entry is excluded by the loaded ignore rules.
     *
     *  The nearest link with a matching rule decides. Inside one link, the last matching rule wins,
     *  so a negated pattern overrides an earlier match.
     *
     *  @param relativePath Entry path relative to the ignore top level using `'/'` separators.
     *  @param isDirectory  True when the candidate entry is a directory.
     *
     *  @return True when the winning rule is not negated. False when no rule matches.
     */
    bool isIgnored (std::string_view relativePath, bool isDirectory) const;

private:
    /** @brief One rule of a `.gitignore` line.
     *
     *  Only a rule whose pattern needs a regex holds a compiled expression. The other rules
     *  are found by a hash lookup, and hold none.
     */
    struct Rule
    {
        /** @brief Compiled expression, or null for a rule found by a hash lookup. */
        std::shared_ptr<jam::RegExp> regExp;

        /** @brief Compiled program that runs the expression, or null for a rule found by a hash lookup. */
        std::shared_ptr<jam::RegExpProgram> program;

        /** @brief True for a pattern that starts with `!`, which re-includes an entry. */
        bool isNegated { false };

        /** @brief True for a pattern that ends with `/`, which matches directories only. */
        bool isDirectoryOnly { false };
    };

    /** @brief Maps a key to the indexes of the rules that have that key, in ascending order.
     *
     *  The key is a literal name, a literal path, or an extension. A lookup by key needs no regex,
     *  so the rules that have a key are one hash lookup, not a scan.
     */
    using RuleIndexes = jam::HashMap<std::string, std::vector<int>, TransparentStringHash, std::equal_to<>>;

    static juce::File getTopLevelDirectory (const juce::File& walkRoot);
    static juce::File getGlobalExcludesFile();
    static juce::String getExcludesFileFromConfig (const juce::File& configFile);
    static std::shared_ptr<const IgnoreRules> getLinkedRules (const std::shared_ptr<const IgnoreRules>& parentRules,
                                                              const juce::File& file,
                                                              const juce::File& newTopLevel,
                                                              std::string newBasePrefix);
    static std::string getTranslatedGlobRegex (std::string_view pattern, bool isAnchored);
    void addRulesFromFile (const juce::File& file);
    void addRule (std::string_view rawLine);
    int getLastMatch (std::string_view relativePath, bool isDirectory) const;
    int getLastIndex (const RuleIndexes& indexes, std::string_view key, bool isDirectory) const;
    std::string getRelativePath (const juce::File& entry) const;

    juce::File topLevel;
    std::string basePrefix;
    std::shared_ptr<const IgnoreRules> parent;
    jam::Array<Rule> rules;

    /** @brief Rules whose pattern is a literal file or folder name, such as `build`. Keyed by that name. */
    RuleIndexes basenames;

    /** @brief Rules whose pattern is an anchored literal path, such as `/docs/build`. Keyed by that path. */
    RuleIndexes relativePaths;

    /** @brief Rules whose pattern is an extension, such as `*.o`. Keyed by the extension without the dot. */
    RuleIndexes extensions;

    /** @brief Indexes of the rules whose pattern needs a regex, in ascending order. The search starts from the last one. */
    std::vector<int> others;
};
