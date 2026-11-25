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

namespace Id
{
/*_____________________________________________________________________________*/

/** @brief SAW vocabulary — identifiers, CLI option flags, and path, config, and undo constants. */

    static constexpr std::string_view helpOption                   { "--help|-h"               };///< Show help text.
    static constexpr std::string_view versionOption                { "--version|-V"            };///< Show tool version.
    static constexpr std::string_view mcpOption                    { "--mcp"                   };///< Serve commands over the Model Context Protocol.
    static constexpr std::string_view regexOption                  { "--regex|-r"              };///< Treat pattern as a regular expression.
    static constexpr std::string_view ignoreCaseOption             { "--ignore-case|-i"        };///< Match without case sensitivity.
    static constexpr std::string_view exactOption                  { "--exact|-x"              };///< Match entire line text.
    static constexpr std::string_view invertOption                 { "--invert|-v"             };///< Select lines that do not match.
    static constexpr std::string_view countOption                  { "--count|-c"              };///< Output match counts per file.
    static constexpr std::string_view filesWithMatchesOption       { "--files-with-matches|-l" };///< List only the paths of files with a match.
    static constexpr std::string_view onlyMatchingOption           { "--only-matching|-o"      };///< Output only the matched text.
    static constexpr std::string_view wordOption                   { "--word|-w"               };///< Match whole words only.
    static constexpr std::string_view quietOption                  { "--quiet|-q"              };///< Suppress standard output lines.
    static constexpr std::string_view lineNumberOption             { "--line-number|-n"        };///< Include line numbers in output.
    static constexpr std::string_view noLineNumberOption           { "--no-line-number"        };///< Omit line numbers from output.
    static constexpr std::string_view hashOption                   { "--hash"                  };///< Include per-line hashes in output.
    static constexpr std::string_view contextOption                { "--context|-C"            };///< Show leading and trailing context lines.
    static constexpr std::string_view afterContextOption           { "--after-context|-A"      };///< Show trailing context lines.
    static constexpr std::string_view beforeContextOption          { "--before-context|-B"     };///< Show leading context lines.
    static constexpr std::string_view afterOption                  { "--after"                 };///< Limit search to lines after a start line.
    static constexpr std::string_view linesOption                  { "--lines"                 };///< Limit scope to a line range.
    static constexpr std::string_view patternOption                { "--pattern|-e"            };///< Add a search pattern.
    static constexpr std::string_view firstOption                  { "--first"                 };///< Stop after the first match per file.
    static constexpr std::string_view includeOption                { "--include"               };///< Include paths matching glob pattern.
    static constexpr std::string_view excludeOption                { "--exclude"               };///< Exclude paths matching glob pattern.
    static constexpr std::string_view hiddenOption                 { "--hidden"                };///< Include hidden files and directories.
    static constexpr std::string_view noIgnoreOption               { "--no-ignore"             };///< Disable ignore file filtering.
    static constexpr std::string_view filesOption                  { "--files"                 };///< List the files that a search reads.
    static constexpr std::string_view withStdinOption              { "--with-stdin"            };///< Read replacement text from stdin.
    static constexpr std::string_view withFileOption               { "--with-file"             };///< Read replacement text from a file.
    static constexpr std::string_view applyOption                  { "--apply"                 };///< Write planned changes to disk.
    static constexpr std::string_view expectOption                 { "--expect"                };///< Expected edit count guard for apply.
    static constexpr std::string_view deleteOption                 { "--delete"                };///< Delete selected lines.
    static constexpr std::string_view replaceOption                { "--replace"               };///< Replace selected lines with text.
    static constexpr std::string_view insertBeforeOption           { "--insert-before"         };///< Insert text before selected line.
    static constexpr std::string_view insertAfterOption            { "--insert-after"          };///< Insert text after selected line.
    static constexpr std::string_view stdinOption                  { "--stdin"                 };///< Read text payload from stdin.
    static constexpr std::string_view verifyHashOption             { "--verify-hash"           };///< Verify line hash before edit.
    static constexpr std::string_view delimiterOption              { "--delimiter|-d"          };///< Input field delimiter string.
    static constexpr std::string_view outputDelimiterOption        { "--output-delimiter|-o"   };///< Output field delimiter string.
    static constexpr std::string_view matchOption                  { "--match"                 };///< Filter lines by pattern before field split.
    static constexpr std::string_view strictOption                 { "--strict"                };///< Fail when a field index is out of range.
    static constexpr std::string_view fileOption                   { "--file"                  };///< Read JSON patch input from a file.
    static constexpr std::string_view listOption                   { "--list"                  };///< List recorded undo transactions.
    static constexpr std::string_view forceOption                  { "--force"                 };///< Overwrite drifted files during undo.
    static constexpr std::string_view stdinPath                    { "<stdin>"                 };///< Standard input label in diagnostics and output.
    static constexpr std::string_view currentDirectory             { "."                       };///< Current working directory path.
    static constexpr std::string_view nullDevice                   { "/dev/null"               };///< POSIX device that gives no byte.
    static constexpr std::string_view gitDirectory                 { ".git"                    };///< Git repository metadata directory name.
    static constexpr std::string_view gitignoreFile                { ".gitignore"              };///< Per-directory ignore file name.
    static constexpr std::string_view gitExcludePath               { ".git/info/exclude"       };///< Repository-local exclude file relative path.
    static constexpr std::string_view xdgConfigHomeVariable        { "XDG_CONFIG_HOME"         };///< XDG config home environment variable name.
    static constexpr std::string_view gitConfigRelativePath        { "git/config"              };///< Relative path to git config under XDG config home.
    static constexpr std::string_view defaultGitConfigRelativePath { ".config/git/config"      };///< Default relative path to git config under user home.
    static constexpr std::string_view userGitConfigFile            { ".gitconfig"              };///< User-level git config file name.
    static constexpr std::string_view gitIgnoreRelativePath        { "git/ignore"              };///< Relative path to global gitignore under XDG config home.
    static constexpr std::string_view defaultGitIgnoreRelativePath { ".config/git/ignore"      };///< Default relative path to global gitignore under home.
    static constexpr std::string_view coreSection                  { "core"                    };///< Git config core section name.
    static constexpr std::string_view excludesFileKey              { "excludesfile"            };///< Git config excludesfile key name.
    static constexpr std::string_view homePrefix                   { "~/"                      };///< Tilde home directory path prefix.
    static constexpr std::string_view ignoreCaseRegexPrefix        { "(\?i:"                   };///< Case-insensitive inline regex group prefix.
    static constexpr std::string_view ignoreCaseRegexSuffix        { ")"                       };///< Case-insensitive inline regex group suffix.
    static constexpr std::string_view exactRegexPrefix             { "^(\?:"                   };///< Exact-line regex group prefix.
    static constexpr std::string_view exactIgnoreCaseRegexPrefix   { "^(\?i:"                  };///< Case-insensitive exact-line regex group prefix.
    static constexpr std::string_view exactRegexSuffix             { ")$"                      };///< Exact-line regex group suffix.
    static constexpr std::string_view wordRegexPrefix              { "(\?<![A-Za-z0-9_])(\?:"  };///< Word-match regex group prefix.
    static constexpr std::string_view wordRegexSuffix              { ")(\?![A-Za-z0-9_])"      };///< Word-match regex group suffix.
    static constexpr std::string_view groupRegexPrefix             { "(\?:"                    };///< Regex group prefix.
    static constexpr std::string_view undoFolder                   { "undo"                    };///< Undo storage directory name.
    static constexpr std::string_view pendingFolder                { "pending"                 };///< Folder of transactions that are not published.
    static constexpr std::string_view blobFolder                   { "blobs"                   };///< Folder of undo backup blob files.
    static constexpr std::string_view recordSuffix                 { ".json"                   };///< Undo transaction record file suffix.
    static constexpr std::string_view blobSuffix                   { ".blob"                   };///< Undo backup blob file suffix.
    static constexpr std::string_view find                         { "find"                    };///< Find command verb.
    static constexpr std::string_view replace                      { "replace"                 };///< Replace command verb.
    static constexpr std::string_view lines                        { "lines"                   };///< Lines command verb.
    static constexpr std::string_view fields                       { "fields"                  };///< Fields command verb.
    static constexpr std::string_view patch                        { "patch"                   };///< Patch command verb.
    static constexpr std::string_view undo                         { "undo"                    };///< Undo command verb.
    static constexpr std::string_view excerpt                      { "excerpt"                 };///< Excerpt command verb.
    static constexpr std::string_view startCol                     { "startCol"                };///< Match start column key.
    static constexpr std::string_view endCol                       { "endCol"                  };///< Match end column key.
    static constexpr std::string_view startLine                    { "startLine"               };///< Operation start line key.
    static constexpr std::string_view lastLine                     { "endLine"                 };///< Operation end line key.
    static constexpr std::string_view deletion                     { "delete"                  };///< Patch delete operation key.
    static constexpr std::string_view insertBefore                 { "insertBefore"            };///< Patch insert-before operation key.
    static constexpr std::string_view insertAfter                  { "insertAfter"             };///< Patch insert-after operation key.
    static constexpr std::string_view hash                         { "hash"                    };///< Line or file hash key.
    static constexpr std::string_view timestamp                    { "timestamp"               };///< Undo transaction timestamp key.
    static constexpr std::string_view transactionCommand           { "command"                 };///< Undo transaction command key.
    static constexpr std::string_view files                        { "files"                   };///< Undo transaction files array key.
    static constexpr std::string_view preHash                      { "preHash"                 };///< File hash before apply.
    static constexpr std::string_view postHash                     { "postHash"                };///< File hash after apply.
    static constexpr std::string_view executable                   { "executable"              };///< Executable bit flag key.
    static constexpr std::string_view run                          { "run"                     };///< Name of the MCP tool that runs a saw command line.
    static constexpr std::string_view exitCode                     { "exitCode"                };///< MCP result key for the exit code.
    static constexpr std::string_view output                       { "output"                  };///< MCP result key for the standard output text.
    static constexpr std::string_view outputBase64                 { "outputBase64"            };///< MCP result key for the standard output in base64.
    static constexpr std::string_view errors                       { "errors"                  };///< MCP result key for the standard error text.
    static constexpr std::string_view errorsBase64                 { "errorsBase64"            };///< MCP result key for the standard error in base64.
    static constexpr std::string_view standardInput                { "stdin"                   };///< MCP request key for the standard input text.

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Id
