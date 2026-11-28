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

    static constexpr std::string_view helpOption                     { "--help|-h"                                                 };///< Show help text.
    static constexpr std::string_view versionOption                  { "--version|-V"                                              };///< Show tool version.
    static constexpr std::string_view acknowledgementsOption         { "--acknowledgements"                                        };///< Show the acknowledgements of the bundled libraries.
    static constexpr std::string_view mcpOption                      { "--mcp"                                                     };///< Serve commands over the Model Context Protocol.
    static constexpr std::string_view regexOption                    { "--regex|-r"                                                };///< Treat pattern as a regular expression.
    static constexpr std::string_view ignoreCaseOption               { "--ignore-case|-i"                                          };///< Match without case sensitivity.
    static constexpr std::string_view exactOption                    { "--exact|-x"                                                };///< Match entire line text.
    static constexpr std::string_view invertOption                   { "--invert|-v"                                               };///< Select lines that do not match.
    static constexpr std::string_view countOption                    { "--count|-c"                                                };///< Output match counts per file.
    static constexpr std::string_view filesWithMatchesOption         { "--files-with-matches|-l"                                   };///< List only the paths of files with a match.
    static constexpr std::string_view onlyMatchingOption             { "--only-matching|-o"                                        };///< Output only the matched text.
    static constexpr std::string_view wordOption                     { "--word|-w"                                                 };///< Match whole words only.
    static constexpr std::string_view quietOption                    { "--quiet|-q"                                                };///< Suppress standard output lines.
    static constexpr std::string_view lineNumberOption               { "--line-number|-n"                                          };///< Include line numbers in output.
    static constexpr std::string_view noLineNumberOption             { "--no-line-number"                                          };///< Omit line numbers from output.
    static constexpr std::string_view hashOption                     { "--hash"                                                    };///< Include per-line hashes in output.
    static constexpr std::string_view contextOption                  { "--context|-C"                                              };///< Show leading and trailing context lines.
    static constexpr std::string_view afterContextOption             { "--after-context|-A"                                        };///< Show trailing context lines.
    static constexpr std::string_view beforeContextOption            { "--before-context|-B"                                       };///< Show leading context lines.
    static constexpr std::string_view afterOption                    { "--after"                                                   };///< Limit search to lines after a start line.
    static constexpr std::string_view linesOption                    { "--lines"                                                   };///< Limit scope to a line range.
    static constexpr std::string_view patternOption                  { "--pattern|-e"                                              };///< Add a search pattern.
    static constexpr std::string_view firstOption                    { "--first"                                                   };///< Stop after the first match per file.
    static constexpr std::string_view includeOption                  { "--include"                                                 };///< Include paths matching glob pattern.
    static constexpr std::string_view excludeOption                  { "--exclude"                                                 };///< Exclude paths matching glob pattern.
    static constexpr std::string_view hiddenOption                   { "--hidden"                                                  };///< Include hidden files and directories.
    static constexpr std::string_view noIgnoreOption                 { "--no-ignore"                                               };///< Disable ignore file filtering.
    static constexpr std::string_view noLimitOption                  { "--no-limit"                                                };///< Print the whole output, without the output limit.
    static constexpr std::string_view filesOption                    { "--files"                                                   };///< List the files that a search reads.
    static constexpr std::string_view followOption                   { "--follow"                                                  };///< Print the bytes appended to the file as it grows.
    static constexpr std::string_view followNameOption               { "--follow-name|-F"                                          };///< Follow the file name, and reopen the file when it is replaced.
    static constexpr std::string_view maxDepthOption                 { "--max-depth|-d"                                            };///< Depth limit of the --files walk; children of the root are depth 1.
    static constexpr std::string_view typeOption                     { "--type|-t"                                                 };///< Entry type that --files lists: file, dir, symlink. Repeatable.
    static constexpr std::string_view sizeOption                     { "--size|-S"                                                 };///< Size filter of --files: +N at least, -N at most, N exact. Repeatable.
    static constexpr std::string_view changedWithinOption            { "--changed-within"                                          };///< Keep --files entries modified after a date or duration.
    static constexpr std::string_view changedBeforeOption            { "--changed-before"                                          };///< Keep --files entries modified before a date or duration.
    static constexpr std::string_view sortOption                     { "--sort"                                                    };///< Order of --files: name, mtime, size.
    static constexpr std::string_view reverseOption                  { "--reverse"                                                 };///< Reverse the order of --files.
    static constexpr std::string_view jsonOption                     { "--json"                                                    };///< Print one JSON object per --files entry.
    static constexpr std::string_view fileValue                      { "file|f"                                                    };///< Entry type: regular file.
    static constexpr std::string_view dirValue                       { "dir|d"                                                     };///< Entry type: folder.
    static constexpr std::string_view symlinkValue                   { "symlink|l"                                                 };///< Entry type: symbolic link.
    static constexpr std::string_view nameValue                      { "name"                                                      };///< Sort key: display path.
    static constexpr std::string_view mtimeValue                     { "mtime"                                                     };///< Sort key and JSON key: modification time.
    static constexpr std::string_view sizeValue                      { "size"                                                      };///< Sort key and JSON key: size in bytes.
    static constexpr std::string_view target                         { "target"                                                    };///< JSON key of the link target, empty for a non-link.
    static constexpr std::string_view epochPrefix                    { "@"                                                         };///< Prefix of a time given as seconds since the Unix epoch.
    static constexpr std::string_view byteUnit                       { "b"                                                         };///< Size unit: bytes.
    static constexpr std::string_view kiloUnit                       { "k|kb"                                                      };///< Size unit: 1000 bytes.
    static constexpr std::string_view megaUnit                       { "m|mb"                                                      };///< Size unit: 1000 kilo.
    static constexpr std::string_view gigaUnit                       { "g|gb"                                                      };///< Size unit: 1000 mega.
    static constexpr std::string_view teraUnit                       { "t|tb"                                                      };///< Size unit: 1000 giga.
    static constexpr std::string_view kibiUnit                       { "ki|kib"                                                    };///< Size unit: 1024 bytes.
    static constexpr std::string_view mebiUnit                       { "mi|mib"                                                    };///< Size unit: 1024 kibi.
    static constexpr std::string_view gibiUnit                       { "gi|gib"                                                    };///< Size unit: 1024 mebi.
    static constexpr std::string_view tebiUnit                       { "ti|tib"                                                    };///< Size unit: 1024 gibi.
    static constexpr std::string_view secondUnit                     { "s|sec|secs|second|seconds"                                 };///< Duration unit: seconds.
    static constexpr std::string_view minuteUnit                     { "m|min|mins|minute|minutes"                                 };///< Duration unit: minutes.
    static constexpr std::string_view hourUnit                       { "h|hr|hrs|hour|hours"                                       };///< Duration unit: hours.
    static constexpr std::string_view dayUnit                        { "d|day|days"                                                };///< Duration unit: days.
    static constexpr std::string_view weekUnit                       { "w|wk|wks|week|weeks"                                       };///< Duration unit: weeks.
    static constexpr std::string_view monthUnit                      { "mo|mos|month|months"                                       };///< Duration unit: calendar months.
    static constexpr std::string_view yearUnit                       { "y|yr|yrs|year|years"                                       };///< Duration unit: calendar years.
    static constexpr std::string_view textOption                     { "--text|-a"                                                 };///< Read a binary file as text.
    static constexpr std::string_view withStdinOption                { "--with-stdin"                                              };///< Read replacement text from stdin.
    static constexpr std::string_view withFileOption                 { "--with-file"                                               };///< Read replacement text from a file.
    static constexpr std::string_view applyOption                    { "--apply"                                                   };///< Write planned changes to disk.
    static constexpr std::string_view expectOption                   { "--expect"                                                  };///< Expected edit count guard for apply.
    static constexpr std::string_view appendOption                   { "--append|-a"                                               };///< Append standard input to an existing file.
    static constexpr std::string_view deleteOption                   { "--delete"                                                  };///< Delete selected lines.
    static constexpr std::string_view replaceOption                  { "--replace"                                                 };///< Replace selected lines with text.
    static constexpr std::string_view insertBeforeOption             { "--insert-before"                                           };///< Insert text before selected line.
    static constexpr std::string_view insertAfterOption              { "--insert-after"                                            };///< Insert text after selected line.
    static constexpr std::string_view stdinOption                    { "--stdin"                                                   };///< Read text payload from stdin.
    static constexpr std::string_view verifyHashOption               { "--verify-hash"                                             };///< Verify line hash before edit.
    static constexpr std::string_view delimiterOption                { "--delimiter|-d"                                            };///< Input field delimiter string.
    static constexpr std::string_view outputDelimiterOption          { "--output-delimiter|-o"                                     };///< Output field delimiter string.
    static constexpr std::string_view matchOption                    { "--match"                                                   };///< Filter lines by pattern before field split.
    static constexpr std::string_view strictOption                   { "--strict"                                                  };///< Fail when a field index is out of range.
    static constexpr std::string_view fileOption                     { "--file"                                                    };///< Read JSON patch input from a file.
    static constexpr std::string_view listOption                     { "--list"                                                    };///< List recorded undo transactions.
    static constexpr std::string_view forceOption                    { "--force"                                                   };///< Overwrite drifted files during undo.
    static constexpr std::string_view showAllOption                  { "--show-all|-A"                                             };///< Show non-printable bytes in notation.
    static constexpr std::string_view plainOption                    { "--plain|-p"                                                };///< Plain output, counted.
    static constexpr std::string_view nonprintableNotationOption     { "--nonprintable-notation"                                   };///< Notation of non-printable bytes.
    static constexpr std::string_view binaryOption                   { "--binary"                                                  };///< Binary output mode.
    static constexpr std::string_view lineRangeOption                { "--line-range|-r"                                           };///< Line range of each file, repeatable.
    static constexpr std::string_view tabsOption                     { "--tabs"                                                    };///< Tab width in columns; 0 passes tabs through.
    static constexpr std::string_view wrapOption                     { "--wrap"                                                    };///< Wrap mode of long lines: auto, never, character, word.
    static constexpr std::string_view chopOption                     { "--chop-long-lines|-S"                                      };///< Never wrap long lines.
    static constexpr std::string_view terminalWidthOption            { "--terminal-width"                                          };///< Output width in columns, or an offset with + or -.
    static constexpr std::string_view numberOption                   { "--number|-n"                                               };///< Line numbers as the only style component.
    static constexpr std::string_view numberNonblankOption           { "--number-nonblank|-b"                                      };///< Line numbers of the non-blank lines, as the style.
    static constexpr std::string_view styleOption                    { "--style"                                                   };///< Components of the decoration, comma separated.
    static constexpr std::string_view decorationsOption              { "--decorations"                                             };///< When to draw decorations: auto, never, always.
    static constexpr std::string_view fileNameOption                 { "--file-name"                                               };///< File name shown in the header.
    static constexpr std::string_view squeezeBlankOption             { "--squeeze-blank|-s"                                        };///< Squeeze consecutive empty lines.
    static constexpr std::string_view squeezeLimitOption             { "--squeeze-limit"                                           };///< Most consecutive empty lines kept when squeezing.
    static constexpr std::string_view quietEmptyOption               { "--quiet-empty|-E"                                          };///< Print no output for an empty file.
    static constexpr std::string_view stripAnsiOption                { "--strip-ansi"                                              };///< Strip ANSI escape sequences: auto, always, never.
    static constexpr std::string_view sanitizeOption                 { "--sanitize"                                                };///< Replace terminal-active bytes with U+FFFD.
    static constexpr std::string_view languageOption                 { "--language|-l"                                             };///< Explicitly set the syntax language.
    static constexpr std::string_view listLanguagesOption            { "--list-languages|-L"                                       };///< List the supported syntax languages.
    static constexpr std::string_view mapSyntaxOption                { "--map-syntax|-m"                                           };///< Map a glob pattern to a syntax name.
    static constexpr std::string_view ignoredSuffixOption            { "--ignored-suffix"                                          };///< Ignore a file name suffix for syntax detection.
    static constexpr std::string_view fallbackSyntaxOption           { "--fallback-syntax|--fallback-language"                     };///< Syntax used when no other rule names one.
    static constexpr std::string_view themeOption                    { "--theme"                                                   };///< Set the theme for syntax highlighting.
    static constexpr std::string_view themeLightOption               { "--theme-light"                                             };///< Theme used with a light terminal background.
    static constexpr std::string_view themeDarkOption                { "--theme-dark"                                              };///< Theme used with a dark terminal background.
    static constexpr std::string_view listThemesOption               { "--list-themes"                                             };///< List the supported themes.
    static constexpr std::string_view colorOption                    { "--color"                                                   };///< Colour output: auto, never, always.
    static constexpr std::string_view forceColorOption               { "--force-colorization|-f"                                   };///< Enable decorations and colour, even when piped.
    static constexpr std::string_view italicTextOption               { "--italic-text"                                             };///< Use italic text in the output: always, never.
    static constexpr std::string_view highlightLineOption            { "--highlight-line|-H"                                       };///< Highlight the specified line ranges.
    static constexpr std::string_view diffOption                     { "--diff|-d"                                                 };///< Show only lines changed against the Git index.
    static constexpr std::string_view diffContextOption              { "--diff-context"                                            };///< Context lines around changed lines.
    static constexpr std::string_view terminalTitleOption            { "--set-terminal-title"                                      };///< Set the terminal title to the file names.
    static constexpr std::string_view completionOption               { "--completion"                                              };///< Show shell completion for a shell.
    static constexpr std::string_view diagnosticOption               { "--diagnostic"                                              };///< Show diagnostic information for bug reports.
    static constexpr std::string_view configFileOption               { "--config-file"                                             };///< Show the path of the configuration file.
    static constexpr std::string_view configDirOption                { "--config-dir"                                              };///< Show the path of the configuration directory.
    static constexpr std::string_view generateConfigFileOption       { "--generate-config-file"                                    };///< Generate a default configuration file.
    static constexpr std::string_view pagingOption                   { "--paging"                                                  };///< When to use the pager: auto, never, always.
    static constexpr std::string_view pagerOption                    { "--pager"                                                   };///< Command of the pager.
    static constexpr std::string_view noPagingOption                 { "-P"                                                        };///< Disable the pager.
    static constexpr std::string_view autoValue                      { "auto"                                                      };///< Automatic choice of a mode.
    static constexpr std::string_view neverValue                     { "never"                                                     };///< Mode that turns a choice off.
    static constexpr std::string_view alwaysValue                    { "always"                                                    };///< Mode that turns a choice on.
    static constexpr std::string_view okValue                        { "ok"                                                        };///< Status of a command that succeeded.
    static constexpr std::string_view totalValue                     { "total"                                                     };///< Name of the line of summed counts.
    static constexpr std::string_view mismatchValue                  { "mismatch"                                                  };///< Status of a command that found no match.
    static constexpr std::string_view errorValue                     { "error"                                                     };///< Status of a command that failed.
    static constexpr std::string_view darkValue                      { "dark"                                                      };///< Use the dark theme, from theme-dark.
    static constexpr std::string_view lightValue                     { "light"                                                     };///< Use the light theme, from theme-light.
    static constexpr std::string_view characterValue                 { "character"                                                 };///< Wrap at the width, by character.
    static constexpr std::string_view wordValue                      { "word"                                                      };///< Wrap at the last space before the width.
    static constexpr std::string_view plainValue                     { "plain"                                                     };///< Style set without components.
    static constexpr std::string_view defaultValue                   { "default"                                                   };///< Style set of grid, header, numbers and snip.
    static constexpr std::string_view fullValue                      { "full"                                                      };///< Style set of default and the file size.
    static constexpr std::string_view styleChanges                   { "changes"                                                   };///< Git change column.
    static constexpr std::string_view styleGrid                      { "grid"                                                      };///< Line between the gutter and the text.
    static constexpr std::string_view styleRule                      { "rule"                                                      };///< Horizontal line between files.
    static constexpr std::string_view styleHeader                    { "header"                                                    };///< File name line of each file.
    static constexpr std::string_view styleHeaderFilename            { "header-filename"                                           };///< File name line of each file.
    static constexpr std::string_view styleHeaderFilesize            { "header-filesize"                                           };///< Size line of each file.
    static constexpr std::string_view styleNumbers                   { "numbers"                                                   };///< Line number of each line.
    static constexpr std::string_view styleSnip                      { "snip"                                                      };///< Marker between two ranges that are not adjacent.
    static constexpr std::string_view stdinPath                      { "<stdin>"                                                   };///< Standard input label in diagnostics and output.
    static constexpr std::string_view stdinArgument                  { "-"                                                         };///< Path argument that names standard input.
    static constexpr std::string_view currentDirectory               { "."                                                         };///< Current working directory path.
    static constexpr std::string_view nullDevice                     { "/dev/null"                                                 };///< POSIX device that gives no byte.
    static constexpr std::string_view gitDirectory                   { ".git"                                                      };///< Git repository metadata directory name.
    static constexpr std::string_view gitignoreFile                  { ".gitignore"                                                };///< Per-directory ignore file name.
    static constexpr std::string_view gitExcludePath                 { ".git/info/exclude"                                         };///< Repository-local exclude file relative path.
    static constexpr std::string_view xdgConfigHomeVariable          { "XDG_CONFIG_HOME"                                           };///< XDG config home environment variable name.
    static constexpr std::string_view gitConfigRelativePath          { "git/config"                                                };///< Relative path to git config under XDG config home.
    static constexpr std::string_view defaultGitConfigRelativePath   { ".config/git/config"                                        };///< Default relative path to git config under user home.
    static constexpr std::string_view userGitConfigFile              { ".gitconfig"                                                };///< User-level git config file name.
    static constexpr std::string_view gitIgnoreRelativePath          { "git/ignore"                                                };///< Relative path to global gitignore under XDG config home.
    static constexpr std::string_view defaultGitIgnoreRelativePath   { ".config/git/ignore"                                        };///< Default relative path to global gitignore under home.
    static constexpr std::string_view coreSection                    { "core"                                                      };///< Git config core section name.
    static constexpr std::string_view excludesFileKey                { "excludesfile"                                              };///< Git config excludesfile key name.
    static constexpr std::string_view homePrefix                     { "~/"                                                        };///< Tilde home directory path prefix.
    static constexpr std::string_view ignoreCaseRegexPrefix          { "(\?i:"                                                     };///< Case-insensitive inline regex group prefix.
    static constexpr std::string_view ignoreCaseRegexSuffix          { ")"                                                         };///< Case-insensitive inline regex group suffix.
    static constexpr std::string_view exactRegexPrefix               { "^(\?:"                                                     };///< Exact-line regex group prefix.
    static constexpr std::string_view exactIgnoreCaseRegexPrefix     { "^(\?i:"                                                    };///< Case-insensitive exact-line regex group prefix.
    static constexpr std::string_view exactRegexSuffix               { ")$"                                                        };///< Exact-line regex group suffix.
    static constexpr std::string_view wordRegexPrefix                { "(\?<![A-Za-z0-9_])(\?:"                                    };///< Word-match regex group prefix.
    static constexpr std::string_view wordRegexSuffix                { ")(\?![A-Za-z0-9_])"                                        };///< Word-match regex group suffix.
    static constexpr std::string_view groupRegexPrefix               { "(\?:"                                                      };///< Regex group prefix.
    static constexpr std::string_view undoFolder                     { "undo"                                                      };///< Undo storage directory name.
    static constexpr std::string_view pendingFolder                  { "pending"                                                   };///< Folder of transactions that are not published.
    static constexpr std::string_view blobFolder                     { "blobs"                                                     };///< Folder of undo backup blob files.
    static constexpr std::string_view recordSuffix                   { ".json"                                                     };///< Undo transaction record file suffix.
    static constexpr std::string_view blobSuffix                     { ".blob"                                                     };///< Undo backup blob file suffix.
    static constexpr std::string_view find                           { "find"                                                      };///< Find command verb.
    static constexpr std::string_view replace                        { "replace"                                                   };///< Replace command verb.
    static constexpr std::string_view lines                          { "lines"                                                     };///< Lines command verb.
    static constexpr std::string_view fields                         { "fields"                                                    };///< Fields command verb.
    static constexpr std::string_view patch                          { "patch"                                                     };///< Patch command verb.
    static constexpr std::string_view undo                           { "undo"                                                      };///< Undo command verb.
    static constexpr std::string_view write                          { "write"                                                     };///< Write command verb.
    static constexpr std::string_view json                           { "json"                                                      };///< Json command verb.
    static constexpr std::string_view jsonNullInputOption            { "-n|--null-input"                                           };///< Use null as the single input value.
    static constexpr std::string_view jsonRawInputOption             { "-R|--raw-input"                                            };///< Read each line as a string.
    static constexpr std::string_view jsonSlurpOption                { "-s|--slurp"                                                };///< Read all inputs into one array.
    static constexpr std::string_view jsonCompactOption              { "-c|--compact-output"                                       };///< Compact instead of pretty-printed output.
    static constexpr std::string_view jsonRawOutputOption            { "-r|--raw-output"                                           };///< Output strings without quotes.
    static constexpr std::string_view jsonRawOutput0Option           { "--raw-output0"                                             };///< Raw output with a NUL after each output.
    static constexpr std::string_view jsonJoinOutputOption           { "-j|--join-output"                                          };///< Raw output without newlines.
    static constexpr std::string_view jsonAsciiOption                { "-a|--ascii-output"                                         };///< Output only ASCII, with escapes.
    static constexpr std::string_view jsonSortKeysOption             { "-S|--sort-keys"                                            };///< Sort the keys of each object on output.
    static constexpr std::string_view jsonColourOption               { "-C|--color-output"                                         };///< Colour the output.
    static constexpr std::string_view jsonMonochromeOption           { "-M|--monochrome-output"                                    };///< Disable colour.
    static constexpr std::string_view jsonTabOption                  { "--tab"                                                     };///< Indent with tabs.
    static constexpr std::string_view jsonIndentOption               { "--indent"                                                  };///< Indent with n spaces, at most 7.
    static constexpr std::string_view jsonUnbufferedOption           { "--unbuffered"                                              };///< Write each output as it is made.
    static constexpr std::string_view jsonStreamOption               { "--stream"                                                  };///< Run the filter over the stream events of each input.
    static constexpr std::string_view jsonStreamErrorsOption         { "--stream-errors"                                           };///< Implies --stream.
    static constexpr std::string_view jsonSeqOption                  { "--seq"                                                     };///< Read and write application/json-seq.
    static constexpr std::string_view jsonFromFileOption             { "-f|--from-file"                                            };///< Load the filter from a file.
    static constexpr std::string_view jsonLibraryPathOption          { "-L|--library-path"                                         };///< Search modules in a directory.
    static constexpr std::string_view jsonArgNamedOption             { "--arg"                                                     };///< Set a variable to a string value.
    static constexpr std::string_view jsonArgJsonOption              { "--argjson"                                                 };///< Set a variable to a JSON value.
    static constexpr std::string_view jsonSlurpFileOption            { "--slurpfile"                                               };///< Set a variable to the values of a file.
    static constexpr std::string_view jsonRawFileOption              { "--rawfile"                                                 };///< Set a variable to the text of a file.
    static constexpr std::string_view jsonArgsOption                 { "--args"                                                    };///< Remaining arguments are string positionals.
    static constexpr std::string_view jsonJsonArgsOption             { "--jsonargs"                                                };///< Remaining arguments are JSON positionals.
    static constexpr std::string_view jsonExitStatusOption           { "-e|--exit-status"                                          };///< Set the exit code from the last output.
    static constexpr std::string_view jsonBinaryOption               { "-b|--binary"                                               };///< Accepted for jq parity; no effect.
    static constexpr std::string_view jsonBuildConfigurationOption   { "--build-configuration"                                     };///< Show the build configuration.
    static constexpr std::string_view jsonRunTestsOption             { "--run-tests"                                               };///< Run the tests of a jq test file.
    static constexpr std::string_view jsonSkipOption                 { "--skip"                                                    };///< Skip the first n tests.
    static constexpr std::string_view jsonTakeOption                 { "--take"                                                    };///< Run n tests after the skipped ones.
    static constexpr std::string_view countLinesOption               { "--lines|-l"                                                };///< Print the line count.
    static constexpr std::string_view countWordsOption               { "--words|-w"                                                };///< Print the word count.
    static constexpr std::string_view countBytesOption               { "--bytes|-c"                                                };///< Print the byte count.
    static constexpr std::string_view sortNumericOption              { "--numeric-sort|-n"                                         };///< Compare by numeric value.
    static constexpr std::string_view sortReverseOption              { "--reverse|-r"                                              };///< Reverse the order.
    static constexpr std::string_view sortUniqueOption               { "--unique|-u"                                               };///< Print one line of each run of equal lines.
    static constexpr std::string_view sortCountOption                { "--count|-c"                                                };///< Prefix each distinct line with its count, as uniq -c.
    static constexpr std::string_view jsonPositionalKey              { "positional"                                                };///< Key of the positional arguments in $ARGS.
    static constexpr std::string_view jsonNamedKey                   { "named"                                                     };///< Key of the named arguments in $ARGS.
    static constexpr std::string_view unicodeNotation                { "unicode"                                                   };///< Unicode control pictures for non-printable bytes.
    static constexpr std::string_view caretNotation                  { "caret"                                                     };///< Caret notation for non-printable bytes.
    static constexpr std::string_view asTextBinary                   { "as-text"                                                   };///< Print a binary file as text.
    static constexpr std::string_view noPrintingBinary               { "no-printing"                                               };///< Do not print the bytes of a binary file.
    static constexpr std::string_view excerpt                        { "excerpt"                                                   };///< Excerpt command verb.
    static constexpr std::string_view count                          { "count"                                                     };///< Count command verb.
    static constexpr std::string_view sort                           { "sort"                                                      };///< Sort command verb.
    static constexpr std::string_view diff                           { "diff"                                                      };///< Diff command verb.
    static constexpr std::string_view startCol                       { "startCol"                                                  };///< Match start column key.
    static constexpr std::string_view endCol                         { "endCol"                                                    };///< Match end column key.
    static constexpr std::string_view startLine                      { "startLine"                                                 };///< Operation start line key.
    static constexpr std::string_view lastLine                       { "endLine"                                                   };///< Operation end line key.
    static constexpr std::string_view deletion                       { "delete"                                                    };///< Patch delete operation key.
    static constexpr std::string_view insertBefore                   { "insertBefore"                                              };///< Patch insert-before operation key.
    static constexpr std::string_view insertAfter                    { "insertAfter"                                               };///< Patch insert-after operation key.
    static constexpr std::string_view hash                           { "hash"                                                      };///< Line or file hash key.
    static constexpr std::string_view timestamp                      { "timestamp"                                                 };///< Undo transaction timestamp key.
    static constexpr std::string_view transactionCommand             { "command"                                                   };///< Undo transaction command key.
    static constexpr std::string_view files                          { "files"                                                     };///< Undo transaction files array key.
    static constexpr std::string_view existing                       { "existing"                                                  };///< True when the file existed before the transaction.
    static constexpr std::string_view folders                        { "folders"                                                   };///< Folders that the transaction created, from the top down.
    static constexpr std::string_view preHash                        { "preHash"                                                   };///< File hash before apply.
    static constexpr std::string_view postHash                       { "postHash"                                                  };///< File hash after apply.
    static constexpr std::string_view executable                     { "executable"                                                };///< Executable bit flag key.
    static constexpr std::string_view run                            { "run"                                                       };///< Name of the MCP tool that runs a saw command line.
    static constexpr std::string_view status                         { "status"                                                    };///< MCP result key for the status of the command.
    static constexpr std::string_view output                         { "output"                                                    };///< MCP result key for the standard output text.
    static constexpr std::string_view outputBase64                   { "outputBase64"                                              };///< MCP result key for the standard output in base64.
    static constexpr std::string_view errors                         { "errors"                                                    };///< MCP result key for the standard error text.
    static constexpr std::string_view errorsBase64                   { "errorsBase64"                                              };///< MCP result key for the standard error in base64.
    static constexpr std::string_view standardInput                  { "stdin"                                                     };///< MCP request key for the standard input text.
    static constexpr std::string_view terminalVariable               { "TERM"                                                      };///< Terminal type environment variable name.
    static constexpr std::string_view colourTermVariable             { "COLORTERM"                                                 };///< Colour capability environment variable name.
    static constexpr std::string_view jsonTostreamFilter             { "tostream"                                                  };///< Filter program used for streaming JSON input.
    static constexpr std::string_view jsonEnvironmentVariable        { "ENV"                                                       };///< jq variable holding the process environment.
    static constexpr std::string_view jsonBuildConfigurationVariable { "JQ_BUILD_CONFIGURATION"                                    };///< jq variable holding the build configuration text.
    static constexpr std::string_view jsonSearchListVariable         { "JQ_SEARCH_LIST"                                            };///< jq variable holding the module search directories.
    static constexpr std::string_view jsonProgOriginVariable         { "JQ_PROG_ORIGIN"                                            };///< jq variable holding the directory of the program file.
    static constexpr std::string_view jsonJqOriginVariable           { "JQ_JQ_ORIGIN"                                              };///< jq variable holding the directory of the executable.
    static constexpr std::string_view jsonArgumentsVariable          { "ARGS"                                                      };///< jq variable holding named and positional arguments.
    static constexpr std::string_view noColourVariable               { "NO_COLOR"                                                  };///< Environment variable disabling coloured output.
    static constexpr std::string_view jqColoursVariable              { "JQ_COLORS"                                                 };///< Environment variable overriding JSON output colours.
    static constexpr std::string_view jsonDefaultColours             { "0;90:0;39:0;39:0;39:0;32:1;39:1;39:1;34"                   };///< Default colon-separated ANSI colour codes for JSON output.
    static constexpr std::string_view jsonPackageFolder              { "jq"                                                        };///< Package subfolder name for module resolution.
    static constexpr std::string_view jsonPackageMain                { "main"                                                      };///< Package entry file stem for module resolution.
    static constexpr std::string_view writeEntryName                 { "ling_write_page"                                           };///< Page library entry that writes the rendered page.
    static constexpr std::string_view runEntryName                   { "ling_run_page"                                             };///< Page library entry that runs the pager.
    static constexpr std::string_view objectFormatSection            { "extensions"                                                };///< Git config section that names the object format.
    static constexpr std::string_view objectFormatKey                { "objectformat"                                              };///< Git config key that names the object format.
    static constexpr std::string_view sha256Format                   { "sha256"                                                    };///< Git object format name of SHA-256 identifiers.
    static constexpr std::string_view plainTextTitle                 { "Plain Text"                                                };///< Language title of the plain text fallback.
    static constexpr std::string_view batFolderName                  { "bat"                                                       };///< Folder name of the bat configuration.
    static constexpr std::string_view homeConfigFolderName           { ".config"                                                   };///< Home folder that holds the configuration folders.
    static constexpr std::string_view batConfigDirectoryVariable     { "BAT_CONFIG_DIR"                                            };///< Environment variable naming the configuration folder.
    static constexpr std::string_view batConfigPathVariable          { "BAT_CONFIG_PATH"                                           };///< Environment variable naming the configuration file.
    static constexpr std::string_view batOptionsVariable             { "BAT_OPTS"                                                  };///< Environment variable holding extra page options.
    static constexpr std::string_view styleSheetWildcard             { "*"                                                         };///< Wildcard that matches every style sheet file.
    static constexpr std::string_view batThemeVariable               { "BAT_THEME"                                                 };///< Environment variable naming the theme.
    static constexpr std::string_view batThemeLightVariable          { "BAT_THEME_LIGHT"                                           };///< Environment variable naming the light theme.
    static constexpr std::string_view batThemeDarkVariable           { "BAT_THEME_DARK"                                            };///< Environment variable naming the dark theme.
    static constexpr std::string_view batStyleVariable               { "BAT_STYLE"                                                 };///< Environment variable naming the style.
    static constexpr std::string_view batPagerVariable               { "BAT_PAGER"                                                 };///< Environment variable naming the pager.
    static constexpr std::string_view batPagingVariable              { "BAT_PAGING"                                                };///< Environment variable naming the paging mode.
    static constexpr std::string_view batWidthVariable               { "BAT_WIDTH"                                                 };///< Environment variable naming the terminal width.
    static constexpr std::string_view spaceChunk                     { "                "                                          };///< Run of spaces that pads the output.
    static constexpr std::string_view wordBreakCharacters            { " \t"                                                       };///< Characters that separate the words of BAT_OPTS.
    static constexpr std::string_view noGridStyleSheet               { ".metrics { tableLineThickness: 0em; ruleThickness: 0em; }" };///< Style sheet that hides the grid lines of a page.
    static constexpr std::string_view themeFolderName                { "themes"                                                    };///< Folder name of the bat themes.

/**______________________________END OF NAMESPACE______________________________*/
}// namespace Id
