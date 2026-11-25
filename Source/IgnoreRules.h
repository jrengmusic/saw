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
        std::unique_ptr<jam::RegExp> regExp;

        /** @brief Compiled program that runs the expression, or null for a rule found by a hash lookup. */
        std::unique_ptr<jam::RegExpProgram> program;

        /** @brief Literals that every match of the expression must contain, or null when the expression requires none.
         *
         *  An entry that contains none of the literals is rejected before the expression runs.
         */
        std::unique_ptr<jam::LiteralSet> literalSet;

        /** @brief True for a pattern that starts with `!`, which re-includes an entry. */
        bool isNegated { false };

        /** @brief True for a pattern that ends with `/`, which matches directories only. */
        bool isDirectoryOnly { false };
    };

    /** @brief Maps a key to the indexes of the rules that have that key, in ascending order.
     *
     *  The key is a literal tail of a path, a literal path, or an extension. A lookup by key needs no regex,
     *  so the rules that have a key are one hash lookup, not a scan.
     */
    using RuleIndexes = jam::HashMap<std::string, jam::Array<int>, TransparentStringHash, std::equal_to<>>;

    /** @brief Returns the top level of the repository that holds a walk root.
     *
     *  The top level is the nearest directory, from walkRoot upward, that holds a `.git` entry.
     *  When no directory holds one, walkRoot is the top level.
     *
     *  @param walkRoot The directory where the walk starts.
     *
     *  @return The top level directory.
     */
    static juce::File getTopLevelDirectory (const juce::File& walkRoot);

    /** @brief Adds the `.gitignore` link of each directory from the top level down to a directory.
     *
     *  The recursion visits the ancestors of the directory first, so the links are added from the top
     *  level downward. A directory without a `.gitignore` adds no link.
     *
     *  @param parentRules The rules chain that the links are added on top of.
     *  @param directory   The walk root, or an ancestor of it.
     *
     *  @return The chain with the new links, or parentRules when no directory has a `.gitignore`.
     */
    static std::shared_ptr<const IgnoreRules> getAncestorRules (const std::shared_ptr<const IgnoreRules>& parentRules,
                                                                const juce::File& directory);

    /** @brief Returns the global excludes file that git would use.
     *
     *  The `core.excludesFile` value of the XDG git config file names the file. The user git config file
     *  takes precedence when it sets the value. A value that starts with the home prefix is relative to
     *  the home folder. Any other value is relative to the working folder. Without a value, the XDG
     *  ignore file is returned, or the ignore file of the home folder when no XDG home is set.
     *
     *  @return The file. The file may not exist.
     */
    static juce::File getGlobalExcludesFile();

    /** @brief Reads the `core.excludesFile` value from one git config file.
     *
     *  A file that does not exist, cannot be read, or holds binary bytes gives an empty value.
     *
     *  @param configFile The git config file to read.
     *
     *  @return The value, or an empty string when none is set.
     */
    static std::string getExcludesFileFromConfig (const juce::File& configFile);

    /** @brief Creates one link of the rules chain, with the rules of one file.
     *
     *  The new link points to parentRules, so no rule is copied. A file that does not exist gives a link
     *  with no rules.
     *
     *  @param parentRules   The link that the new link sits on. Null for the first link.
     *  @param file          The rules file to read.
     *  @param newTopLevel   The directory that the patterns of the new link are relative to.
     *  @param newBasePrefix The prefix that the patterns carry, with a trailing slash, or empty.
     *
     *  @return The new link.
     */
    static std::shared_ptr<const IgnoreRules> getLinkedRules (const std::shared_ptr<const IgnoreRules>& parentRules,
                                                              const juce::File& file,
                                                              const juce::File& newTopLevel,
                                                              std::string newBasePrefix);

    /** @brief Translates one glob pattern of a rules file into a regular expression.
     *
     *  An anchored pattern matches from the start of the relative path. A floating pattern matches
     *  after any slash. A leading `**` that is followed by a slash matches any number of folders.
     *  Each other glob step is translated by appendGlobStep. The expression ends at the end of the path.
     *
     *  @param pattern    The pattern, without its negation mark, directory marker, and anchor.
     *  @param isAnchored True to anchor the pattern at the top level.
     *
     *  @return The regular expression source.
     */
    static std::string getTranslatedGlobRegex (std::string_view pattern, bool isAnchored);

    /** @brief Adds the rules of each line of a rules file to this link.
     *
     *  A file that does not exist, cannot be read, or holds binary bytes adds no rule.
     *
     *  @param file The rules file to read.
     */
    void addRulesFromFile (const juce::File& file);

    /** @brief Adds one rule from one line of a rules file, in the family that its pattern selects.
     *
     *  A literal name, or a literal after a leading folder wildcard `**`, goes to suffixes. An anchored
     *  literal path goes to relativePaths. An extension, with or without a leading folder wildcard, goes
     *  to extensions. Any other glob goes to others.
     *  A glob that does not compile adds no rule. A line without a pattern adds no rule.
     *
     *  @param rawLine The line of the rules file, without its line ending.
     */
    void addRule (std::string_view rawLine);

    /** @brief Returns the index of the last rule of this link that matches the entry.
     *
     *  The rules are looked up by key first: the literal tails of the path at each folder boundary, the
     *  literal path, and the extension. Then the regex rules that come after the best key match are
     *  tested, from the last one backward. A path outside basePrefix matches no rule of this link.
     *
     *  @param relativePath Entry path relative to the ignore top level using `'/'` separators.
     *  @param isDirectory  True when the entry is a directory.
     *
     *  @return The index of the last matching rule, or -1 when no rule of this link matches.
     */
    int getLastMatch (std::string_view relativePath, bool isDirectory) const;

    /** @brief Returns the index of the last rule of a key that applies to the entry type.
     *
     *  A directory-only rule applies to a directory only, so it is skipped for any other entry.
     *
     *  @param indexes     The key map to search: suffixes, relativePaths, or extensions.
     *  @param key         The literal key to look up.
     *  @param isDirectory True when the entry is a directory.
     *
     *  @return The index of the last rule of the key that applies, or -1 when none applies.
     */
    int getLastIndex (const RuleIndexes& indexes, std::string_view key, bool isDirectory) const;

    /** @brief Returns the path of an entry relative to the top level, in display form.
     *
     *  @param entry The entry to describe.
     *
     *  @return The relative path, or empty when the entry is the top level.
     */
    std::string getRelativePath (const juce::File& entry) const;

    /** @brief Directory that the patterns of this link are relative to. */
    juce::File topLevel;

    /** @brief Relative prefix of the folder of this link, with a trailing slash, or empty. Paths outside it match nothing here. */
    std::string basePrefix;

    /** @brief The link of the enclosing rules file, which is consulted after this link. Null for the first link. */
    std::shared_ptr<const IgnoreRules> parent;

    /** @brief Every rule of this link, in file order. The indexes of the containers below point into it. */
    jam::Array<Rule> rules;

    /** @brief Rules whose pattern is a literal name, such as `build`, or a literal after a leading `**`, such as `**`/Builds/x64.
     *
     *  Keyed by that literal. Such a rule applies to an entry at any depth, so getLastMatch looks up every
     *  tail of the path that starts at a folder boundary. A walk needs no expression for these rules.
     */
    RuleIndexes suffixes;

    /** @brief Rules whose pattern is an anchored literal path, such as `/docs/build`. Keyed by that path. */
    RuleIndexes relativePaths;

    /** @brief Rules whose pattern is an extension, such as `*.o`, or `**` followed by `*.o`. Keyed by the extension without the dot. */
    RuleIndexes extensions;

    /** @brief Indexes of the rules whose pattern needs a regex, in ascending order. The search starts from the last one. */
    jam::Array<int> others;
};
