# PLAN: saw — Part 7: lines Mutate Mode, PatchValidator, and patch

**RFC:** none — objective from ARCHITECT prompt
**Date:** 2026-11-22
**BLESSED Compliance:** verified
**Language Constraints:** C++17 / JUCE 8.0.17 + JAM (LANGUAGE.md "C++ / JUCE")

## Overview

This part completes the mutating mode of `lines` (`SPEC.md` §5.3), implements `PatchValidator : jam::JsonValidator` and `Patch` (`SPEC.md` §5.5), wires `Id::patch` in `Source/Main.cpp`, and adds reference (`sed`) and baseline test cases to `test/cases.md`.

## Language / Framework Constraints

- One object, one translation unit (`CODING.md:899-937`): `Lines` in `Source/Lines.h/.cpp`, `PatchValidator` in `Source/PatchValidator.h/.cpp`, `Patch` in `Source/Patch.h/.cpp`.
- Established JAM validator family (`N4`, `A14`, `jam_JsonValidator.h:12`): `struct PatchValidator : jam::JsonValidator` validates JSON syntax and patch schema via `addRules (Rules&)` / `getRules() const override`, plus `isValidForFile (...)`. `PatchValidator` owns validation only (no JSON getter helpers).
- Complete at creation and zero fake-carrier structs (`S8`, `P11`): `PatchValidator` and `Patch` read `jam::JsonDocument` directly in place.
- Zero raw string literals in `.cpp` (`PLAN-saw-0-tracker.md` Rule 4): every error detail comes from `text::Diagnostics::failPatch*` (`cast/text.md`).
- At most 3 branches per function (`S14`): operation shapes dispatch through `jam::Function::Map` tables (`lineMutations`, `entryValidators`, `entryChanges`, `N13`).
- `Patch` checks `--expect N` against `entries.size()` before reading files (`SPEC.md` §5.5 step 2) and sorts `LineDiff::Change` entries in ascending `start` order before constructing `LineDiff`.
- No comments and no doxygen before the audit (`CAROL.md` Code Hygiene).

## Framework Inventory

Full tables live in `PLAN-saw-0-tracker.md` §Framework Inventory. This part uses:

- **API:** A1, A4, A7, A8, A10, A14 (`jam::JsonDocument`, `jam::JsonValidator`), A15 (`jam::Format::getNumber<int>`), A17, A18, A19, A20, A25.
- **Pattern:** P3, P6, P11, P14, P17.
- **Names:** N1 (`Lines`, `Patch`), N4 (`PatchValidator : jam::JsonValidator`, `addRules`, `getRules`, `isValidForFile`), N5 (`LineDiff`, `LineDiff::Change`), N6 (`run`, `get`, `add`, `is`), N9, N10 (`Transaction`), N11, N13 (`lineMutations`, `entryValidators`, `entryChanges`).
- **Synthesis:** S1, S2, S3, S4, S5, S6, S7, S8, S9, S10, S11, S12, S13, S14.
- **Absence:** none.

## Validation Gate

Each step is validated by COUNSELOR before proceeding to the next — against `MANIFESTO.md` (BLESSED), `NAMES.md`, `CODING.md`, and the locked PLAN decisions. @Auditor runs ONCE, after the final step — never per step.

## Steps

### Step 1: Implement `lines` Mutate Mode in `Source/Lines.cpp`
**Scope:** `~/Documents/Poems/dev/saw/Source/Lines.cpp`
**Action:** Replace the Part 5 mutate placeholder in `Source/Lines.cpp` with the full implementation of `SPEC.md` §5.3 Mutate Mode:
1. Include `"LineDiff.h"` and `"Transaction.h"`.
2. Define static `getLineMutations()` (`N13`, `S1`, `S2`, `S14`) returning `const jam::Function::Map<juce::Identifier, jam::Function::pointerCapacity, LineDiff::Change (juce::Range<int>, std::string_view)>&`:
   - `juce::Identifier (Id::deleteOption)` → `LineDiff::Change { lineRange.getStart() - 1, lineRange.getLength(), {} }`
   - `juce::Identifier (Id::replaceOption)` → `LineDiff::Change { lineRange.getStart() - 1, lineRange.getLength(), LineDiff::getLines (payloadText) }`
   - `juce::Identifier (Id::insertBeforeOption)` → `LineDiff::Change { lineRange.getStart() - 1, 0, LineDiff::getLines (payloadText) }`
   - `juce::Identifier (Id::insertAfterOption)` → `LineDiff::Change { lineRange.getStart(), 0, LineDiff::getLines (payloadText) }`
3. In `runMutation`:
   - Reject read-only options (`--line-number`, `--hash`) and `--stdin` with `--delete` (`failFlagUnknown`, exit `2`).
   - Require single-line token (no `:`) for `--insert-before` / `--insert-after` (`failInsertSingleLine`, exit `2`).
   - Require `positionals.size() >= 2` (`failMissingPath`, exit `2`).
   - Read `payloadText` from `Stdio::getInput()` when `--stdin`, else `arguments.getValue (activeMutation.toString()).toStdString()`.
   - Collect target files via `Paths::getFiles (rawPaths, {}, {}, false, false)`, load each `TextDocument::fromFile`, reject binary files (`failBinaryFile`), check bounds (`failLineRangeBounds`, allowing `0` for `--insert-after`), check `--verify-hash` (`failHashMismatch`), build `LineDiff ({ change })`, call `transaction.addFile (targetFile, std::move (doc), std::move (diff), 1)`, and return `transaction.run (arguments, false)`.
**Inventory:** A1, A7, A8, A10, A17, A19, A20, P3, P6, P17, N1, N5, N6, N9, N10, N11, N13, S1, S2, S3, S4, S6, S8, S10, S11, S14.
**Validation:** COUNSELOR reads `Source/Lines.cpp` and verifies `getLineMutations()` dispatch, bounds/hash checks, `Transaction::addFile` + `run`, and ≤3 branches per function.

### Step 2: Implement `Source/PatchValidator.h` and `Source/PatchValidator.cpp`
**Scope:** `~/Documents/Poems/dev/saw/Source/PatchValidator.h`, `~/Documents/Poems/dev/saw/Source/PatchValidator.cpp`
**Action:**
1. Create `Source/PatchValidator.h` (`N4`, inherits `jam::JsonValidator`; validation only):
   ```cpp
   #pragma once
   #include <JuceHeader.h>
   #include "TextDocument.h"

   struct PatchValidator : jam::JsonValidator
   {
       static void addRules (Rules& rules);
       const Rules& getRules() const override;

       static juce::Result isValidForFile (const jam::JsonDocument& document,
                                           const jam::Array<int>& entryIndices,
                                           const TextDocument& targetDocument);
   };
   ```
2. Create `Source/PatchValidator.cpp` implementing `SPEC.md` §5.5 Validation Rules:
   - `addRules (Rules& rules)` calls `jam::JsonValidator::addRules (rules)` and registers patch schema rule checking:
     - Root value is `map::JsonTokenType::array` (else `failPatchJsonInvalid` with `failPatchRootNotArray` — zero raw literals!).
     - Each array element is an object (`failPatchEntryNotObject`), matches exactly one of the five distinguishing keys `{ Id::startCol, Id::deletion, Id::startLine, Id::insertAfter, Id::insertBefore }` (`failPatchEntryShape`), and dispatches to `getEntryValidators()` (`jam::Function::Map`, `N13`, `S14`) checking exact required/optional keys and types (`failPatchEntryFields`).
   - `isValidForFile (document, entryIndices, targetDocument)`:
     - Checks line bounds (`1 <= startLine <= endLine <= numLines`, or `0 <= insertAfter <= numLines`), column bounds (`0 <= startCol <= endCol <= lineLen`), and optional `Id::hash` (`failPatchBounds` / `failHashMismatch`).
     - Rejects all conflicts with `failPatchConflict`:
       1. Two column-span edits on the same line with `prev.endCol > curr.startCol` or `prev.startCol == curr.startCol`.
       2. Any column-span edit combined with a delete or range-replace on the same line.
       3. Overlapping delete/replace line ranges (`juce::Range<int>::intersects`, `A10`).
       4. Two inserts (`insertBefore` or `insertAfter`) targeting the same line, or an insert targeting a line covered by a delete or range-replace.
**Inventory:** A10, A14, A15, A17, A19, A25, P6, P11, P17, N4, N6, N9, N13, S1, S2, S3, S4, S6, S7, S8, S9, S10, S12, S14.
**Validation:** COUNSELOR reads `Source/PatchValidator.h/.cpp` and checks: inherits `jam::JsonValidator`, validation-only API, zero raw string literals, explicit insert conflict checks, ≤3 branches per function.

### Step 3: Implement `Source/Patch.h`, `Source/Patch.cpp`, and wire `Id::patch` in `Source/Main.cpp`
**Scope:** `~/Documents/Poems/dev/saw/Source/Patch.h`, `~/Documents/Poems/dev/saw/Source/Patch.cpp`, `~/Documents/Poems/dev/saw/Source/Main.cpp`
**Action:**
1. Create `Source/Patch.h` (`struct Patch { static int run (const juce::ArgumentList& arguments); };`).
2. Create `Source/Patch.cpp` implementing `SPEC.md` §5.5:
   - Parse `Arguments arguments (argumentList, 2, jam::Strings { Id::applyOption }, jam::Strings { Id::fileOption, Id::expectOption })`; reject extra positionals (`failUnexpectedArgument`).
   - Read JSON bytes from `--file` or piped stdin (`failPatchRequiresInput` when stdin is a terminal and `--file` absent).
   - Parse `jam::JsonDocument::parse (jsonBytes)` and run `PatchValidator {}.isValid (*jsonDoc)` (`N4`, `A14`).
   - **Check `--expect N` before reading any target file (`SPEC.md` §5.5 step 2):** when `--apply` without `--expect`, fail with `failApplyRequiresExpect` (`exit 2`); when `--expect` is set, validate integer (`>= 0`) and compare against `entryCount` (if mismatch, print `failExpectMismatch` and return `Stdio::exitMismatch`, `exit 1`).
   - Group root entry indices by canonical symlink-resolved target path (`getLinkedTarget().getFullPathName()`) in ascending UTF-8 `displayPath` order (`jam::Strings::sort (false)`).
   - Build `LineDiff::Change` entries using `getEntryChanges()` (`jam::Function::Map`, `N13`, `S14`) for line operations and right-to-left (descending `startCol`) splice for column-span entries.
   - **Sort `changes` in ascending `start` order** (with `insertBefore` before `insertAfter` at equal `start` if ever applicable) before constructing `LineDiff (std::move (changes))`.
   - For each target file: load `TextDocument::fromFile`, reject binary, call `PatchValidator::isValidForFile`, call `transaction.addFile (targetFile, std::move (targetDoc), std::move (diff), entryIndices.size())`.
   - Return `transaction.run (arguments, false)`.
3. Wire `Id::patch` in `Source/Main.cpp`.
**Inventory:** A1, A4, A8, A10, A14, A15, A17, A18, A19, A20, P3, P6, P11, P17, N1, N4, N5, N6, N9, N10, N11, N13, S1, S2, S3, S4, S6, S8, S10, S11, S14.
**Validation:** COUNSELOR reads `Source/Patch.h/.cpp` and `Source/Main.cpp` and verifies: `--expect` checked before file reads, `changes` sorted ascending by `start`, zero intermediate structs, ≤3 branches per function.

### Step 4: Add Part 7 `lines` (Mutate Mode) and `patch` test cases to `test/cases.md`
**Scope:** `~/Documents/Poems/dev/saw/test/cases.md`, `~/Documents/Poems/dev/saw/test/fixture/lines-basic/`, `~/Documents/Poems/dev/saw/test/fixture/patch-basic/`, `~/Documents/Poems/dev/saw/test/stdin/`, `~/Documents/Poems/dev/saw/test/baseline/`
**Action:** Create fixtures and stdin payloads (`lines_insert.txt`, `patch_valid.json`, `patch_unsorted.json`, `patch_empty.json`, `patch_bad_json.txt`, `patch_bad_entry.json`, `patch_conflict_cols.json`, `patch_conflict_ranges.json`, `patch_conflict_inserts.json`, `patch_bounds.json`, `patch_hash_mismatch.json`) and add reference (`sed`) and baseline cases to `test/cases.md`.
**Inventory:** A26, P12.
**Validation:** ARCHITECT builds `saw` and runs `nvim --headless -l test/run.lua ~/.local/bin/saw`; every case prints `PASS` and exits `0`.

## BLESSED Alignment

- **B — Bound:** `PatchValidator` validates JSON syntax, schema, bounds, hashes, and conflicts before any disk write.
- **L — Lean:** `PatchValidator : jam::JsonValidator` joins the JAM validator family; `Function::Map` tables keep every function ≤3 branches.
- **E — Explicit:** Zero raw literals; explicit insert conflict rules and ascending `changes` sort.
- **S — SSOT:** `jam::JsonDocument` is read in place (`S8`); `TextDocument::getLineHash` verifies all hashes.
- **S — Stateless:** `Lines`, `PatchValidator`, and `Patch` are stateless structs.
- **E — Encapsulation:** Validation lives in `PatchValidator`; execution and commit live in `Transaction`.
- **D — Deterministic:** Canonical symlink grouping, sorted UTF-8 `displayPath` order, sorted `changes`.

## Risks / Open Questions

None.
