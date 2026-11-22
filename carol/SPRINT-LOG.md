# SPRINT-LOG.md

**Project:** saw  
**Repository:** /Users/jreng/Documents/Poems/dev/saw  
**Started:** 2026-11-22

**Purpose:** Long-term context memory across sessions. Tracks completed work, debts paid, and debts deferred to the inter-sprint ledger (`DEBT.md`). Written by PRIMARY agents only when ARCHITECT explicitly requests.

---

## 📖 Notation Reference

**[N]** = Sprint Number (e.g., `1`, `2`, `3`...)

**Sprint:** A discrete unit of work completed by one or more agents, ending with ARCHITECT approval ("done", "good", "commit")

---

## ⚠️ CRITICAL RULES

**AGENTS BUILD CODE FOR ARCHITECT TO TEST**
- Agents build/modify code ONLY when ARCHITECT explicitly requests
- ARCHITECT tests and provides feedback

**AGENTS NEVER RUN GIT COMMANDS WITHOUT ARCHITECT'S EXPLICIT INSTRUCTION**
- Write code changes without running git commands
- Agent runs git ONLY when user explicitly requests
- Never autonomous git operations
- One exception: `git log` for a history question that bears on the task (CAROL.md Git)
- **When committing:** Always stage ALL changes with `git add -A` before commit
  - ❌ DON'T selectively stage files (agents forget/miss files)
  - ✅ DO `git add -A` to capture every modified file

**SPRINT-LOG WRITTEN BY PRIMARY AGENTS ONLY**
- **COUNSELOR** writes to SPRINT-LOG
- Only when user explicitly says: `"log sprint"`
- No intermediate summary files
- No automatic logging after every task
- Latest sprint at top, keep last 5 entries

**NAMING RULE (CODE VOCABULARY)**
- All identifiers must obey project-specific naming conventions (see NAMES.md)
- Variable names: semantic + precise (not `temp`, `data`, `x`)
- Function names: verb-noun pattern (initRepository, detectCanonBranch)
- Struct fields: domain-specific terminology (not generic `value`, `item`, `entry`)
- Type names: PascalCase, clear intent (CanonBranchConfig, not BranchData)

**BEFORE CODING: ALWAYS SEARCH EXISTING PATTERNS**
- ❌ NEVER invent new states, enums, or utility functions without checking if they exist
- ✅ Always grep/search the codebase first for existing patterns
- ✅ Check types, constants, and error handling patterns before creating new ones
- **Methodology:** Read → Understand → Find SSOT → Use existing pattern

**TRUST THE LIBRARY, DON'T REINVENT**
- ❌ NEVER create custom helpers for things the library/framework already does
- ✅ Trust the library/framework - it's battle-tested

**FAIL-FAST RULE (CRITICAL)**
- ❌ NEVER silently ignore errors (no error suppression)
- ❌ NEVER use fallback values that mask failures
- ❌ NEVER return empty strings/zero values when operations fail
- ❌ NEVER use early returns
- ✅ ALWAYS check error returns explicitly
- ✅ ALWAYS return errors to caller or log + fail fast

**⚠️ NEVER REMOVE THESE RULES**
- Rules at top of SPRINT-LOG.md are immutable
- If rules need update: ADD new rules, don't erase old ones

---

## Quick Reference

### For Agents

**When user says:** `"log sprint"`

1. **Check:** Did I (PRIMARY agent) complete work this session?
2. **If YES:** Write sprint block to SPRINT-LOG.md (latest first)
3. **Include:** Files modified, changes made, alignment check, debts paid, debts deferred
4. **Hygiene:** After writing the SPRINT-LOG entry, drain paid debts from project-root `DEBT.md` via `carol debt clear <id>` for each ID under *Debts Paid*. Receipt first, then clean the books.

### For User

**Activate PRIMARY:**
```
"@CAROL.md COUNSELOR: Rock 'n Roll"
```

**Log completed work:**
```
"log sprint"
```

**Invoke subagent:**
```
"@oracle analyze this"
"@engineer scaffold that"
"@auditor verify this"
```

**Available Agents:**
- **PRIMARY:** COUNSELOR (domain specific strategic analysis)
- **Subagents:** Pathfinder, Engineer, Auditor, Librarian

---

<!-- SPRINT HISTORY STARTS BELOW -->
<!-- Latest sprint at top, oldest at bottom -->
<!-- Keep last 5 sprints, rotate older to git history -->

## SPRINT HISTORY

<!-- The entry format is in /log (commands/log.md). It is the only source. -->
<!-- Sprint entries go here, written by PRIMARY agents -->

## Sprint 1: SAW v0.0.1 Implementation, Optimization, Audit, and Doxygen ✅

**Date:** 2026-11-23
**Duration:** 10:50

### Agents Participated
- Primary: COUNSELOR — planned and orchestrated Parts 1–9, validated all Engineer diffs, filtered Auditor findings, synced `SPEC.md`, and logged Sprint 1.
- Discovery & Profiling & Doxygen: Pathfinder — inspected JAM/JUCE owner APIs, ran all 16 sequential `scout` benchmarks, diagnosed hot-path bottlenecks, and completed the post-audit Doxygen pass (0 warnings).
- Implementation: Engineer — extended `jam::RegExp::findNext`, built the SAW skeleton and all 6 verbs (`find`, `replace`, `lines`, `fields`, `patch`, `undo`), authored the 91-case Lua test suite and 16 `scout` benchmarks, implemented the SIMD/parallel/fused-walk optimizations, and resolved all accepted Auditor findings.
- QA/QC: Auditor — ran a wholesale audit across Parts 1–9 against `MANIFESTO.md`, `CODING.md`, `NAMES.md`, and `SPEC.md`.

### Files Modified (42+ files across `saw` and `jam`)
- `/Users/jreng/Documents/Poems/dev/jam/jam_core/text/jam_RegExp.h:60-69`, `jam_RegExp.cpp:15-40` — added offset-based `findNext` returning capture views and delegated `getCaptures` to `findNext (text, 0)`.
- `Source/Main.cpp`, `Source/Stdio.h/.cpp`, `Source/Arguments.h/.cpp`, `Source/Jobs.h` — CLI entrypoint, UTF-8 `Stdio::getFormatted`, raw UTF-8 `jam::Strings` argument parser with `getLineRangeFilter`, and lock-free `Jobs::run`.
- `Source/TextDocument.h/.cpp`, `Source/Pattern.h/.cpp`, `Source/IgnoreRules.h/.cpp`, `Source/Paths.h/.cpp` — single-scan `TextDocument` with `fromTextFile`, zero-alloc SIMD `Pattern` with required-literal regex prefilter and anchored exact regex, zero-alloc `IgnoreRules`, and parallel `Paths::applyToDirectory`.
- `Source/Find.h/.cpp`, `Source/Fields.h/.cpp`, `Source/Lines.h/.cpp` — fused single-pass parallel walk+search with POSIX buffered read and deferred NUL check in `Find`, zero-alloc streaming `Fields`, and early-stopping streaming `Lines`.
- `Source/LineDiff.h/.cpp`, `Source/Writer.h/.cpp`, `Source/UndoCache.h/.cpp`, `Source/Transaction.h/.cpp`, `Source/Replace.h/.cpp`, `Source/PatchValidator.h/.cpp`, `Source/Patch.h/.cpp`, `Source/Undo.h/.cpp` — GNU-exact unified diff with shared `getLinesPreservingEmpty`, atomic `juce::TemporaryFile` writes, 32-entry rolling undo cache, two-phase `Transaction`, and full `replace`, `lines` mutate, `patch`, and `undo` verbs.
- `project-info.md`, `cast/{identifiers,text,files,banner}.md`, `cast/source.cast`, `Source/HELP.md`, `Source/help/*.md`, `SPEC.md` — `cast` vocabulary/codegen, help pages, and `SPEC.md` §2/§4.4 sync; deleted all 10 completed `PLAN-saw-*.md` files.
- `test/run.lua`, `test/cases.md`, `test/{fixture,stdin,baseline}/`, `benchmark/*.sh`, `benchmark/*.md` — 91-case Neovim Lua comparison/baseline test suite and 16 `scout` benchmark manifests + reports.

### Alignment Check
- [x] BLESSED principles followed
- [x] NAMES.md adhered
- [x] MANIFESTO.md principles applied

### Problems Solved
- Replaced `juce::ArgumentList` with raw UTF-8 `jam::Strings` after verifying JUCE trims/drops empty arguments and mis-matches `-1` against `-h` (`juce_ConsoleApplication.cpp:122-127,152-156`).
- Added `Stdio::getFormatted` (two-pass `vsnprintf` + `fromUTF8`) after verifying `juce::String::formattedRaw` double/triple-encoded UTF-8 em-dash (`juce_String.cpp:1848`).
- Escaped `\|` in GFM help tables (`Source/help/{find,lines}.md`) to fix `jam::MarkdownDocument::parse` null dereference.
- Closed `saw` benchmark gaps via lock-free `Jobs::run`, zero-alloc SIMD `Pattern` + regex literal prefilter, streaming `Lines`/`Fields`, POSIX buffered read + deferred NUL check, and fused single-pass parallel walk+search: beats `sed` (-19.7% to -86.0%), `awk` (-74.0% to -76.4%), and `grep` (-79.9% to -88.4%), and reaches statistical parity with `rg` (+2.4%, `p = 0.082`) at 19–24% lower user CPU and 23–24% lower kernel CPU.
- Resolved Auditor Findings 1–3, 5–10 (anchored `-r -x` regex, `.at()` indexing, structured bindings, `.compare()` on `string_view`, `cast/text.md` error constants, `try*` verb removal, fused-walk error propagation, shared `LineDiff`/`Arguments`/`TextDocument`/`Paths` helpers, descriptive binding names); rejected Finding 4 rollback and synced `SPEC.md` to ratified fast paths.
- Completed post-audit comprehensive Doxygen pass across `jam_RegExp.h` and all 18 `saw/Source/*.h` headers (0 warnings).

### State for Continuation
- Release build (`cast cast/spell.md --no-sign`) passes with 0 warnings, 0 errors (`cast: Fine.`), installed at `~/.local/bin/saw`.
- Functional suite (`nvim --headless -l test/run.lua ~/.local/bin/saw`) passes 91/91 cases (exit `0`).
- All 16 `scout` benchmark reports are up to date under `benchmark/*-report.md`.
- None — sprint fully closed, nothing open.

### Debts Paid
- None

### Debts Deferred
- None

---

**End of SPRINT-LOG.md Template**

Copy this template to your project root as `SPRINT-LOG.md` and customize:
- Project name
- Repository URL/path
- Starting date
- Add project-specific rules to CRITICAL RULES section

Rock 'n Roll!  
**JRENG!**
