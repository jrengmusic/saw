# PLAN: saw — Part 6: LineDiff, Writer, UndoCache, Transaction, and replace

**RFC:** none — objective from ARCHITECT prompt
**Date:** 2026-11-22
**BLESSED Compliance:** verified
**Language Constraints:** C++17 / JUCE 8.0.17 + JAM (LANGUAGE.md "C++ / JUCE")

## Overview

This part implements the diff, atomic writer, undo-cache, and transaction layers (`LineDiff`, `Writer`, `UndoCache`, `Transaction`) and the `replace` verb (`SPEC.md` §4.5, §4.7, §5.2, §6), wires `Id::replace` in `Source/Main.cpp`, and adds reference (`sed`) and baseline test cases to `test/cases.md`.

## Language / Framework Constraints

- One object, one translation unit (`CODING.md:899-937`): `LineDiff.h/.cpp`, `Writer.h/.cpp`, `UndoCache.h/.cpp`, `Transaction.h/.cpp`, `Replace.h/.cpp`.
- Single responsibility (`MANIFESTO.md` E): `Writer` owns atomic `juce::TemporaryFile` file writes (`toFile`) and POSIX execute-bit queries (`isExecutable`); `UndoCache` owns only `<app-data>/saw/undo/`; `Transaction` owns the planned batch and two-phase commit (`commit()`).
- Zero fake-carrier structs (`S8`) and zero shadow state (`MANIFESTO.md` S): `Transaction` reads display paths from `originalDocuments` (no duplicate `displayPaths` array); `UndoCache::getTransactionDocument` returns `std::unique_ptr<jam::JsonDocument>` in place (`P11`).
- Use framework API out of the box (`S9`): `jam::File::getUserApplicationDataDirectory (ProjectInfo::projectName)` (`A23`), `jam::File::getOrCreateDirectory` (`A23`), `juce::File::setExecutePermission` (`A4`), `jam::Format::appendDecimal` (`A15`).
- Exact `SPEC.md` §5.2 options on `replace` (no unlisted `--hidden` or `--no-ignore`).
- No redundant STL includes (`S6`), no comments/doxygen before audit (`CAROL.md` Code Hygiene).

## Framework Inventory

Full tables live in `PLAN-saw-0-tracker.md` §Framework Inventory. This part uses:

- **API:** A1, A2, A3, A4, A5, A6, A7, A8, A10, A11, A13, A14, A15 (`jam::Format::appendDecimal`), A16, A17, A19, A20, A23 (`jam::File::getUserApplicationDataDirectory`, `getOrCreateDirectory`), A24, A25.
- **Pattern:** P3, P5, P6, P7, P10, P11, P14, P17.
- **Names:** N1 (`Replace`), N2 (`Writer`), N5 (`LineDiff`, `LineDiff::Change`, `changes`), N6 (`run`, `get`, `add`, `remove`, `toFile`, `commit`, `is`), N9, N10 (`UndoCache`, `Transaction`), N11.
- **Synthesis:** S1, S2, S3, S4, S5, S6, S7, S8, S9, S10, S11, S12, S13, S14.
- **Absence:** X1, X7, X8 (`gmtime_s` on Windows / `gmtime_r` on POSIX), X9 (`Writer::isExecutable`).

## Validation Gate

Each step is validated by COUNSELOR before proceeding to the next — against `MANIFESTO.md` (BLESSED), `NAMES.md`, `CODING.md`, and the locked PLAN decisions. @Auditor runs ONCE, after the final step — never per step.

## Steps

### Step 1: Implement `Source/LineDiff.h` and `Source/LineDiff.cpp`
**Scope:** `~/Documents/Poems/dev/saw/Source/LineDiff.h`, `~/Documents/Poems/dev/saw/Source/LineDiff.cpp`
**Action:**
1. Create `Source/LineDiff.h` (`N5`):
   ```cpp
   #pragma once
   #include <JuceHeader.h>
   #include "TextDocument.h"

   class LineDiff
   {
   public:
       struct Change
       {
           int start { 0 };
           int length { 0 };
           jam::Strings insertedLines;
       };

       static constexpr int contextLineCount { 3 };

       explicit LineDiff (jam::Array<Change> explicitChanges);
       LineDiff (const TextDocument& original, const TextDocument& target);

       std::string getPlannedText (const TextDocument& original) const;
       std::string getUnifiedDiff (const TextDocument& original,
                                   const TextDocument& planned) const;
       static jam::Strings getLines (std::string_view text);

       jam::Array<Change> changes;

   private:
       static std::string getHunkRangeText (int zeroBasedStart, int lineCount);
   };
   ```
2. Create `Source/LineDiff.cpp`:
   - `getLines (std::string_view text)` (unambiguous trailing-newline rule for replacement/insertion text):
     - If `text.empty()`, return `jam::Strings { juce::String {} }` (one empty line, so `--replace ""` replaces with an empty line; `--delete` passes `{}` directly without calling `getLines`).
     - Strip at most one trailing `'\n'` (and one preceding `'\r'` if present) from `text`, then split on `'\n'` (stripping a trailing `'\r'` on each line using `.at()`, `S4`) into `jam::Strings`. (Thus `"a\nb\n"` → `["a", "b"]`, `"a\nb"` → `["a", "b"]`, `"\n"` → `[""]`, `"a\n\n"` → `["a", ""]`.)
   - `LineDiff (const TextDocument& original, const TextDocument& target)` (`X1`): trim common prefix/suffix lines, compute Myers shortest edit script on remaining line slices, coalesce contiguous edits into `changes` in ascending `start` order.
   - `getPlannedText (const TextDocument& original) const` (`SPEC.md` §4.5):
     - Precondition: `changes` is sorted in ascending `start` order.
     - Assemble output lines by walking 0-based lines `0..original.getNumLines() - 1` and applying each `Change`.
     - If output has 0 lines, return `{}`.
     - Trailing newline rule: let `const bool touchedFinalLine { original.getNumLines() == 0 or (not changes.isEmpty() and changes.last().start + changes.last().length >= original.getNumLines()) };`. Emit `original.getLineEnding()` after the final line when `original.hasTrailingNewline() or touchedFinalLine` (if final line was deleted from a multi-line file, the new final line was followed by a newline in the original; if lines were inserted into a 0-byte file, end with `\n`).
   - `getHunkRangeText (int zeroBasedStart, int lineCount)`: format with `jam::Format::appendDecimal` (`A15`, `S9`) per `SPEC.md` §4.7 (`0` → `"<start>,0"`, `1` → `"<start+1>"`, `N` → `"<start+1>,<N>"`).
   - `getUnifiedDiff`: GNU-exact per `SPEC.md` §4.7, merging hunks when gap `<= 2 * contextLineCount`, emitting `text::Diagnostics::diffOldHeaderFormat`, `diffNewHeaderFormat`, and `diffNoNewline`.
**Inventory:** A10, A15, A19, A20, P17, N5, N6, N9, S3, S4, S6, S9, S10, S14, X1.
**Validation:** COUNSELOR reads `Source/LineDiff.h/.cpp` and verifies exact `getLines` and `getPlannedText` trailing-newline rules, `jam::Format::appendDecimal`, `.at()`, and ≤3 branches per function.

### Step 2: Implement `Source/Writer.h` and `Source/Writer.cpp`
**Scope:** `~/Documents/Poems/dev/saw/Source/Writer.h`, `~/Documents/Poems/dev/saw/Source/Writer.cpp`
**Action:**
1. Create `Source/Writer.h` (`N2`, `N6`, matching `cast/Source/Writer.h` `toFile` family):
   ```cpp
   #pragma once
   #include <JuceHeader.h>

   struct Writer
   {
       static juce::Result toFile (const juce::File& targetFile,
                                   std::string_view bytes,
                                   bool isExecutable);
       static bool isExecutable (const juce::File& file) noexcept;
   };
   ```
2. Create `Source/Writer.cpp`:
   - `toFile (targetFile, bytes, isExecutable)` (`A2`, `A3`, `A4`, `S9`):
     - Construct `juce::TemporaryFile temp (targetFile, juce::TemporaryFile::useHiddenFile);`.
     - In a block scope, open `juce::FileOutputStream stream (temp.getFile());`, check `not stream.failedToOpen()`, write `bytes` when `not bytes.empty()`, call `stream.flush()`, check `stream.getStatus().wasOk()`.
     - Call `temp.getFile().setExecutePermission (isExecutable)` (`A4`).
     - Verify `temp.overwriteTargetFileWithTemporary()`.
   - `isExecutable (file)` (`X9`): under `#if ! JUCE_WINDOWS`, `#include <sys/stat.h>` and check `stat (...) == 0 and (info.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) != 0`; under `#if JUCE_WINDOWS`, return `false`.
**Inventory:** A2, A3, A4, P17, N2, N6, S3, S6, S9, S10, S14, X9.
**Validation:** COUNSELOR reads `Source/Writer.h/.cpp` and checks: RAII `TemporaryFile` + `FileOutputStream`, checked `write()` + `flush()` + `getStatus()`, `setExecutePermission`, zero `replaceWithData()`.

### Step 3: Implement `Source/UndoCache.h` and `Source/UndoCache.cpp`
**Scope:** `~/Documents/Poems/dev/saw/Source/UndoCache.h`, `~/Documents/Poems/dev/saw/Source/UndoCache.cpp`
**Action:**
1. Create `Source/UndoCache.h` (owns ONLY `<app-data>/saw/undo/`):
   ```cpp
   #pragma once
   #include <JuceHeader.h>

   struct UndoCache
   {
       static constexpr int maxTransactions { 32 };
       static constexpr int randomHexLength { 6 };
       static constexpr int initialHexBound { 0x100000 };
       static constexpr size_t timeBufferCapacity { 32 };

       static juce::File getDirectory();
       static std::pair<juce::String, juce::String> getNextIdAndTimestamp();
       static std::pair<juce::Result, juce::String> addTransaction (const juce::String& commandName,
                                                                    const jam::Strings& canonicalPaths,
                                                                    const jam::Strings& preHashes,
                                                                    const jam::Strings& postHashes,
                                                                    const jam::Array<bool>& executableFlags,
                                                                    const jam::Array<std::string_view>& preEditBlobs);
       static jam::Strings getTransactionIds();
       static std::pair<juce::Result, std::unique_ptr<jam::JsonDocument>> getTransactionDocument (const juce::String& transactionId);

   private:
       static void removeOldestTransactions();
   };
   ```
2. Create `Source/UndoCache.cpp` implementing `SPEC.md` §6.1–6.3:
   - `getDirectory()`: `return jam::File::getUserApplicationDataDirectory (ProjectInfo::projectName).getChildFile (Id::undoFolder);` (`A23`).
   - `getNextIdAndTimestamp()` (`X8`, `A11`): use `gmtime_s (&utcTime, &now)` under `#if JUCE_WINDOWS` and `gmtime_r (&now, &utcTime)` under `#else` (no MSVC C4996 warning, thread-safe); format `Id::utcCompactFormat` and `Id::utcIsoFormat` via `std::strftime`; scan existing dirs for same-second monotonic hex suffix.
   - `addTransaction`: create directories with `jam::File::getOrCreateDirectory` (`A23`), write blobs and `meta.json` (`A16`, `P10`) via `Writer::toFile`; on any error, call `txDir.deleteRecursively (false)` before returning failure; on success call `removeOldestTransactions()`.
   - `removeOldestTransactions()`: collect directory names via non-recursive `RangedDirectoryIterator`, sort ascending via `jam::Strings::sort (false)`, and delete oldest entries beyond `maxTransactions` (`32`).
   - `getTransactionIds()`: return directory names sorted newest first.
   - `getTransactionDocument`: load `meta.json` and parse via `jam::JsonDocument::parse`, validate required keys and blob existence in place (`P11`, `S8`), return `{ ok, std::move (jsonDoc) }` or `failUndoUnknown` / `failUndoCorrupt`.
**Inventory:** A4, A5, A10, A11, A14, A16, A19, A20, A23, A24, A25, P10, P11, P14, P17, N6, N9, N10, S3, S4, S6, S7, S8, S9, S10, S11, S14, X8.
**Validation:** COUNSELOR reads `Source/UndoCache.h/.cpp` and checks: `gmtime_s`/`gmtime_r`, partial `txDir` cleanup on error, `jam::File::getUserApplicationDataDirectory (ProjectInfo::projectName)`, `removeOldestTransactions()` naming, zero fake-carrier structs.

### Step 4: Implement `Source/Transaction.h` and `Source/Transaction.cpp`
**Scope:** `~/Documents/Poems/dev/saw/Source/Transaction.h`, `~/Documents/Poems/dev/saw/Source/Transaction.cpp`
**Action:**
1. Create `Source/Transaction.h` (zero shadow `displayPaths`, zero dead declarations, `commit()` verb):
   ```cpp
   #pragma once
   #include <JuceHeader.h>
   #include "Arguments.h"
   #include "LineDiff.h"
   #include "TextDocument.h"

   class Transaction
   {
   public:
       explicit Transaction (const juce::String& newCommandName);

       void addFile (const juce::File& targetFile,
                     std::unique_ptr<TextDocument> originalDocument,
                     LineDiff diff,
                     int fileEditCount);
       int run (const Arguments& arguments, bool isStandardInput) const;

   private:
       std::string getDryRunDiffText() const;
       int runDryRun() const;
       int runCommit() const;
       std::pair<juce::Result, juce::String> commit() const;

       juce::String commandName;
       jam::Array<juce::File> targetFiles;
       jam::Array<std::unique_ptr<TextDocument>> originalDocuments;
       jam::Array<std::unique_ptr<TextDocument>> plannedDocuments;
       jam::Array<LineDiff> diffs;
       jam::Array<bool> executableFlags;
       int totalEditCount { 0 };
   };
   ```
2. Create `Source/Transaction.cpp` implementing `SPEC.md` §3.1, §3.2, §4.7, §5.2, §6.3:
   - `addFile`: `if (fileEditCount > 0)`: build `plannedDoc = TextDocument::fromString (diff.getPlannedText (*originalDocument), originalDocument->getDisplayPath())`, add `totalEditCount += fileEditCount`, and when `originalDocument->getBytes() != plannedDoc->getBytes()`, append to `targetFiles`, `originalDocuments`, `plannedDocuments`, `diffs`, and `executableFlags` (`Writer::isExecutable (targetFile)`).
   - `run`: enforce `--apply` + stdin (`failApplyStandardInput`), `--apply` without `--expect` (`failApplyRequiresExpect`), `--expect` integer and count check (`failExpectMismatch` exits `1` after printing diff), then dispatch `runCommit()` or `runDryRun()`.
   - `runDryRun`: write `getDryRunDiffText()`, print `summaryDryRun` with `(totalEditCount, originalDocuments.size(), totalEditCount)` when `totalEditCount > 0` (or `summaryDryRunZero` when `0`; when `totalEditCount > 0` and `originalDocuments.isEmpty()`, print `summaryDryRun` with `0` changed files or `summaryNoContentChanged` on apply).
   - `runCommit`: handle `totalEditCount == 0` (`summaryAppliedZero`), `originalDocuments.isEmpty()` (`summaryNoContentChanged`), else call `commit()` and print `summaryApplied`.
   - `commit()` (`SPEC.md` §6.3): stage via `UndoCache::addTransaction`. For `k` in `0..targetFiles.size() - 1`: verify on-disk SHA-256 equals `preHashes.at (k)`, write via `Writer::toFile`, re-read and verify SHA-256 equals `postHashes.at (k)`. On any failure at index `k`, restore **only already-swapped files `0..k - 1`** (and `k` only if `Writer::toFile` already succeeded on `k` before post-verify failed — never overwrite an externally modified file `k` that failed pre-check!), delete `txDir`, and return `failWrite`.
**Inventory:** A2, A3, A4, A7, A10, A19, A20, P14, P17, N5, N6, N9, N10, N11, S1, S3, S4, S6, S7, S8, S9, S10, S11, S14.
**Validation:** COUNSELOR reads `Source/Transaction.h/.cpp` and verifies: zero shadow `displayPaths`, zero dead declarations, `commit()` rolls back `0..k-1` on pre-check/write failure, ≤3 branches per function.

### Step 5: Implement `Source/Replace.h`, `Source/Replace.cpp`, and wire `Id::replace` in `Source/Main.cpp`
**Scope:** `~/Documents/Poems/dev/saw/Source/Replace.h`, `~/Documents/Poems/dev/saw/Source/Replace.cpp`, `~/Documents/Poems/dev/saw/Source/Main.cpp`
**Action:**
1. Create `Source/Replace.h` (`struct Replace { static int run (const juce::ArgumentList& arguments); };`).
2. Create `Source/Replace.cpp` implementing `SPEC.md` §5.2:
   - Parse `Arguments arguments (argumentList, 2, flagOptions, valueOptions)` with ONLY the 10 options of `SPEC.md` §5.2:
     - `flagOptions`: `jam::Strings { Id::regexOption, Id::ignoreCaseOption, Id::withStdinOption, Id::applyOption }`
     - `valueOptions`: `jam::Strings { Id::linesOption, Id::withFileOption, Id::includeOption, Id::excludeOption, Id::expectOption }`
   - Reject `--with-stdin` + `--with-file` (`failWithSourcesConflict`).
   - Validate `--lines` via `Arguments::getLineRange (..., false)`, compile `Pattern::create`.
   - When `-r` is set, validate replacement `$` references: `$$` is literal `$`; `$0`..`$9` require `digit <= pattern->getNumCaptureGroups()` (fail with `failCaptureInvalid`, exit `2` when `digit > pattern->getNumCaptureGroups()`); any `$` not followed by `$` or `0`..`9` (including trailing `$`) is emitted literally as `$`.
   - `getDocumentDiff` scans each line in `lineRange`, expands matches, advances past `matchEnd` (or `+ 1` byte for zero-length match), builds `LineDiff::Change` entries.
   - Plan over stdin (`isStandardInput = true`) or `Paths::getFiles (..., false, false)` via `Jobs::run`, add to `Transaction (Id::replace.toString())`, return `transaction.run (arguments, isStandardInput)`.
3. Wire `Id::replace` in `Source/Main.cpp`.
**Inventory:** A1, A6, A7, A8, A10, A13, A17, A19, A20, P3, P6, P7, P14, P17, N1, N5, N6, N9, N10, N11, S1, S2, S3, S4, S6, S7, S8, S10, S11, S14.
**Validation:** COUNSELOR reads `Source/Replace.h/.cpp` and `Source/Main.cpp` and verifies exact `SPEC.md` §5.2 option set, `digit > numCaptureGroups` validation, literal non-digit `$`, and ≤3 branches per function.

### Step 6: Add Part 6 `replace` test cases to `test/cases.md`
**Scope:** `~/Documents/Poems/dev/saw/test/cases.md`, `~/Documents/Poems/dev/saw/test/fixture/replace-basic/`, `~/Documents/Poems/dev/saw/test/stdin/`, `~/Documents/Poems/dev/saw/test/baseline/`
**Action:** Create fixtures and add reference (`sed`) and baseline cases (dry-run, 0 edits, `--apply --expect 0`, no-op identical replacement, overlapping `"aa"` in `"aaa"`, zero-length `^`, CRLF, `\ No newline at end of file`, `--with-file`, `--with-stdin`, `--expect` mismatch exit `1`, `--apply` without `--expect` exit `2`, invalid capture `$5` exit `2`, valid `$2` with 2 groups, `--apply` on stdin exit `2`) to `test/cases.md`.
**Inventory:** A26, P12.
**Validation:** ARCHITECT builds `saw` and runs `nvim --headless -l test/run.lua ~/.local/bin/saw`; every case prints `PASS` and exits `0`.

## BLESSED Alignment

- **B — Bound:** `Writer::toFile` scopes RAII temp files; `UndoCache::addTransaction` cleans partial `txDir`; `Transaction::commit` rolls back `0..k-1`.
- **L — Lean:** `Writer` and `UndoCache` have one responsibility each; every function has ≤3 branches.
- **E — Explicit:** Exact `getLines`, trailing-newline, capture-bound, and rollback rules leave zero room for inference.
- **S — SSOT:** `LineDiff::changes` drives both diff and planned text; `Transaction` holds no shadow `displayPaths`.
- **S — Stateless:** `Writer`, `UndoCache`, and `Replace` are stateless structs.
- **E — Encapsulation:** `Replace` tells `Transaction::addFile` and `Transaction::run`.
- **D — Deterministic:** Sorted UTF-8 paths and monotonic same-second hex suffixes.

## Risks / Open Questions

None.
