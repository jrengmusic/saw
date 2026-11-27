# RFC — Exclusive SAW: Remaining File, Discovery, Stream, and Append Capabilities
Date: 2026-11-27
Status: Ready for COUNSELOR handoff
Author: MACHINIST
Targets: `saw` (`~/Documents/Poems/dev/saw`), CAROL gate (`~/.carol/hooks/saw-gate.mjs`)

## Problem Statement

CAROL now enforces whole-session `saw` usage across Claude Code, `agy`, and OpenCode for `sed`, `awk`, `grep`, `rg`, `jq`, `cat`, `bat`, `head`, `tail`, `cut`, `Edit`, `Write`, and `Grep` (`~/.carol/hooks/saw-gate.mjs:1-53`).
Agents still fall back to shell utilities for seven recurring file and text-stream tasks that `saw 0.0.1 (c8865a6)` does not yet cover, plus one crash in `saw page` on piped stdin with TTY stdout.

ARCHITECT directed this RFC verbatim:
- *"any file tools (sed, awk, grep, rg, find, jq, Write, Read) could be exclusive to saw MCP first. probably the missing feature now is tail -F, that could be implemented later on saw engine."*
- *"what feature actually missing that could be implemented so we could use exclusive saw, for example - find with metadata - tail - etc"*
- *"1, 2, 3 append / write RFC to saw repo"*

Scope covers:
- Group 1: `saw page` piped-stdin crash fix, live follow (`tail -f` / `tail -F`), and file discovery with metadata (`find` / `fd` / `ls -lt` / `stat`).
- Group 2: counts (`saw count`), line sorting and deduplication (`saw sort`), two-file diff and byte check (`saw diff`), and field ranges in `saw fields`.
- Group 3 (append only): append mode (`saw write --append`). Atomic `mv`/`cp`/`rm` are explicitly out of scope.

## Research Summary

### 1. Observed `saw 0.0.1 (c8865a6)` defects

1. **`saw page` crashes on piped stdin when stdout is a TTY (exit 139 / SIGSEGV):**
   - Command: `printf 'a\nb\nc\n' | saw page -r 1:2` (PTY stdout) → exit `139`.
   - Command: `printf 'a\nb\nc\n' | saw page -r 1:2 -` (PTY stdout) → exit `139`.
   - Command: `printf 'a\nb\nc\n' | saw page -r 1:2 -P --color=never --decorations=never` → exit `0`, prints `a` and `b`.
   - Command: `printf 'a\nb\nc\n' | saw page -r 1:2 | wc -l` (piped stdout) → exit `0`, prints `2`.
   - Root path: `isRenderedPage` (`Source/Page.cpp:191-196`) returns true on CLI TTY stdout, routing into `getRenderedOutput` (`Source/Page.cpp:645-646`) and `getPageOutput` (`Source/Page.cpp:602`). The Lua test suite (`test/run.lua`) runs with non-TTY pipes, so it never exercised this branch.

2. **`saw fields` rejects range specs:**
   - Command: `printf 'a b c\n' | saw fields 1-2` → `saw: invalid field spec: 1-2`, exit `2`.
   - Root path: `getFieldIndices` (`Source/Fields.cpp:27-51`) splits only on commas and parses each token as a single integer via `getParsedFieldIndex` (`Source/Fields.cpp:11-25`).

### 2. Current `saw` capabilities and gaps

- **Live follow (`tail -f` / `tail -F`):** `saw page -r -<N>:` replaces static `tail -n <N>`. No follow or rotation-reopen mode exists. `~/.carol/hooks/saw-gate.mjs:15` keeps `TAIL_FOLLOW_PATTERN` as the sole `tail` exemption.
- **File discovery with metadata:** `Paths::getFiles` (`Source/Paths.h:40-44`) filters only by `includeGlobs`, `excludeGlobs`, `isIncludingHidden`, and `isIgnoringRules`. It returns `Entry = std::tuple<std::string, juce::File, bool>` (`Source/Paths.h:12`) sorted by display path. It has no depth limit, entry-type filter (`file`, `dir`, `symlink`), modification-time filter, size filter, sort key (`mtime`, `size`, `name`), reverse flag, or JSON metadata output (`--json`).
- **Counts (`wc`):** No verb prints line, word, and byte counts for files or stdin.
- **Sorting and deduplication (`sort` / `uniq`):** No verb sorts lines, removes duplicates, or counts duplicate runs.
- **Two-file diff and byte check (`diff` / `cmp`):** `LineDiff` (`Source/LineDiff.h:10-68`) already implements Myers line diff (`LineDiff (const TextDocument& original, const TextDocument& target)`) and unified diff rendering. `saw page --diff` (`Source/Page.cpp:496-497`) only diffs a file against git HEAD; no CLI or MCP path diffs two arbitrary files or checks byte equality quietly (`cmp -s`).
- **Append mode (`saw write --append`):** `Write::getArguments` (`Source/Write.cpp:45-48`) accepts only `--apply` and `--expect`. `runWrite` (`Source/Write.cpp:18-43`) replaces the entire target file with `standardInput` through `Transaction`. Appending to a file today requires sending the full prior content plus new lines in `stdin`.

### 3. Harness and MCP constraints

- `saw --mcp` (`Source/ToolServer.cpp`) serves request-response tool calls over JSON-RPC stdio. An unbounded `--follow` call on MCP would block the tool call forever and must be rejected on the MCP lane with exit code 2.
- Binary media reads (images, PDF, audio, video) remain on native `Read` / `read_file` because LLM harnesses attach them as multimodal payloads.
- `~/.carol/hooks/saw-gate.mjs:124-131` (`getShellDenial`) runs in Node.js before shell execution on both macOS and Windows across Claude Code, `agy`, and OpenCode. Matching optional `.exe` suffixes covers Windows `find.exe` and `sort.exe` alongside MSYS2/Unix binaries.

## Principles and Rationale

- **Bounds:** Follow polling, directory depth, and field-range expansion enforce explicit bounds (no negative depth, no zero field index, no unbounded MCP follow).
- **Lean:** Reuse existing subsystems instead of duplicating engines:
  - `saw diff` reuses `LineDiff` (`Source/LineDiff.h:68`).
  - `saw write --append` reuses `Transaction` (`Source/Write.cpp:35-42`) so appends gain atomic swaps, checksum verification, and `saw undo` automatically.
  - Metadata discovery extends `Paths` (`Source/Paths.h`) rather than building a second walker.
- **Explicit:** Each new tool family is a dedicated verb (`saw count`, `saw sort`, `saw diff`) with one responsibility, clean `--help`, and direct MCP arguments. Mutating append still obeys the two-gate rule (`--apply --expect 1`) and previews the appended diff on dry-run.
- **SSOT:** `--json` is the single metadata format for `saw find --files` (composable directly with `saw json`). `SPEC.md`, `Source/HELP.md`, and `test/cases.md` hold the single behavior definition; `~/.carol/hooks/saw-gate.mjs` adds each denial only after `saw` passes its tests on both CLI and MCP lanes.
- **Stateless:** Read-only verbs (`count`, `sort`, `diff`, `find --files`) hold no persistent state.
- **Encapsulation:** Each verb exposes `getArguments` and `run` returning `Stdio::Output`, matching `Write`, `Fields`, and `Page`.
- **Deterministic:** Every walk, sort, count, diff, and raw-byte append produces identical output on macOS and Windows.

**Considered and rejected:**
- **Allowing `--follow` over MCP:** Rejected under Bounds and Deterministic; MCP tool calls must terminate. Follow is CLI-only (for human terminals and background watchdogs).
- **Folding `count` and two-file `diff` into `saw page`:** Rejected by ARCHITECT in favor of three dedicated verbs (`saw count`, `saw sort`, `saw diff`).
- **Adding a separate `--stat` TSV format alongside `--json`:** Rejected by ARCHITECT under SSOT (`--json` only).
- **Auto-inserting a newline on `saw write --append`:** Rejected by ARCHITECT under Deterministic (append exact raw bytes from `stdin`).
- **Adding `mv` / `cp` / `rm` in this RFC:** Excluded per ARCHITECT's explicit scope (*"1, 2, 3 append"*).

## Scaffold

Exact integration points in `saw` `Source/` and settled decisions:

1. **`saw page` piped-stdin crash (`Source/Page.cpp`):**
   - Reproduce under a PTY test harness (or forced `--decorations=always --color=always`) with piped stdin + `-r 1:2`.
   - Fix null/lifetime defect in the rendered path (`Source/Page.cpp:580-648`) and add a regression test in `test/cases.md`.

2. **Live follow (`Source/PageOptions.h`, `Source/Page.cpp`):**
   - Add `--follow` (`-f`, follow descriptor) and `--follow-name` (`-F`, reopen on rotation/truncation) to `PageOptions`.
   - Reject both flags when `!std::holds_alternative<Input::StandardInput> (input)` (MCP lane) with exit code 2.

3. **Metadata discovery (`Source/Paths.h`, `Source/Paths.cpp`, `Source/Find.cpp`):**
   - Extend walk options with `maxDepth`, `entryTypes` (`file`, `dir`, `symlink`), `mtime` filter, `size` filter, `sortKey` (`name`, `mtime`, `size`), and `isReverse`.
   - Add `--json` output mode on `saw find --files` emitting one JSON object per entry (`path`, `type`, `size`, `mtime`, `mode`, `target`) — `--json` only, composable with `saw json`.

4. **Field ranges (`Source/Fields.cpp:11-51`):**
   - Extend `getFieldIndices` to accept `A-B` (inclusive range, ascending or descending) and `A-` (open-ended to end of line, resolved per line against `tokens.size()`). Reject `0` and malformed bounds with `failFieldSpecInvalid`.

5. **Dedicated verb `saw diff` (`Source/LineDiff.h`, new `Source/Diff.h` / `Source/Diff.cpp`):**
   - Accept two file paths (or one path and `-` for stdin).
   - Quiet byte-equality mode (`-q` / `--quiet`, replaces `cmp -s`): compares raw bytes and emits nothing (exit 0 identical, 1 different, 2 error).
   - Default mode: constructs `TextDocument::fromTextFile` for both inputs, computes `LineDiff (original, target)`, and emits unified diff (exit 0 identical, 1 different, 2 error).

6. **Dedicated verb `saw count` (new `Source/Count.h` / `Source/Count.cpp`):**
   - Operate on file paths or stdin via `Input::create` / `Paths::getReadableFiles`.
   - Emit line (`-l`), word (`-w`), and byte (`-c`) counts, defaulting to all three.

7. **Dedicated verb `saw sort` (new `Source/Sort.h` / `Source/Sort.cpp`):**
   - Operate on file paths or stdin via `Input::create` / `Paths::getReadableFiles`.
   - Support UTF-8 byte order (default), numeric (`-n`), reverse (`-r`), unique (`-u`), and duplicate counts (`-c`).

8. **Append mode (`Source/Write.cpp:18-65`):**
   - Add `Id::appendOption` (`--append`, `-a`) to `Write::getArguments`.
   - When `isExisting` and `--append` are set, concatenate `originalDocument->getBytes()` with the exact raw bytes of `standardInput->getBytes()` (no synthetic newline inserted), then pass through `Transaction::addFile` and `transaction.run (arguments)` (dry-run shows only the appended diff; `--apply --expect 1` commits atomically and records an undo entry).

9. **MCP semantic status (`Source/ToolServer.cpp:115-120`, `Source/Stdio.h:14-20`, `test/run.lua`):**
   - Replace numeric `"exitCode"` in the MCP JSON response with `"status"`: `"ok"` (`exitSuccess`, 0), `"mismatch"` (`exitMismatch`, 1), or `"error"` (`exitError`, 2+ / runtime errors).
   - CLI keeps the implicit OS integer exit code for shell operators (`&&`, `||`, `$?`), clean pipe-safe stdout, and stderr messages on error.
   - Update `SPEC.md` and `test/run.lua` MCP lane to check `"status"` instead of `"exitCode"`.

10. **Downstream CAROL gate expansion (`~/.carol/hooks/saw-gate.mjs`):**
   - After each corresponding `saw` suite passes on both CLI and MCP lanes, add `find`, `wc`, `sort`, `uniq`, `diff`, `cmp` to `SESSION_TOOLS` (with optional `.exe` matching for Windows `find.exe` / `sort.exe`), add their per-tool rows to `SAW_ROWS`, and remove `TAIL_FOLLOW_PATTERN`.

## BLESSED Compliance Checklist
- [ ] Bounds  - [ ] Lean  - [ ] Explicit  - [ ] SSOT
- [ ] Stateless  - [ ] Encapsulation  - [ ] Deterministic

## Open Questions

None.

## Handoff Notes

- **Baseline commit:** `saw 0.0.1 (c8865a6)` passes both `nvim --headless -l test/run.lua ~/.local/bin/saw` (CLI lane) and `... mcp` (MCP lane) with exit 0.
- **CAROL enforcement state:** `~/.carol` (`4cfe1a3`) and `~/.config` (`1843123`) enforce whole-session `saw` across Claude Code (`190/190` pass), `agy` (validated), and OpenCode (`196/196` pass).
- **Settled ARCHITECT rulings (all 5 decisions closed):**
  1. Three dedicated verbs: `saw count`, `saw sort`, `saw diff`.
  2. `--json` only for `saw find --files` metadata (`path`, `type`, `size`, `mtime`, `mode`, `target`).
  3. `saw write --append` appends exact raw bytes from `stdin`.
  4. Whole-session cross-platform denial of `find`, `wc`, `sort`, `uniq`, `diff`, `cmp`, and `tail -f/-F` (with `.exe` matching on Windows) in `~/.carol/hooks/saw-gate.mjs` once each `saw` suite passes.
  5. Replace MCP `"exitCode"` with `"status"` (`"ok"`, `"mismatch"`, `"error"`). CLI keeps implicit integer exit codes for shell operators plus clean stdout and stderr error messages.
- **Suggested execution order for COUNSELOR:**
  1. Fix `saw page` piped-stdin TTY crash (`Source/Page.cpp`) first — live SIGSEGV defect blocking piped `saw page -r`.
  2. Implement `saw fields` ranges, `saw write --append`, and MCP `"status"` (`Source/ToolServer.cpp`) — smallest diffs on existing code.
  3. Implement `saw find --files` metadata + `--json`, `saw count`, `saw sort`, `saw diff`, and `saw page --follow` / `--follow-name`.
- **Downstream handoff:** After `saw` ships and passes CLI + MCP lanes on macOS and Windows, MACHINIST updates `~/.carol/hooks/saw-gate.mjs` and removes `TAIL_FOLLOW_PATTERN`.
