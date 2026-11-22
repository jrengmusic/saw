# PLAN: saw — Part 1: jam::RegExp::findNext

**RFC:** none — objective from ARCHITECT prompt
**Date:** 2026-11-22
**BLESSED Compliance:** verified
**Language Constraints:** C++17 / JUCE 8.0.17 + JAM (LANGUAGE.md "C++ / JUCE")

## Overview

This part adds `jam::RegExp::findNext (std::string_view text, size_t offset) const` to `jam_core` in `~/Documents/Poems/dev/jam` (`SPEC.md` §3 item 3, §4.8, §9 Performance and Quality item 3). `getCaptures (text)` forwards to `findNext (text, 0)` so every existing caller keeps identical behavior.

## Language / Framework Constraints

- One object, one translation unit (`CODING.md:899-937`): `jam::RegExp` lives in `jam_core/text/jam_RegExp.h` and `jam_core/text/jam_RegExp.cpp`.
- Namespace enclosure follows JAM format (`CODING.md:671-679`, `jam_RegExp.h:3-5,105-106`, `jam_RegExp.cpp:1-3`).
- Preconditions use `assert` (`CAROL.md` Code Contract, `CODING.md:693`).
- No comments and no doxygen before the audit (`CAROL.md` Code Hygiene).

## Framework Inventory

Full tables live in `PLAN-saw-0-tracker.md` §Framework Inventory. This part uses:

- **API:** A13 (`jam::RegExp`, `jam_core/text/jam_RegExp.h:54-58`, `jam_RegExp.cpp:15-33,307-313`), A19 (`jam::Array`, `at`, `size`, `resize`).
- **Pattern:** P16 (JAM namespace enclosure).
- **Names:** N12 (`findNext`, `getCaptures`).
- **Synthesis:** S4 (`.at()` on `jam::Array`), S10 (brace initialization, alternative tokens).
- **Absence:** none.

## Validation Gate

Each step is validated by COUNSELOR before proceeding to the next — against `MANIFESTO.md` (BLESSED), `NAMES.md`, `CODING.md`, and the locked PLAN decisions. @Auditor runs ONCE, after the final step — never per step.

## Steps

### Step 1: Declare `findNext` on `jam::RegExp`
**Scope:** `~/Documents/Poems/dev/jam/jam_core/text/jam_RegExp.h`
**Action:** In `class RegExp` (`jam_RegExp.h:54-58`), immediately after `jam::Array<std::optional<std::string_view>> getCaptures (std::string_view text) const;` (`jam_RegExp.h:58`), add:

```cpp
    jam::Array<std::optional<std::string_view>> findNext (std::string_view text, size_t offset) const;
```

Introduce no other symbols, types, or members.
**Inventory:** A13, A19, P16, N12.
**Validation:** COUNSELOR reads `jam_RegExp.h:50-65` and checks:
1. The declaration signature matches verbatim.
2. Zero comments or doxygen blocks were added (`CAROL.md` Code Hygiene).

### Step 2: Implement `findNext` and delegate `getCaptures` in `jam_RegExp.cpp`
**Scope:** `~/Documents/Poems/dev/jam/jam_core/text/jam_RegExp.cpp`
**Action:** Replace `RegExp::getCaptures` (`jam_RegExp.cpp:15-33`) with:

```cpp
jam::Array<std::optional<std::string_view>> RegExp::getCaptures (std::string_view text) const
{
    return findNext (text, 0);
}

jam::Array<std::optional<std::string_view>> RegExp::findNext (std::string_view text, size_t offset) const
{
    assert (offset <= text.size());

    for (size_t start { offset }; start <= text.size(); start = getNextPosition (text, start, true))
    {
        Captures captures;
        captures.resize (groupNames.size());

        const MatcherContinuation continuation { [text, start, &captures] (size_t end)
        {
            captures.at (0) = text.substr (start, end - start);
            return true;
        } };

        if (isMatch (root, text, start, true, captures, continuation))
            return captures;
    }

    return Captures();
}
```

Termination verified at `jam_RegExp.cpp:307-313`: when `start == text.size()`, `getNextPosition (text, start, true)` returns `start + 1`. Because `start` indexes into `text`, `^` matches only at `position == 0` and `captures.at (0)` is a `std::string_view` slice of `text`.
**Inventory:** A13, A19, P16, N12, S4, S10.
**Validation:** COUNSELOR reads `jam_RegExp.cpp:10-45` and checks:
1. `assert (offset <= text.size());` is the precondition.
2. `getCaptures` delegates in one line to `findNext (text, 0)`.
3. `captures.at (0)` uses `.at()` (`S4`) and brace initialization `size_t start { offset }` (`S10`).
4. Zero comments or doxygen blocks were added.

## BLESSED Alignment

- **B — Bound:** `findNext` returns `std::string_view` slices into the caller's `text`; zero heap copy of input.
- **L — Lean:** `getCaptures` delegates to `findNext (text, 0)` in one line; no duplicate loop.
- **E — Explicit:** `assert (offset <= text.size())` states the precondition; slot `0` holds the full match view.
- **S — SSOT:** One matching loop serves `hasMatch`, `getCaptures`, and multi-match line scanning.
- **S — Stateless:** `findNext` is `const` and mutates no member of `RegExp`.
- **E — Encapsulation:** Callers pass `text` and `offset` without touching AST nodes or continuations.
- **D — Deterministic:** For any `(text, offset)`, `findNext` steps forward deterministically via `getNextPosition`.

## Risks / Open Questions

None.
