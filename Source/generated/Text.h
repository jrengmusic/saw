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

/** @brief saw message texts — errors, summaries, undo listing, and diff format strings. */
struct Diagnostics
{
    static constexpr const char* const failCommandUnknown           { "unknown command: %s"                                                                                };///< CLI received an unrecognized verb.
    static constexpr const char* const failFlagUnknown              { "unknown flag: %s"                                                                                   };///< CLI received an unrecognized option flag.
    static constexpr const char* const failFlagValueMissing         { "flag requires a value: %s"                                                                          };///< Value-taking flag appeared with no argument.
    static constexpr const char* const failUnexpectedArgument       { "unexpected argument: %s"                                                                            };///< Command received an extra positional argument.
    static constexpr const char* const failMissingPath              { "missing path argument"                                                                              };///< File path argument was omitted.
    static constexpr const char* const failNotFound                 { "not found: %s"                                                                                      };///< Target file or directory does not exist.
    static constexpr const char* const failBinaryFile               { "binary file: %s"                                                                                    };///< Target file contains binary bytes.
    static constexpr const char* const failStandardInput            { "cannot read stdin"                                                                                  };///< Standard input stream could not be read.
    static constexpr const char* const failIntegerInvalid           { "invalid integer for %s: %s"                                                                         };///< Flag value is not a valid integer.
    static constexpr const char* const failFindRequiresPattern      { "find requires <pattern>"                                                                            };///< Find verb invoked without a pattern argument.
    static constexpr const char* const failReplaceRequiresArguments { "replace requires <find> and <with>"                                                                 };///< Replace verb invoked without required arguments.
    static constexpr const char* const failWithSourcesConflict      { "specify at most one of --with-stdin, --with-file"                                                   };///< Mutually exclusive replacement input flags supplied together.
    static constexpr const char* const failLinesRequiresRange       { "lines requires <range>"                                                                             };///< Lines verb invoked without a line range argument.
    static constexpr const char* const failFieldsRequiresSpec       { "fields requires <spec>"                                                                             };///< Fields verb invoked without a field spec argument.
    static constexpr const char* const failPatternEmpty             { "pattern cannot be empty"                                                                            };///< Search or match pattern is an empty string.
    static constexpr const char* const failRegexInvalid             { "invalid regex: %s"                                                                                  };///< Regular expression failed to compile.
    static constexpr const char* const failApplyRequiresExpect      { "--apply requires --expect <N>"                                                                      };///< Apply flag supplied without expected edit count.
    static constexpr const char* const failApplyStandardInput       { "cannot --apply on stdin; specify file paths"                                                        };///< Apply flag combined with standard input target.
    static constexpr const char* const failExpectMismatch           { "expected %d edit(s), found %d; nothing written"                                                     };///< Planned edit count does not match --expect value.
    static constexpr const char* const failCaptureInvalid           { "invalid capture group $%d in replacement"                                                           };///< Replacement references $k where k > capture group count.
    static constexpr const char* const failWrite                    { "write failed for %s: %s; rolled back"                                                               };///< File write failed; previous state restored.
    static constexpr const char* const failOverwriteFailed          { "overwrite failed"                                                                                   };///< Temporary file failed to overwrite target file.
    static constexpr const char* const failLineRangeInvalid         { "invalid line range: %s"                                                                             };///< Line range string does not parse.
    static constexpr const char* const failLineRangeBounds          { "line range %s out of bounds for %s (%d lines)"                                                      };///< Line range exceeds file line count.
    static constexpr const char* const failInsertSingleLine         { "insert requires a single line number, got %s"                                                       };///< Insert operation given a range instead of a single line.
    static constexpr const char* const failMutateFlagsConflict      { "specify at most one of --delete, --replace, --insert-before, --insert-after"                        };///< Mutually exclusive line edit flags supplied together.
    static constexpr const char* const failHashMismatch             { "hash mismatch at %s:%d: expected %s, got %s"                                                        };///< Observed line hash does not match --verify-hash value.
    static constexpr const char* const failFieldSpecInvalid         { "invalid field spec: %s"                                                                             };///< Field spec string does not parse.
    static constexpr const char* const failFieldSpecZero            { "invalid field spec: %s (1-based or negative)"                                                       };///< Field index zero given instead of 1-based or negative index.
    static constexpr const char* const failDelimiterEmpty           { "delimiter cannot be empty"                                                                          };///< Field delimiter string is empty.
    static constexpr const char* const failFieldBounds              { "field %d out of range at %s:%d"                                                                     };///< Requested field index exceeds line field count under --strict.
    static constexpr const char* const failPatchRequiresInput       { "patch requires stdin or --file <path>"                                                              };///< Patch verb invoked with neither stdin nor --file input.
    static constexpr const char* const failPatchJsonInvalid         { "invalid patch JSON: %s"                                                                             };///< Patch payload failed JSON parsing.
    static constexpr const char* const failPatchRootNotArray        { "root must be an array"                                                                              };///< Patch JSON root value is not an array.
    static constexpr const char* const failPatchEntryInvalid        { "invalid patch entry %d: %s"                                                                         };///< Patch entry at index failed validation.
    static constexpr const char* const failPatchEntryNotObject      { "entry must be an object"                                                                            };///< Patch array element is not a JSON object.
    static constexpr const char* const failPatchEntryShape          { "unrecognized or conflicting operation fields"                                                       };///< Patch entry lacks a single valid operation key.
    static constexpr const char* const failPatchEntryFields         { "missing or invalid required fields"                                                                 };///< Patch entry lacks required path, line, or text fields.
    static constexpr const char* const failPatchConflict            { "conflicting edits in %s at line %d"                                                                 };///< Multiple patch entries target the same file and line.
    static constexpr const char* const failPatchBounds              { "out of bounds in %s at entry %d"                                                                    };///< Patch entry line range exceeds file bounds.
    static constexpr const char* const failUndoEmpty                { "no undo transactions available"                                                                     };///< Undo directory holds no recorded transactions.
    static constexpr const char* const failUndoUnknown              { "unknown transaction: %s"                                                                            };///< Requested undo transaction id does not exist.
    static constexpr const char* const failUndoCorrupt              { "corrupt transaction %s: %s"                                                                         };///< Undo transaction metadata or blob is unreadable or malformed.
    static constexpr const char* const failUndoDrift                { "cannot undo %s: %s changed since apply (pass --force to overwrite)"                                 };///< Target file content changed after the transaction applied.
    static constexpr const char* const failUndoMissing              { "cannot undo %s: %s missing since apply (pass --force to overwrite)"                                 };///< Target file is missing after the transaction applied.
    static constexpr const char* const failUndoWrite                { "undo failed for %s: %s; rolled back"                                                                };///< Undo restore write failed; previous state restored.
    static constexpr const char* const failChecksumMismatch         { "checksum mismatch"                                                                                  };///< Written file hash does not match recorded pre-apply hash.
    static constexpr const char* const summaryDryRun                { "dry-run \xe2\x80\x94 %d edit(s) in %d file(s); nothing written. Pass --apply --expect %d to write." };///< Summary emitted for non-empty dry-run edit plan.
    static constexpr const char* const summaryDryRunZero            { "dry-run \xe2\x80\x94 0 edits; nothing written."                                                     };///< Summary emitted when dry-run produces zero edits.
    static constexpr const char* const summaryApplied               { "applied %d edit(s) in %d file(s) (tx: %s)"                                                          };///< Summary emitted after edits are written and recorded.
    static constexpr const char* const summaryAppliedZero           { "applied 0 edit(s) in 0 file(s)"                                                                     };///< Summary emitted when apply runs with zero edits.
    static constexpr const char* const summaryNoContentChanged      { "no content changed; nothing written"                                                                };///< Summary emitted when planned edits produce identical file content.
    static constexpr const char* const summaryDryRunUndo            { "dry-run undo (%s) \xe2\x80\x94 %d file(s); nothing written. Pass --apply to restore."               };///< Summary emitted for dry-run undo preview.
    static constexpr const char* const summaryUndone                { "undone %s \xe2\x80\x94 restored %d file(s)"                                                         };///< Summary emitted after undo restores recorded files.
    static constexpr const char* const undoListFormat               { "%s\t%s\t%d file(s)\t%s"                                                                             };///< Tab-delimited format string for undo --list rows.
    static constexpr const char* const diffOldHeaderFormat          { "--- %s\n"                                                                                           };///< Unified diff old-file header format string.
    static constexpr const char* const diffNewHeaderFormat          { "+++ %s (planned)\n"                                                                                 };///< Unified diff planned new-file header format string.
    static constexpr const char* const diffNoNewline                { "\\ No newline at end of file\n"                                                                     };///< Unified diff marker for missing trailing newline.
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace text
