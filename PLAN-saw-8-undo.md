# PLAN: saw — Part 8: undo

**RFC:** none — objective from ARCHITECT prompt
**Date:** 2026-11-22
**BLESSED Compliance:** verified
**Language Constraints:** C++17 / JUCE 8.0.17 + JAM (LANGUAGE.md "C++ / JUCE")

## Overview

This part implements the `undo` verb (`SPEC.md` §5.6, §6), wires `Id::undo` in `Source/Main.cpp`, and adds multi-step transaction, drift, and rollback test cases to `test/cases.md`.

## Language / Framework Constraints

- One object, one translation unit (`CODING.md:899-937`): `Undo` in `Source/Undo.h/.cpp`.
- Stateless command struct with `static int run (const juce::ArgumentList& arguments)` (`P3`, `N1`).
- Complete at creation and zero fake-carrier structs (`S8`, `P11`): `Undo` reads `meta.json` directly from `UndoCache::getTransactionDocument (txId)`.
- Reverse unified diff (`current -> original`) uses `LineDiff (current, original)` (`Part 6 Step 1`).
- Atomic restoration uses `Writer::toFile` (`Part 6 Step 2`, `A2`, `A3`, `A4`), verifies post-restore SHA-256 against `preHash`, and rolls back `0..k - 1` (with `preUndoExecutableFlags`) on failure at `k` (`SPEC.md` §5.6).
- No comments and no doxygen before the audit (`CAROL.md` Code Hygiene).

## Framework Inventory

Full tables live in `PLAN-saw-0-tracker.md` §Framework Inventory. This part uses:

- **API:** A1, A2, A3, A4, A7, A8, A10, A14, A17, A19, A20, A23, A25.
- **Pattern:** P3, P6, P11, P14, P17.
- **Names:** N1 (`Undo`), N2 (`Writer`), N5 (`LineDiff`), N6 (`run`, `get`, `toFile`, `is`), N9, N10 (`UndoCache`), N11.
- **Synthesis:** S1, S2, S3, S4, S5, S6, S7, S8, S9, S10, S11, S13, S14.
- **Absence:** X1, X9.

## Validation Gate

Each step is validated by COUNSELOR before proceeding to the next — against `MANIFESTO.md` (BLESSED), `NAMES.md`, `CODING.md`, and the locked PLAN decisions. @Auditor runs ONCE, after the final step — never per step.

## Steps

### Step 1: Implement `Source/Undo.h`, `Source/Undo.cpp`, and wire `Id::undo` in `Source/Main.cpp`
**Scope:** `~/Documents/Poems/dev/saw/Source/Undo.h`, `~/Documents/Poems/dev/saw/Source/Undo.cpp`, `~/Documents/Poems/dev/saw/Source/Main.cpp`
**Action:**
1. Create `Source/Undo.h` (`struct Undo { static int run (const juce::ArgumentList& arguments); };`).
2. Create `Source/Undo.cpp` implementing `SPEC.md` §5.6:
   - Parse `Arguments arguments (argumentList, 2, jam::Strings { Id::listOption, Id::applyOption, Id::forceOption }, jam::Strings {})`.
   - `runList`: reject extra positionals or `--apply`/`--force` (`exit 2`); iterate `UndoCache::getTransactionIds()`, read `id`, `timestamp`, `command`, and `files` count from `UndoCache::getTransactionDocument (txId)` (`P11`), print `text::Diagnostics::undoListFormat` via `Stdio::print`; return `Stdio::exitSuccess` (`0`).
   - Select `txId` from `positionals.at (0)` or latest `UndoCache::getTransactionIds().at (0)` (else `failUndoEmpty`, `exit 2`).
   - Load `metaDoc` via `UndoCache::getTransactionDocument (txId)`.
   - Verify every file in `Id::files` of `*metaDoc` into parallel arrays `targetFiles`, `currentDocuments`, `originalDocuments`, `preHashes`, `executableFlags`, `preUndoExecutableFlags`, `existedBeforeUndo`:
     - Load `originalDoc` from `txDir.getChildFile (blobName)` and verify `originalDoc->getContentHash().compare (preHash) == 0` (else `failUndoCorrupt`).
     - When `not targetFile.existsAsFile()`: without `--force`, fail with `failUndoMissing` (`exit 2`); with `--force`, push `TextDocument::fromString ({}, canonicalPath)`, `preUndoExecutableFlags.add (false)`, `existedBeforeUndo.add (false)`.
     - When `targetFile.existsAsFile()`: load `currentDoc`, verify `currentDoc->getContentHash().compare (postHash) == 0` unless `--force` (else `failUndoDrift`, `exit 2`), push `preUndoExecutableFlags.add (Writer::isExecutable (targetFile))`, `existedBeforeUndo.add (true)`.
   - Dry-run (default): concatenate `LineDiff (*currentDocuments.at (i), *originalDocuments.at (i)).getUnifiedDiff (...)`, write via `Stdio::write`, print `summaryDryRunUndo`, return `Stdio::exitSuccess` (`0`).
   - `--apply`: for `k` in `0..targetFiles.size() - 1`, call `Writer::toFile (targetFiles.at (k), originalDocuments.at (k)->getBytes(), executableFlags.at (k))` and verify restored SHA-256 equals `preHashes.at (k)`. On any failure at `k`, roll back `0..k - 1` (and `k` only if `toFile` already swapped `k` before post-verify failed): for each rolled-back `j`, if `existedBeforeUndo.at (j)` restore `currentDocuments.at (j)->getBytes()` with `preUndoExecutableFlags.at (j)` via `Writer::toFile`, else delete `targetFiles.at (j)`; print `failUndoWrite` and return `Stdio::exitError` (`2`).
   - Delete `UndoCache::getDirectory().getChildFile (txId).deleteRecursively (false)`, print `summaryUndone`, return `Stdio::exitSuccess` (`0`).
3. Wire `Id::undo` in `Source/Main.cpp`.
**Inventory:** A1, A2, A3, A4, A7, A8, A10, A14, A17, A19, A20, P3, P6, P11, P14, P17, N1, N2, N5, N6, N9, N10, N11, S1, S2, S3, S4, S6, S7, S8, S9, S10, S11, S13, S14, X1, X9.
**Validation:** COUNSELOR reads `Source/Undo.h/.cpp` and `Source/Main.cpp` and checks: `preUndoExecutableFlags` populated and used on rollback, rollback range `0..k-1`, `Writer::toFile` used, zero intermediate structs, ≤3 branches per function.

### Step 2: Add Part 8 `undo` multi-step test cases to `test/cases.md`
**Scope:** `~/Documents/Poems/dev/saw/test/cases.md`, `~/Documents/Poems/dev/saw/test/fixture/undo-basic/`, `~/Documents/Poems/dev/saw/test/fixture/undo-drifted/`, `~/Documents/Poems/dev/saw/test/baseline/`
**Action:** Create fixtures (`doc.txt`, executable `script.sh`, drifted `doc.txt`) and add cases (`undo-empty-cache`, `undo-list-empty`, `undo-unknown-tx`, `undo-dry-run-and-apply`, `undo-lifo-two-transactions`, `undo-drift-refused-and-forced`) to `test/cases.md`.
**Inventory:** A26, P12.
**Validation:** ARCHITECT builds `saw` and runs `nvim --headless -l test/run.lua ~/.local/bin/saw`; every case across all six verbs prints `PASS` and exits `0`.

## BLESSED Alignment

- **B — Bound:** Verifies every `postHash` and `preHash` before writing; rolls back `0..k-1` with original `preUndoExecutableFlags`.
- **L — Lean:** Reuses `UndoCache`, `Writer`, and `LineDiff`; every function has ≤3 branches.
- **E — Explicit:** Drift and missing files fail with `failUndoDrift` / `failUndoMissing` unless `--force` is passed.
- **S — SSOT:** `UndoCache` `meta.json` is read in place without carrier structs.
- **S — Stateless:** `Undo` is a stateless struct.
- **E — Encapsulation:** Reads through `UndoCache::getTransactionDocument`, writes through `Writer::toFile`.
- **D — Deterministic:** Same-second monotonic hex suffixes guarantee deterministic LIFO ordering.

## Risks / Open Questions

None.
