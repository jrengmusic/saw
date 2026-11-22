/*******************************************************************************
                        Codegen Annotated Source of Truth
————————————————————————————————————————————————————————————————————————————————

            ░░████████████░░████████████░░████████████░░████████████
            ░░████  ░░████░░████  ░░████░░████  ░░████    ░░████
            ░░████        ░░████  ░░████░░████            ░░████
            ░░████        ░░████████████░░████████████    ░░████
            ░░████        ░░████  ░░████        ░░████    ░░████
            ░░████  ░░████░░████  ░░████░░████  ░░████    ░░████
            ░░████████████░░████  ░░████░░████████████    ░░████

————————————————————————————————————————————————————————————————————————————————
                         FOR YOUR EYES ONLY, DO NOT EDIT
********************************************************************************/

#include <JuceHeader.h>
#include "Generated.h"

namespace Id
{
/*_____________________________________________________________________________*/

/** @brief SAW vocabulary — identifiers, CLI option flags, and path, config, and undo constants. */

    const juce::Identifier find                         { juce::String::fromUTF8 ("find") };
    const juce::Identifier replace                      { juce::String::fromUTF8 ("replace") };
    const juce::Identifier lines                        { juce::String::fromUTF8 ("lines") };
    const juce::Identifier fields                       { juce::String::fromUTF8 ("fields") };
    const juce::Identifier patch                        { juce::String::fromUTF8 ("patch") };
    const juce::Identifier startCol                     { juce::String::fromUTF8 ("startCol") };
    const juce::Identifier endCol                       { juce::String::fromUTF8 ("endCol") };
    const juce::Identifier startLine                    { juce::String::fromUTF8 ("startLine") };
    const juce::Identifier deletion                     { juce::String::fromUTF8 ("delete") };
    const juce::Identifier hash                         { juce::String::fromUTF8 ("hash") };
    const juce::Identifier timestamp                    { juce::String::fromUTF8 ("timestamp") };
    const juce::Identifier files                        { juce::String::fromUTF8 ("files") };
    const juce::Identifier blob                         { juce::String::fromUTF8 ("blob") };
    const juce::Identifier preHash                      { juce::String::fromUTF8 ("preHash") };
    const juce::Identifier postHash                     { juce::String::fromUTF8 ("postHash") };
    const juce::Identifier executable                   { juce::String::fromUTF8 ("executable") };
    const juce::String     helpOption                   { juce::String::fromUTF8 ("--help|-h") };
    const juce::String     versionOption                { juce::String::fromUTF8 ("--version|-V") };
    const juce::String     regexOption                  { juce::String::fromUTF8 ("--regex|-r") };
    const juce::String     ignoreCaseOption             { juce::String::fromUTF8 ("--ignore-case|-i") };
    const juce::String     exactOption                  { juce::String::fromUTF8 ("--exact|-x") };
    const juce::String     invertOption                 { juce::String::fromUTF8 ("--invert|-v") };
    const juce::String     countOption                  { juce::String::fromUTF8 ("--count|-c") };
    const juce::String     quietOption                  { juce::String::fromUTF8 ("--quiet|-q") };
    const juce::String     lineNumberOption             { juce::String::fromUTF8 ("--line-number|-n") };
    const juce::String     noLineNumberOption           { juce::String::fromUTF8 ("--no-line-number") };
    const juce::String     hashOption                   { juce::String::fromUTF8 ("--hash") };
    const juce::String     contextOption                { juce::String::fromUTF8 ("--context|-C") };
    const juce::String     afterOption                  { juce::String::fromUTF8 ("--after") };
    const juce::String     linesOption                  { juce::String::fromUTF8 ("--lines") };
    const juce::String     firstOption                  { juce::String::fromUTF8 ("--first") };
    const juce::String     includeOption                { juce::String::fromUTF8 ("--include") };
    const juce::String     excludeOption                { juce::String::fromUTF8 ("--exclude") };
    const juce::String     hiddenOption                 { juce::String::fromUTF8 ("--hidden") };
    const juce::String     noIgnoreOption               { juce::String::fromUTF8 ("--no-ignore") };
    const juce::String     withStdinOption              { juce::String::fromUTF8 ("--with-stdin") };
    const juce::String     withFileOption               { juce::String::fromUTF8 ("--with-file") };
    const juce::String     applyOption                  { juce::String::fromUTF8 ("--apply") };
    const juce::String     expectOption                 { juce::String::fromUTF8 ("--expect") };
    const juce::String     deleteOption                 { juce::String::fromUTF8 ("--delete") };
    const juce::String     replaceOption                { juce::String::fromUTF8 ("--replace") };
    const juce::String     insertBeforeOption           { juce::String::fromUTF8 ("--insert-before") };
    const juce::String     insertAfterOption            { juce::String::fromUTF8 ("--insert-after") };
    const juce::String     stdinOption                  { juce::String::fromUTF8 ("--stdin") };
    const juce::String     verifyHashOption             { juce::String::fromUTF8 ("--verify-hash") };
    const juce::String     delimiterOption              { juce::String::fromUTF8 ("--delimiter|-d") };
    const juce::String     outputDelimiterOption        { juce::String::fromUTF8 ("--output-delimiter|-o") };
    const juce::String     matchOption                  { juce::String::fromUTF8 ("--match") };
    const juce::String     strictOption                 { juce::String::fromUTF8 ("--strict") };
    const juce::String     fileOption                   { juce::String::fromUTF8 ("--file") };
    const juce::String     listOption                   { juce::String::fromUTF8 ("--list") };
    const juce::String     forceOption                  { juce::String::fromUTF8 ("--force") };
    const juce::String     stdinPath                    { juce::String::fromUTF8 ("<stdin>") };
    const juce::String     currentDirectory             { juce::String::fromUTF8 (".") };
    const juce::String     gitDirectory                 { juce::String::fromUTF8 (".git") };
    const juce::String     gitignoreFile                { juce::String::fromUTF8 (".gitignore") };
    const juce::String     gitExcludePath               { juce::String::fromUTF8 (".git/info/exclude") };
    const juce::String     xdgConfigHomeVariable        { juce::String::fromUTF8 ("XDG_CONFIG_HOME") };
    const juce::String     gitConfigRelativePath        { juce::String::fromUTF8 ("git/config") };
    const juce::String     defaultGitConfigRelativePath { juce::String::fromUTF8 (".config/git/config") };
    const juce::String     userGitConfigFile            { juce::String::fromUTF8 (".gitconfig") };
    const juce::String     gitIgnoreRelativePath        { juce::String::fromUTF8 ("git/ignore") };
    const juce::String     defaultGitIgnoreRelativePath { juce::String::fromUTF8 (".config/git/ignore") };
    const juce::String     coreSection                  { juce::String::fromUTF8 ("core") };
    const juce::String     excludesFileKey              { juce::String::fromUTF8 ("excludesfile") };
    const juce::String     homePrefix                   { juce::String::fromUTF8 ("~/") };
    const juce::String     ignoreCaseRegexPrefix        { juce::String::fromUTF8 ("(\?i:") };
    const juce::String     ignoreCaseRegexSuffix        { juce::String::fromUTF8 (")") };
    const juce::String     exactRegexPrefix             { juce::String::fromUTF8 ("^(\?:") };
    const juce::String     exactIgnoreCaseRegexPrefix   { juce::String::fromUTF8 ("^(\?i:") };
    const juce::String     exactRegexSuffix             { juce::String::fromUTF8 (")$") };
    const juce::String     undoFolder                   { juce::String::fromUTF8 ("undo") };
    const juce::String     metaFile                     { juce::String::fromUTF8 ("meta.json") };
    const juce::String     blobSuffix                   { juce::String::fromUTF8 (".blob") };
    const juce::String     utcCompactFormat             { juce::String::fromUTF8 ("%Y%m%dT%H%M%S") };
    const juce::String     utcIsoFormat                 { juce::String::fromUTF8 ("%Y-%m-%dT%H:%M:%SZ") };

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Id
namespace files
{
/*_____________________________________________________________________________*/

/** @brief File names saw reads: help texts. */

const juce::String sawHelp     { juce::String::fromUTF8 ("HELP.md") };
const juce::String findHelp    { juce::String::fromUTF8 ("find.md") };
const juce::String replaceHelp { juce::String::fromUTF8 ("replace.md") };
const juce::String linesHelp   { juce::String::fromUTF8 ("lines.md") };
const juce::String fieldsHelp  { juce::String::fromUTF8 ("fields.md") };
const juce::String patchHelp   { juce::String::fromUTF8 ("patch.md") };
const juce::String undoHelp    { juce::String::fromUTF8 ("undo.md") };

/**______________________________END OF NAMESPACE______________________________*/
}// namespace files
namespace map
{
/*_____________________________________________________________________________*/

const jam::HashMap<juce::String, juce::String> banner
{
    { juce::String::fromUTF8 ("blueMana"),         juce::String::fromUTF8 ("\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88  \xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88  \xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88        \xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88") },
    { juce::String::fromUTF8 ("helloSummer"),      juce::String::fromUTF8 ("\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x91\xe2\x96\x91\xe2\x96\x91\xe2\x96\x91\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88  \xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x91\xe2\x96\x91\xe2\x96\x91\xe2\x96\x91\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88  \xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88        \xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88") },
    { juce::String::fromUTF8 ("highBlue"),         juce::String::fromUTF8 ("\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88    \xe2\x96\x91\xe2\x96\x91\xe2\x96\x91\xe2\x96\x91  \xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88    \xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88  \xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88  \xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88  \xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88") },
    { juce::String::fromUTF8 ("aquarius"),         juce::String::fromUTF8 ("\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88  \xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88  \xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88  \xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88  \xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88") },
    { juce::String::fromUTF8 ("homeworld"),        juce::String::fromUTF8 ("\xe2\x96\x91\xe2\x96\x91\xe2\x96\x91\xe2\x96\x91\xe2\x96\x91\xe2\x96\x91\xe2\x96\x91\xe2\x96\x91\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88  \xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x91\xe2\x96\x91\xe2\x96\x91\xe2\x96\x91\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88  \xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88  \xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88  \xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88") },
    { juce::String::fromUTF8 ("oceanBlue"),        juce::String::fromUTF8 ("\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88    \xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88  \xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88    \xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88  \xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88") },
    { juce::String::fromUTF8 ("swimmer"),          juce::String::fromUTF8 ("\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88  \xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88    \xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88  \xe2\x96\x91\xe2\x96\x91\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x91\xe2\x96\x91\xe2\x96\x91\xe2\x96\x91\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x91\xe2\x96\x91") },
    { juce::String::fromUTF8 ("wayBeyondTheBlue"), juce::String::fromUTF8 ("\xe2\x96\x91\xe2\x96\x91\xe2\x96\x91\xe2\x96\x91\xe2\x96\x91\xe2\x96\x91\xe2\x96\x91\xe2\x96\x91\xe2\x96\x91\xe2\x96\x91\xe2\x96\x91\xe2\x96\x91  \xe2\x96\x91\xe2\x96\x91\xe2\x96\x91\xe2\x96\x91    \xe2\x96\x91\xe2\x96\x91\xe2\x96\x91\xe2\x96\x91    \xe2\x96\x91\xe2\x96\x91\xe2\x96\x91\xe2\x96\x91    \xe2\x96\x91\xe2\x96\x91\xe2\x96\x91\xe2\x96\x91  ") },
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace map
