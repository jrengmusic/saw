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
 * @file Text.h
 * @brief saw message strings — diagnostics, summaries, and diff headers.
 */

#pragma once

namespace text
{
/*_____________________________________________________________________________*/

/** @brief saw message texts — errors, summaries, undo messages, and diff marker. */
struct Diagnostics
{
    static constexpr const char* const failCommandUnknown           { "unknown command: "                                                           };///< CLI received an unrecognized verb.
    static constexpr const char* const failFlagUnknown              { "unknown flag: "                                                              };///< CLI received an unrecognized option flag.
    static constexpr const char* const failFlagValueMissing         { "flag requires a value: "                                                     };///< Value-taking flag appeared with no argument.
    static constexpr const char* const failUnexpectedArgument       { "unexpected argument: "                                                       };///< Command received an extra positional argument.
    static constexpr const char* const failMissingPath              { "missing path argument"                                                       };///< File path argument was omitted.
    static constexpr const char* const failNotFound                 { "not found: "                                                                 };///< Target file or directory does not exist.
    static constexpr const char* const failRead                     { "cannot read "                                                                };///< Target file or directory exists but cannot be read.
    static constexpr const char* const failBinaryFile               { "binary file: "                                                               };///< Target file contains binary bytes.
    static constexpr const char* const entriesNotSearched           { " ignored or hidden entries not searched (pass --no-ignore or --hidden)"      };///< Suffix that names the entries a walk skipped.
    static constexpr const char* const failHardLinkedFile           { "hard-linked file: "                                                          };///< Target file has more than one hard link.
    static constexpr const char* const failReadOnlyFile             { "read-only file: "                                                            };///< Target file cannot be written.
    static constexpr const char* const failStandardInput            { "cannot read stdin"                                                           };///< Standard input stream could not be read.
    static constexpr const char* const failStandardOutput           { "cannot write stdout"                                                         };///< Standard output stream could not be written.
    static constexpr const char* const failIntegerInvalid           { "invalid integer for "                                                        };///< Flag value is not a valid integer.
    static constexpr const char* const failPathQuote                { "'"                                                                           };///< Opens and closes the path of a directory failure.
    static constexpr const char* const failIsDirectory              { "' is a directory."                                                           };///< Closes the path of a folder named by a page.
    static constexpr const char* const failChoiceInvalid            { "invalid value for "                                                          };///< Flag value is not one of the allowed values.
    static constexpr const char* const failThemeUnknown             { "unknown theme: "                                                             };///< Theme name that no style sheet has.
    static constexpr const char* const failFindRequiresPattern      { "find requires <pattern>"                                                     };///< Find verb invoked without a pattern argument.
    static constexpr const char* const failReplaceRequiresArguments { "replace requires <find> and <with>"                                          };///< Replace verb invoked without required arguments.
    static constexpr const char* const failWithSourcesConflict      { "specify at most one of --with-stdin, --with-file"                            };///< Mutually exclusive replacement input flags supplied together.
    static constexpr const char* const failLinesRequiresRange       { "lines requires <range>"                                                      };///< Lines verb invoked without a line range argument.
    static constexpr const char* const failFieldsRequiresSpec       { "fields requires <spec>"                                                      };///< Fields verb invoked without a field spec argument.
    static constexpr const char* const failExcerptRequiresTarget    { "excerpt requires <target>"                                                   };///< Excerpt verb invoked without a target and without stdin lines.
    static constexpr const char* const excerptEscaped               { " escaped"                                                                    };///< Block header suffix for a block with an invalid UTF-8 line.
    static constexpr const char* const excerptNotFound              { "NOT FOUND"                                                                   };///< Excerpt target file does not exist.
    static constexpr const char* const excerptUnreadable            { "UNREADABLE"                                                                  };///< Excerpt target is a folder or cannot be read.
    static constexpr const char* const excerptBinary                { "BINARY"                                                                      };///< Excerpt target holds a NUL byte.
    static constexpr const char* const excerptEmpty                 { "EMPTY"                                                                       };///< Excerpt target is a 0-byte file.
    static constexpr const char* const excerptNoMatch               { "NO MATCH"                                                                    };///< Excerpt literal target matches no line.
    static constexpr const char* const excerptOutOfRange            { "OUT OF RANGE"                                                                };///< Excerpt target line range is invalid or starts past the last line.
    static constexpr const char* const failPatternEmpty             { "pattern cannot be empty"                                                     };///< Search or match pattern is an empty string.
    static constexpr const char* const failPatternNewline           { "pattern cannot contain a newline"                                            };///< Search pattern holds a line ending.
    static constexpr const char* const failRegexInvalid             { "invalid regex: "                                                             };///< Regular expression failed to compile.
    static constexpr const char* const failRegexSteps               { "regex exceeded its step limit: "                                             };///< Regular expression matching stopped at the step limit.
    static constexpr const char* const failApplyRequiresExpect      { "--apply requires --expect <N>"                                               };///< Apply flag supplied without expected edit count.
    static constexpr const char* const failJsonFilterRequired       { "json requires <filter>"                                                      };///< Json verb invoked without a filter.
    static constexpr const char* const failJsonIndentRange          { "invalid value for --indent: maximum is "                                     };///< Indent value is above the largest accepted indentation.
    static constexpr const char* const failJsonParse                { "parse error: "                                                               };///< Input or argument text is not one valid JSON value.
    static constexpr const char* const jsonRecordSkipped            { "skipped JSON text: "                                                         };///< A --seq record that is not one valid JSON value.
    static constexpr const char* const jsonRecordTruncated          { "skipped truncated JSON text at line "                                        };///< A --seq record ends in a number, boolean or null with no whitespace.
    static constexpr const char* const jsonRecordOutside            { "skipped text before the first record separator at line "                     };///< Bytes of --seq input before the first record separator.
    static constexpr const char* const failJsonArgumentJson         { "invalid JSON text passed to --argjson"                                       };///< Argument text of --argjson holds no single JSON value.
    static constexpr const char* const failJsonArgumentsJson        { "invalid JSON text passed to --jsonargs"                                      };///< Positional text of --jsonargs holds no single JSON value.
    static constexpr const char* const failJsonSlurpBad             { "Bad JSON in --slurpfile "                                                    };///< Opens the failure of a --slurpfile file that holds bad JSON.
    static constexpr const char* const slurpNameSeparator           { " "                                                                           };///< Connector between the name and the path of a --slurpfile failure.
    static constexpr const char* const slurpPathSeparator           { ": "                                                                          };///< Connector between the path and the message of a --slurpfile failure.
    static constexpr const char* const failJsonErrorPrefix          { "error (at "                                                                  };///< Start of the runtime error text.
    static constexpr const char* const failJsonErrorSeparator       { "): "                                                                         };///< Separator before the message of a string error.
    static constexpr const char* const failJsonNotString            { ") (not a string): "                                                          };///< Separator before the JSON text of a non-string error.
    static constexpr const char* const failJsonUnknownLocation      { "<unknown>"                                                                   };///< Location of a runtime error before any input is read.
    static constexpr const char* const failJsonStandardInputName    { "<stdin>"                                                                     };///< Location name of the standard input.
    static constexpr const char* const jsonVersion                  { "jq-1.8.2"                                                                    };///< Version line of the json verb.
    static constexpr const char* const jsonBuildConfiguration       { "JAM_USING_JSON_FILTER=1"                                                     };///< Build configuration line of the json verb.
    static constexpr const char* const jsonDebugLabel               { "DEBUG:"                                                                      };///< Label of the debug line on standard error.
    static constexpr const char* const jsonTestHeader               { "Test #"                                                                      };///< Start of the header of one test.
    static constexpr const char* const jsonTestProgramOpen          { ": '"                                                                         };///< Opens the program text of a test header.
    static constexpr const char* const jsonTestProgramClose         { "' at line number "                                                           };///< Closes the program text of a test header.
    static constexpr const char* const jsonTestProgramSeparator     { ": "                                                                          };///< Separator before the program of a test message.
    static constexpr const char* const jsonTestOf                   { "of"                                                                          };///< Separates the passed and the run counts of the summary.
    static constexpr const char* const jsonTestSummary              { " tests passed ("                                                             };///< Opens the malformed count of the summary.
    static constexpr const char* const jsonTestMalformed            { "malformed,"                                                                  };///< Separates the malformed and the skipped counts of the summary.
    static constexpr const char* const jsonTestSkipped              { " skipped)"                                                                   };///< Closes the summary.
    static constexpr const char* const jsonTestErroneous            { "*** Erroneous program failed with extra message '"                           };///< Start of a message of a program that fails to compile.
    static constexpr const char* const jsonTestErroneousEnd         { "' at line "                                                                  };///< Closes the message text, before the line number.
    static constexpr const char* const jsonTestInputInvalid         { "*** Input is invalid on line "                                               };///< Start of a message of an input that is not one value.
    static constexpr const char* const jsonTestExpectedInvalid      { "*** Expected result is invalid on line "                                     };///< Start of a message of an expected result that is not one value.
    static constexpr const char* const jsonTestInsufficient         { "*** Insufficient results for test at line number "                           };///< Start of a message of a test with fewer results than expected.
    static constexpr const char* const jsonTestWrong                { "*** Wrong result for test at line number "                                   };///< Start of a message of a test with a different result.
    static constexpr const char* const jsonTestExtra                { "*** Extra results for test at line number "                                  };///< Start of a message of a test with more results than expected.
    static constexpr const char* const jsonTestUnexpectedCompile    { "*** Program compiled, but a compile error was expected at line "             };///< Start of a message of a fail test whose program compiled.
    static constexpr const char* const jsonTestFailMarker           { "%%FAIL"                                                                      };///< Marker that starts a block that expects a compile error.
    static constexpr const char* const failApplyStandardInput       { "cannot --apply on stdin; specify file paths"                                 };///< Apply flag combined with standard input target.
    static constexpr const char* const failExpectMismatch           { "expected "                                                                   };///< Planned edit count does not match --expect value.
    static constexpr const char* const foundEdits                   { " edit(s), found "                                                            };///< Connector before the number of edits found.
    static constexpr const char* const nothingWritten               { "; nothing written"                                                           };///< Suffix that says nothing was written.
    static constexpr const char* const failCaptureInvalid           { "invalid capture group $"                                                     };///< Replacement references $k where k > capture group count.
    static constexpr const char* const inReplacement                { " in replacement"                                                             };///< Suffix that names the replacement text.
    static constexpr const char* const failWrite                    { "write failed for "                                                           };///< Opens the failure of a file write.
    static constexpr const char* const rolledBack                   { "; rolled back"                                                               };///< Suffix that says the previous state was restored.
    static constexpr const char* const failOverwriteFailed          { "overwrite failed"                                                            };///< Temporary file failed to overwrite target file.
    static constexpr const char* const rollbackFailed               { "; rollback failed for "                                                      };///< Connector before a file that could not be restored.
    static constexpr const char* const backupKept                   { "; backup kept in "                                                           };///< Connector before the folder that keeps the backup.
    static constexpr const char* const failLineRangeInvalid         { "invalid line range: "                                                        };///< Line range string does not parse.
    static constexpr const char* const failLineRangeBounds          { "line range "                                                                 };///< Line range exceeds file line count.
    static constexpr const char* const outOfBoundsFor               { " out of bounds for "                                                         };///< Connector between a line range and its file.
    static constexpr const char* const linesOpen                    { " ("                                                                          };///< Opens the line count of a file.
    static constexpr const char* const linesClose                   { " lines)"                                                                     };///< Closes the line count of a file.
    static constexpr const char* const failInsertSingleLine         { "insert requires a single line number, got "                                  };///< Insert operation given a range instead of a single line.
    static constexpr const char* const failMutateFlagsConflict      { "specify at most one of --delete, --replace, --insert-before, --insert-after" };///< Mutually exclusive line edit flags supplied together.
    static constexpr const char* const failHashMismatch             { "hash mismatch at "                                                           };///< Observed line hash does not match --verify-hash value.
    static constexpr const char* const hashExpected                 { ": expected "                                                                 };///< Connector before the expected hash.
    static constexpr const char* const hashFound                    { ", got "                                                                      };///< Connector before the observed hash.
    static constexpr const char* const failFieldSpecInvalid         { "invalid field spec: "                                                        };///< Field spec string does not parse.
    static constexpr const char* const fieldSpecZero                { " (1-based or negative)"                                                      };///< Suffix for a field index of zero.
    static constexpr const char* const failDelimiterEmpty           { "delimiter cannot be empty"                                                   };///< Field delimiter string is empty.
    static constexpr const char* const failFieldBounds              { "field "                                                                      };///< Requested field index exceeds line field count under --strict.
    static constexpr const char* const outOfRangeAt                 { " out of range at "                                                           };///< Connector before the position of the field.
    static constexpr const char* const failPatchRequiresInput       { "patch requires stdin or --file <path>"                                       };///< Patch verb invoked with neither stdin nor --file input.
    static constexpr const char* const failWriteRequiresInput       { "write requires stdin"                                                        };///< Write verb invoked with a terminal on standard input.
    static constexpr const char* const failPatchJsonInvalid         { "invalid patch JSON: "                                                        };///< Patch payload failed JSON parsing.
    static constexpr const char* const failPatchRootNotArray        { "root must be an array"                                                       };///< Patch JSON root value is not an array.
    static constexpr const char* const failPatchEntryInvalid        { "invalid patch entry "                                                        };///< Patch entry at index failed validation.
    static constexpr const char* const failPatchEntryNotObject      { "entry must be an object"                                                     };///< Patch array element is not a JSON object.
    static constexpr const char* const failPatchEntryShape          { "unrecognized or conflicting operation fields"                                };///< Patch entry lacks a single valid operation key.
    static constexpr const char* const failPatchEntryFields         { "missing or invalid required fields"                                          };///< Patch entry lacks required path, line, or text fields.
    static constexpr const char* const failPatchConflict            { "conflicting edits in "                                                       };///< Multiple patch entries target the same file and line.
    static constexpr const char* const atLine                       { " at line "                                                                   };///< Connector before the line number.
    static constexpr const char* const failPatchBounds              { "out of bounds in "                                                           };///< Patch entry line range exceeds file bounds.
    static constexpr const char* const atEntry                      { " at entry "                                                                  };///< Connector before the entry index.
    static constexpr const char* const failUndoRequiresId           { "undo requires <tx-id>"                                                       };///< Undo invoked without a transaction id.
    static constexpr const char* const failUndoUnknown              { "unknown transaction: "                                                       };///< Requested undo transaction id does not exist.
    static constexpr const char* const failUndoPending              { "transaction "                                                                };///< Requested transaction is not published, so another process can own it.
    static constexpr const char* const undoPending                  { " is pending (pass --force to restore it)"                                    };///< Suffix that explains the pending transaction.
    static constexpr const char* const failUndoCorrupt              { "corrupt transaction "                                                        };///< Undo transaction metadata or blob is unreadable or malformed.
    static constexpr const char* const failUndo                     { "cannot undo "                                                                };///< Opens the failure of an undo for a file.
    static constexpr const char* const undoDrift                    { " changed since apply (pass --force to overwrite)"                            };///< Suffix for a file whose content changed after the transaction applied.
    static constexpr const char* const undoMissing                  { " missing since apply (pass --force to overwrite)"                            };///< Suffix for a file that is missing after the transaction applied.
    static constexpr const char* const failUndoWrite                { "undo failed for "                                                            };///< Opens the failure of an undo write.
    static constexpr const char* const failChecksumMismatch         { "checksum mismatch"                                                           };///< Written file hash does not match recorded pre-apply hash.
    static constexpr const char* const summaryDryRun                { "dry-run \xe2\x80\x94 "                                                       };///< Opens the summary for a non-empty dry-run edit plan.
    static constexpr const char* const editsIn                      { " edit(s) in "                                                                };///< Connector between the edit count and the file count.
    static constexpr const char* const expectToWrite                { " file(s); nothing written. Pass --apply --expect "                           };///< Suffix of a dry-run summary, before the expected edit count.
    static constexpr const char* const toWrite                      { " to write."                                                                  };///< Closes a dry-run summary.
    static constexpr const char* const summaryDryRunZero            { "dry-run \xe2\x80\x94 0 edits; nothing written."                              };///< Summary emitted when dry-run produces zero edits.
    static constexpr const char* const summaryApplied               { "applied "                                                                    };///< Opens the summary emitted after edits are written and recorded.
    static constexpr const char* const transactionOpen              { " file(s) (tx: "                                                              };///< Connector before the transaction id.
    static constexpr const char* const summaryAppliedZero           { "applied 0 edit(s) in 0 file(s)"                                              };///< Summary emitted when apply runs with zero edits.
    static constexpr const char* const summaryNoContentChanged      { "no content changed; nothing written"                                         };///< Summary emitted when planned edits produce identical file content.
    static constexpr const char* const summaryDryRunUndo            { "dry-run undo ("                                                              };///< Opens the summary emitted for dry-run undo preview.
    static constexpr const char* const undoFiles                    { ") \xe2\x80\x94 "                                                             };///< Connector between the transaction id and the file count.
    static constexpr const char* const applyToRestore               { " file(s); nothing written. Pass --apply to restore."                         };///< Suffix of a dry-run undo summary.
    static constexpr const char* const summaryUndone                { "undone "                                                                     };///< Opens the summary emitted after undo restores recorded files.
    static constexpr const char* const undoneFiles                  { " \xe2\x80\x94 restored "                                                     };///< Connector before the restored file count.
    static constexpr const char* const files                        { " file(s)"                                                                    };///< Unit suffix for a file count.
    static constexpr const char* const diffNoNewline                { "\\ No newline at end of file\n"                                              };///< Unified diff marker for missing trailing newline.
    static constexpr const char* const failDirectory                { "directory is not an absolute folder path"                                    };///< MCP call gave a folder that cannot be set as working folder.
};
/** @brief saw format strings — excerpt blocks and diff headers. */
struct Formats
{
    static constexpr const char* const excerptReceipt { "# "                                               };///< Opens the receipt line for excerpt.
    static constexpr const char* const excerptTargets { " targets, "                                       };///< Connector after the target count in the receipt line.
    static constexpr const char* const excerptFound   { " found, "                                         };///< Connector after the found count in the receipt line.
    static constexpr const char* const excerptMissing { " missing\n"                                       };///< Closes the receipt line with the missing count and a newline.
    static constexpr const char* const excerptHeader  { "## "                                              };///< Opens the block header for excerpt.
    static constexpr const char* const excerptOf      { " of "                                             };///< Connector between the line range and the line count.
    static constexpr const char* const excerptHash    { " sha256:"                                         };///< Connector before the block hash.
    static constexpr const char* const diffOldHeader  { "--- "                                             };///< Unified diff old-file header prefix.
    static constexpr const char* const diffNewHeader  { "+++ "                                             };///< Unified diff planned new-file header prefix.
    static constexpr const char* const diffPlanned    { " (planned)\n"                                     };///< Closes the planned new-file header with a newline.
    static constexpr const char* const diffHunkOpen   { "@@ -"                                             };///< Opens a unified diff hunk header.
    static constexpr const char* const diffHunkNew    { " +"                                               };///< Starts the planned range of a unified diff hunk header.
    static constexpr const char* const diffHunkClose  { " @@\n"                                            };///< Closes a unified diff hunk header with a newline.
    static constexpr const char* const tabFillOne     { "\xe2\x86\xb9"                                     };///< Tab fill of width 1 in show-all output.
    static constexpr const char* const tabFillTwo     { "\xe2\x94\x9c\xe2\x94\xa4"                         };///< Tab fill of width 2 in show-all output.
    static constexpr const char* const tabFillThree   { "\xe2\x94\x9c\xe2\x94\x80\xe2\x94\xa4"             };///< Tab fill of width 3 in show-all output.
    static constexpr const char* const tabFillFour    { "\xe2\x94\x9c\xe2\x94\x80\xe2\x94\x80\xe2\x94\xa4" };///< Tab fill of width 4 in show-all output.
    static constexpr const char* const spaceNotation  { "\xc2\xb7"                                         };///< Notation of a space in show-all output.
    static constexpr const char* const fileLabel      { "File"                                             };///< Name of the file property of the header.
    static constexpr const char* const emptyMark      { "   <EMPTY>"                                       };///< Mark of an empty file, after its name in the header.
    static constexpr const char* const sizeLabel      { "Size"                                             };///< Name of the size property of the header.
    static constexpr const char* const stdinLabel     { "STDIN"                                            };///< Label of standard input in the file header.
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace text
