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

/**
 * @file Identifiers.h
 * @brief SAW CLI vocabulary.
 */

#pragma once

namespace Id
{
/*_____________________________________________________________________________*/

/** @brief SAW vocabulary — identifiers, CLI option flags, and path, config, and undo constants. */

extern const juce::Identifier find;                        ///< Find command verb.
extern const juce::Identifier replace;                     ///< Replace command verb.
extern const juce::Identifier lines;                       ///< Lines command verb.
extern const juce::Identifier fields;                      ///< Fields command verb.
extern const juce::Identifier patch;                       ///< Patch command verb.
extern const juce::Identifier startCol;                    ///< Match start column key.
extern const juce::Identifier endCol;                      ///< Match end column key.
extern const juce::Identifier startLine;                   ///< Operation start line key.
extern const juce::Identifier deletion;                    ///< Patch delete operation key.
extern const juce::Identifier hash;                        ///< Line or file hash key.
extern const juce::Identifier timestamp;                   ///< Undo transaction timestamp key.
extern const juce::Identifier files;                       ///< Undo transaction files array key.
extern const juce::Identifier blob;                        ///< Undo backup blob file key.
extern const juce::Identifier preHash;                     ///< File hash before apply.
extern const juce::Identifier postHash;                    ///< File hash after apply.
extern const juce::Identifier executable;                  ///< Executable bit flag key.
extern const juce::String     helpOption;                  ///< Show help text.
extern const juce::String     versionOption;               ///< Show tool version.
extern const juce::String     regexOption;                 ///< Treat pattern as a regular expression.
extern const juce::String     ignoreCaseOption;            ///< Match without case sensitivity.
extern const juce::String     exactOption;                 ///< Match entire line text.
extern const juce::String     invertOption;                ///< Select lines that do not match.
extern const juce::String     countOption;                 ///< Output match counts per file.
extern const juce::String     quietOption;                 ///< Suppress standard output lines.
extern const juce::String     lineNumberOption;            ///< Include line numbers in output.
extern const juce::String     noLineNumberOption;          ///< Omit line numbers from output.
extern const juce::String     hashOption;                  ///< Include per-line hashes in output.
extern const juce::String     contextOption;               ///< Show leading and trailing context lines.
extern const juce::String     afterOption;                 ///< Limit search to lines after a start line.
extern const juce::String     linesOption;                 ///< Limit scope to a line range.
extern const juce::String     firstOption;                 ///< Stop after the first match per file.
extern const juce::String     includeOption;               ///< Include paths matching glob pattern.
extern const juce::String     excludeOption;               ///< Exclude paths matching glob pattern.
extern const juce::String     hiddenOption;                ///< Include hidden files and directories.
extern const juce::String     noIgnoreOption;              ///< Disable ignore file filtering.
extern const juce::String     withStdinOption;             ///< Read replacement text from stdin.
extern const juce::String     withFileOption;              ///< Read replacement text from a file.
extern const juce::String     applyOption;                 ///< Write planned changes to disk.
extern const juce::String     expectOption;                ///< Expected edit count guard for apply.
extern const juce::String     deleteOption;                ///< Delete selected lines.
extern const juce::String     replaceOption;               ///< Replace selected lines with text.
extern const juce::String     insertBeforeOption;          ///< Insert text before selected line.
extern const juce::String     insertAfterOption;           ///< Insert text after selected line.
extern const juce::String     stdinOption;                 ///< Read text payload from stdin.
extern const juce::String     verifyHashOption;            ///< Verify line hash before edit.
extern const juce::String     delimiterOption;             ///< Input field delimiter string.
extern const juce::String     outputDelimiterOption;       ///< Output field delimiter string.
extern const juce::String     matchOption;                 ///< Filter lines by pattern before field split.
extern const juce::String     strictOption;                ///< Fail when a field index is out of range.
extern const juce::String     fileOption;                  ///< Read JSON patch input from a file.
extern const juce::String     listOption;                  ///< List recorded undo transactions.
extern const juce::String     forceOption;                 ///< Overwrite drifted files during undo.
extern const juce::String     stdinPath;                   ///< Standard input label in diagnostics and output.
extern const juce::String     currentDirectory;            ///< Current working directory path.
extern const juce::String     gitDirectory;                ///< Git repository metadata directory name.
extern const juce::String     gitignoreFile;               ///< Per-directory ignore file name.
extern const juce::String     gitExcludePath;              ///< Repository-local exclude file relative path.
extern const juce::String     xdgConfigHomeVariable;       ///< XDG config home environment variable name.
extern const juce::String     gitConfigRelativePath;       ///< Relative path to git config under XDG config home.
extern const juce::String     defaultGitConfigRelativePath;///< Default relative path to git config under user home.
extern const juce::String     userGitConfigFile;           ///< User-level git config file name.
extern const juce::String     gitIgnoreRelativePath;       ///< Relative path to global gitignore under XDG config home.
extern const juce::String     defaultGitIgnoreRelativePath;///< Default relative path to global gitignore under home.
extern const juce::String     coreSection;                 ///< Git config core section name.
extern const juce::String     excludesFileKey;             ///< Git config excludesfile key name.
extern const juce::String     homePrefix;                  ///< Tilde home directory path prefix.
extern const juce::String     ignoreCaseRegexPrefix;       ///< Case-insensitive inline regex group prefix.
extern const juce::String     ignoreCaseRegexSuffix;       ///< Case-insensitive inline regex group suffix.
extern const juce::String     exactRegexPrefix;            ///< Exact-line regex group prefix.
extern const juce::String     exactIgnoreCaseRegexPrefix;  ///< Case-insensitive exact-line regex group prefix.
extern const juce::String     exactRegexSuffix;            ///< Exact-line regex group suffix.
extern const juce::String     undoFolder;                  ///< Undo storage directory name.
extern const juce::String     metaFile;                    ///< Undo transaction metadata file name.
extern const juce::String     blobSuffix;                  ///< Undo backup blob file suffix.
extern const juce::String     utcCompactFormat;            ///< Compact UTC timestamp format for transaction ids.
extern const juce::String     utcIsoFormat;                ///< ISO-8601 UTC timestamp format for metadata.

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Id
