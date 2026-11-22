# PLAN: saw — Part 5: fields and lines Read Mode

**RFC:** none — objective from ARCHITECT prompt
**Date:** 2026-11-22
**BLESSED Compliance:** verified
**Language Constraints:** C++17 / JUCE 8.0.17 + JAM (LANGUAGE.md "C++ / JUCE")

## Overview

This part implements the `fields` verb (`SPEC.md` §5.4) and the read mode of the `lines` verb (`SPEC.md` §5.3), wires both in `Source/Main.cpp`, and adds reference (`awk`, `sed`) and baseline cases to `test/cases.md`.

## Language / Framework Constraints

- One object, one translation unit (`CODING.md:899-937`): `Fields` in `Source/Fields.h/.cpp`, `Lines` in `Source/Lines.h/.cpp`.
- Stateless command structs with `static int run (const juce::ArgumentList& arguments)` (`P3`, `N1`).
- Zero fake-carrier structs (`S8`): line ranges use `juce::Range<int>` from `Arguments::getLineRange` (`A10`), file lists use `jam::Strings` from `Paths::getFiles` (`A20`).
- Strictly adheres to `SPEC.md` §5.3 and §5.4: neither `lines` nor `fields` emits an unlisted `<path>:` prefix on multi-file output (matching `sed` and `awk`).
- Every function has ≤3 branches (`S14`), uses `.at()` (`S4`), and has zero bail-out guards or `else` after `return` (`S3`).
- No comments and no doxygen before the audit (`CAROL.md` Code Hygiene).

## Framework Inventory

Full tables live in `PLAN-saw-0-tracker.md` §Framework Inventory. This part uses:

- **API:** A1, A6, A7, A8, A10 (`juce::Range<int>::getIntersectionWith`), A12, A13, A17, A19, A20, A25.
- **Pattern:** P3, P5, P6, P7, P14, P17.
- **Names:** N1 (`Fields`, `Lines`), N6 (`run`, `get`, `is`, `has`), N9, N11.
- **Synthesis:** S1, S2, S3, S4, S5, S6, S7, S8, S9, S10, S11, S12, S13, S14.
- **Absence:** X4, X5, X6.

## Validation Gate

Each step is validated by COUNSELOR before proceeding to the next — against `MANIFESTO.md` (BLESSED), `NAMES.md`, `CODING.md`, and the locked PLAN decisions. @Auditor runs ONCE, after the final step — never per step.

## Steps

### Step 1: Implement `Source/Fields.h` and `Source/Fields.cpp`
**Scope:** `~/Documents/Poems/dev/saw/Source/Fields.h`, `~/Documents/Poems/dev/saw/Source/Fields.cpp`
**Action:**
1. Create `Source/Fields.h` (`struct Fields { static int run (const juce::ArgumentList& arguments); };`).
2. Create `Source/Fields.cpp` implementing `SPEC.md` §5.4:
   - Parse `Arguments arguments (argumentList, 2, flagOptions, valueOptions)`:
     - `flagOptions`: `jam::Strings { Id::regexOption, Id::firstOption, Id::strictOption }`
     - `valueOptions`: `jam::Strings { Id::delimiterOption, Id::outputDelimiterOption, Id::matchOption, Id::linesOption, Id::afterOption }`
   - Check `arguments.getStatus()`; require `positionals.size() >= 1` (`failFieldsRequiresSpec`).
   - `getFieldIndices (specText)` → `std::pair<juce::Result, jam::Array<int>>`: reject empty/leading/trailing/consecutive `,` (`failFieldSpecInvalid`), validate each integer via round-trip (`P5`), reject `0` (`failFieldSpecZero`).
   - `getOutputDelimiter (arguments)`: `--output-delimiter` if set, else `--delimiter` if set, else `" "`. Reject empty `-d ""` (`failDelimiterEmpty`).
   - Validate `--after` (`>= 0`), `--lines` (`Arguments::getLineRange (..., false)`), and `--match` (`Pattern::create`).
   - `getLineTokens (line, hasCustomDelimiter, delimiter)` → `jam::Array<std::string_view>`:
     - When `line.empty()`: return `{}` (`0` fields for both default whitespace and custom delimiter, matching `awk`).
     - When `not hasCustomDelimiter`: trim leading/trailing `' '` and `'\t'`; if empty return `{}`; split on contiguous runs of `' '` and `'\t'`.
     - When `hasCustomDelimiter`: split `line` on literal occurrences of `delimiter`.
   - `getFormattedDocument (doc, fieldIndices, hasCustomDelimiter, delimiter, outputDelimiter, matchPattern, lineRange, afterLine, isFirstOnly, isStrict)` → `std::tuple<juce::Result, bool, std::string>` (`{ status, anyLineMatched, output }`):
     - Iterate 1-based `lineNumber` in `1..doc.getNumLines()` where `lineNumber > afterLine and lineRange.contains (lineNumber)`:
       - When `matchPattern != nullptr` and `not matchPattern->hasMatch (doc.getLine (lineNumber))`, skip line.
       - Set `anyLineMatched = true`.
       - Compute `tokens = getLineTokens (...)`.
       - When `tokens.isEmpty() and not isStrict`: append `"\n"`.
       - Otherwise iterate `k` in `0..fieldIndices.size() - 1`:
         - `requested = fieldIndices.at (k)`; `slotIndex = requested > 0 ? requested - 1 : tokens.size() + requested`.
         - If `slotIndex < 0 or slotIndex >= tokens.size()`: when `isStrict`, return `{ fail (failFieldBounds), false, {} }`.
         - Else append `tokens.at (slotIndex)`.
         - When `k + 1 < fieldIndices.size()`, append `outputDelimiter`.
         - After loop, append `"\n"`.
       - When `isFirstOnly`, stop loop (`break`).
   - Run over stdin (`Stdio::getInput()`) when `positionals.size() == 1 and not Stdio::isTerminalInput()` (failing with `failMissingPath` when stdin is a terminal), or over `Paths::getFiles` via `Jobs::run`.
   - When `arguments.contains (Id::matchOption) and not anyLineMatched`, return `Stdio::exitMismatch` (`1`).
   - Write via `Stdio::write` and return `Stdio::exitSuccess` (`0`).
**Inventory:** A6, A8, A10, A19, A20, A25, P3, P5, P7, P14, P17, N1, N6, N9, N11, S1, S3, S4, S5, S6, S7, S8, S10, S11, S12, S14.
**Validation:** COUNSELOR reads `Source/Fields.h/.cpp` and verifies non-empty line formatting (`\n`, `anyLineMatched`, `isFirstOnly`), empty-line `0`-field behavior, zero `<path>:` prefix, `.at()`, and ≤3 branches per function.

### Step 2: Implement `Source/Lines.h` and `Source/Lines.cpp` (Read Mode) and wire `Id::fields` and `Id::lines` in `Source/Main.cpp`
**Scope:** `~/Documents/Poems/dev/saw/Source/Lines.h`, `~/Documents/Poems/dev/saw/Source/Lines.cpp`, `~/Documents/Poems/dev/saw/Source/Main.cpp`
**Action:**
1. Create `Source/Lines.h` (`struct Lines { static int run (const juce::ArgumentList& arguments); };`).
2. Create `Source/Lines.cpp` implementing `SPEC.md` §5.3 Read Mode:
   - Check `const bool hasStdinFlag { Arguments::containsOptionBeforeTerminator (argumentList, 2, Id::stdinOption) };` (stops at `--`). When true, place `Id::replaceOption`, `Id::insertBeforeOption`, and `Id::insertAfterOption` in `flagOptions`; otherwise in `valueOptions`.
   - Count active mutating flags among `{ Id::deleteOption, Id::replaceOption, Id::insertBeforeOption, Id::insertAfterOption }`; if `> 1`, print `failMutateFlagsConflict` and return `Stdio::exitError` (`2`).
   - Require `positionals.size() >= 1` (`failLinesRequiresRange`); parse `Arguments::getLineRange (positionals.at (0), arguments.contains (Id::insertAfterOption))`.
   - When `mutateCount == 0` (Read mode):
     - Reject mutating-only flags (`--stdin`, `--verify-hash`, `--apply`, `--expect`) with `failFlagUnknown` (`exit 2`).
     - `getFormattedLines (doc, lineRange, isShowingHash, isShowingLineNumber)`:
       - Clamp via `const auto clampedRange { lineRange.getIntersectionWith (juce::Range<int> (1, doc.getNumLines() + 1)) };` (`A10`, `S9`).
       - For each `line` in `[clampedRange.getStart(), clampedRange.getEnd())`:
         - If `isShowingHash`: emit `<line>:<hhhh>|<lineText>\n` (`--hash` already includes the line number and takes precedence over `-n`).
         - Else if `isShowingLineNumber`: emit `<line>\t<lineText>\n`.
         - Else: emit `<lineText>\n`.
     - Run over stdin or `Paths::getFiles` via `Jobs::run`; write via `Stdio::write` and return `Stdio::exitSuccess` (`0`).
   - When `mutateCount == 1`: return `Stdio::exitError` placeholder until Part 7 Step 1 implements `runMutation`.
3. Wire `Id::fields` and `Id::lines` in `Source/Main.cpp`.
**Inventory:** A6, A7, A8, A10, A17, A19, A20, P3, P6, P7, P17, N1, N6, N9, N11, S1, S2, S3, S4, S6, S8, S9, S10, S11, S14.
**Validation:** COUNSELOR reads `Source/Lines.h/.cpp` and `Source/Main.cpp` and checks: `--`-safe `--stdin` detection, `getIntersectionWith` clamping, `--hash` over `-n` precedence, zero `<path>:` prefix, ≤3 branches per function.

### Step 3: Add Part 5 `fields` and `lines` (Read Mode) cases to `test/cases.md`
**Scope:** `~/Documents/Poems/dev/saw/test/cases.md`, `~/Documents/Poems/dev/saw/test/fixture/fields-basic/`, `~/Documents/Poems/dev/saw/test/fixture/lines-basic/`, `~/Documents/Poems/dev/saw/test/baseline/`
**Action:** Create fixtures and add reference (`awk -F:`, `sed -n "3,7p"`, `sed -n "5p"`, `sed -n "1,4p"`, `awk "NR>=8"`) and baseline cases (`fields` quoted, `--match --first`, `--match -r` miss exit `1`, empty line with `-d`, `--strict` exit `2`, index `0` exit `2`, invalid spec exit `2`, empty `-d ""` exit `2`; `lines` `-n`, `--hash`, `--hash -n`, out-of-bounds clamp exit `0`, start-past-EOF exit `0`, invalid range `5:2` exit `2`, negative range `-1` exit `2`) to `test/cases.md`.
**Inventory:** A26, P12.
**Validation:** ARCHITECT builds `saw` and runs `nvim --headless -l test/run.lua ~/.local/bin/saw`; every case prints `PASS` and exits `0`.

## BLESSED Alignment

- **B — Bound:** `Fields` and `Lines` operate over `TextDocument` views with zero copy of unrequested lines.
- **L — Lean:** `juce::Range<int>::getIntersectionWith` clamps ranges in one expression (`S9`). Every function has ≤3 branches.
- **E — Explicit:** Every error condition and precedence rule (`--hash` over `-n`, empty-line `0` fields) is explicit.
- **S — SSOT:** Line range parsing lives in `Arguments::getLineRange`; 4-hex hashing lives in `TextDocument::getLineHash`.
- **S — Stateless:** `Fields` and `Lines` are stateless structs.
- **E — Encapsulation:** Input collection and argument parsing stay in `Paths` and `Arguments`.
- **D — Deterministic:** Multi-file inputs run in sorted UTF-8 path order and join deterministically.

## Risks / Open Questions

None.
