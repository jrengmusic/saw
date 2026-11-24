# PLAN: SAW must live to its premise

**RFC:** RFC-live-to-premise.md
**Date:** 2026-11-24
**BLESSED Compliance:** verified
**Language Constraints:** C++17 / JUCE 8.0.17 + JAM. macOS and Windows.

## Context

ARCHITECT: *"saw, simple cli to replace sed, awk, grep, ripgrep, has its own formatter. this is carol in house tools enforced by protocol. use it always correct, always deterministic."*, *"cli isn't enough? need lazy load mcp that never inject anything when idle? never eat context when not being used? always on demand? fine, add one. what the fuck is module for?"*, *"mcp isn't new pattern, module is."*, *"do we need all? why are we handrolling everything from macos lib? why not crossplatfrom using juce? and jam?"*, *"crossplatfrom, must be fastest. don't just hack and patch."*

This plan replaces PLAN-live-to-premise.md. The RFC work of the earlier plan (D1–D8, R1–R9, L1–L10, N3, N8, S3–S6, T) is in the tree and does not change here.

## Overview

1. `saw` is the CLI. `saw_ToolServer` is the MCP server. The shell module is deleted.
2. One file mechanism for macOS and Windows, as SPEC:104 rules: *"JAM first, then JUCE, then the C++17 standard library."*
3. Each document line that COUNSELOR added against a quoted ruling is deleted.

**The launch limit.** The RFC measured it (RFC:85-87), and this session measured it again. A new process of any program that Apple does not ship loses to Apple's `sed` and `grep` on small files and head rows: an empty C program costs 10.53M instructions, `sed 20q` costs 9.25M. The server starts no process for each call and beats every reference in every row of the two sets that ran (SPRINT-LOG:178). This plan does not win the CLI small-file rows and writes no exemption for them. The final report lists each lost row.

**Build ruling:** *"build permission granted. dont abuse buid for each turn code implementation, dont audit witohut runtime test. only use use build for final validation"*. Two builds. `cast cast/spell.md --no-sign`, foreground, by Pathfinder.

**Endpoint:** the report of Step 9. No commit message (*"DONT FUCKING GIVE MEE COMMIT MESSAGE"*). The log waits for ARCHITECT's /log.

**Not in this plan:** CAROL wiring (MCP registration, hooks, agent tool lists). SPEC:850: *"carol init on saw repo by ARCHITECT"*, *"The SAW implementation plan does not include it."* Step 9 gives the exact entries in chat.

## Decided by measurement and reads (this session)

| Fact | Evidence | Decision |
|---|---|---|
| No other in-house tool has a shell module. MCP is the pattern | `scout/Source/Main.cpp:262`, `whatdbg/Source/Main.cpp:262`, `ling/Source/Main.cpp:302` | Step 2 |
| MCP needs `juce_events`, `jam_data_structures`, `jam_terminal`, which include `juce_gui_basics`. Frameworks add 0.9–1.2 ms to each start | `jam_model_context_protocol.h:10-15`, `jam_data_structures.h:33`, `jam_terminal.h:54`, RFC:67-71 | The server stays a separate binary |
| C++17 walk 5.6 ms, native walk 5.2 ms (3,077 files, one thread). Tree rows win by 3.0 ms (`findLiteral`) and 2.1 ms (`findFiles`) | `forms/results.txt`, `matrix-tree-table.md` | Step 6 |
| Checked read 9.9 µs against 8.1 µs at 1K, 26.1 against 24.7 at 100K | `forms/results.txt` | Steps 4, 5 |
| Whole-file parallel read at 100M: +1.4 ms against the map. `findContext` 100M wins by 1.0 ms | `forms/results2.txt`, `matrix-large-table.md` | The map stays |
| `juce::MemoryMappedFile` read-only maps `MAP_SHARED`. A file that is cut short ends the process with signal 10. Only `MAP_PRIVATE \| MAP_RESILIENT_MEDIA` gives no fault | `juce_SharedCode_posix.h:587-590`, earlier plan Step 13b measurement | Step 3 |
| A standard input file read in place of mapped: 100M rows near `rg` (pipe form 23.9 ms, `rg` 22.8 ms) | `matrix-input-table.md:172-187` | stdin map stays (Absence) |
| `juce::File::moveFileTo` deletes the target first; `createDirectory` fails in a race; `deleteFile` returns true for a missing file, and `UndoCache.cpp:475` needs false | `juce_File.cpp:300-315`, `:531-547`, `juce_SharedCode_posix.h:395-407`, SPRINT-LOG:171 | `jam::File` keeps its C++17 forms |

## Language / Framework Constraints

- LANGUAGE.md C++/JUCE: one object, one translation unit. No `.cpp` is added or split.
- Generated files (`CMakeLists.txt`, `Source/generated/*`) come from cast only.
- No comment and no doxygen before the post-audit pass.

## Framework Inventory

### API

| Need | Module | API | Source | Step |
|---|---|---|---|---|
| Read a file | juce_core | `juce::FileInputStream`, `InputStream::read (void*, size_t)` | `juce_FileInputStream.h:56-87`; use `Input.cpp:38-41` | 4 |
| Map a file | juce_core | `juce::MemoryMappedFile (file, readOnly)` | `juce_MemoryMappedFile.h:72-106`; use `Input.cpp:29` | 4 |
| File type | C++17 | `std::filesystem::status` | `jam_core.h:50` includes it | 4 |
| List a folder | C++17 | `std::filesystem::directory_iterator` | same | 6 |
| Path from `juce::File` | C++17 | `std::filesystem::u8path (file.getFullPathName().toStdString())` | `jam_File.cpp:147` | 4, 6 |
| JUCE patch lane | jam | `juce-posix-file-hook.patch`, row in the patch table | `jam/user-modules-info.md:1421`; `jam/patch/juce-posix-file-hook.patch:8-16` | 3 |
| Config macro table | saw | `## macro` row | `project-info.md:213` | 3 |

### Pattern usage

| Pattern | Exemplar | Use sites | Step |
|---|---|---|---|
| One reader behind a view | `Input::fromFile` | `TextDocument.cpp:84`; Search reads with its own code at `Search.cpp:194-253` | 4, 5 |
| JUCE config macro, default keeps every project | `JUCE_USE_FOUNDATION`, patch lines 10-16 | `project-info.md:213` | 3 |
| Failure from a null input | `TextDocument::getInput`, `TextDocument.cpp:80-90` | `Search.cpp:139-142` (`getFailedSearch`) | 5 |
| Folder listing as names and entries | `Paths.cpp:186-218` | `Paths.cpp:401`, `:456` | 6 |

### Names

| Family | Members | Intent | Step |
|---|---|---|---|
| JUCE config macros | `JUCE_USE_FOUNDATION`, `JUCE_USE_CURL`; new `JUCE_USE_RESILIENT_MAPPING` | feature switch | 3 |
| saw macro rows | `juceUseFoundation`, `juceUseCurl`; new `juceUseResilientMapping` | names the macro | 3 |

No other name is added.

### Synthesis

| Rule | Established form | Exemplar |
|---|---|---|
| Use the framework API fully | JUCE stream and map behind `Input` | `Input.cpp:29`, `:38` |
| No duplicate logic (SSOT) | one reader, one listing | Steps 4–6 |
| No bail-out guards | result returns | `Input.cpp:104-105` |
| Delete first, implement after | CAROL Refactor-Rewrite | Steps 2, 4–6 |

### Absence

| Need | Query | Result |
|---|---|---|
| File read, map or listing in jam_core | doxygen index, `jam_File.h` declarations | none (inventory, section 1) |
| File type in JUCE | `juce_File.h:116-132`, `juce_SharedCode_posix.h:257-260` | `existsAsFile` accepts a named pipe |
| Entry type in JUCE listing | `juce_RangedDirectoryIterator.h:49-83` | folder flag only, and a status call for each entry |
| Map of a descriptor (standard input) | `juce_MemoryMappedFile.h:72-96`, C++17 | file paths only |
| Atomic move over a target, folder creation in a race | `juce_File.cpp:300-315`, `:531-547` | none; `jam::File` owns them |

## Validation Gate

Each step is validated by COUNSELOR before proceeding to the next — against
MANIFESTO.md (BLESSED), NAMES.md, ~/.carol/CODING.md, and the locked PLAN decisions
(no deviation, no scope drift). @Auditor runs ONCE, after the final step, covering
the whole sprint — never per step.

## Steps

### Step 1: Documents
**Scope:** `PLAN-live-to-premise.md`, `SPEC.md`, `CLAUDE.md`
**Action:** COUNSELOR writes this plan to `PLAN-live-to-premise.md`. SPEC: delete lines 79, 783-795 and 993; delete the shell part of 907 and 944; rewrite 84 and 86 to one mechanism; line 63 keeps ARCHITECT's quote and gains the new MCP quote; §5.8 gains the same quote. The truncation sentence of 793 moves to the map line of §2, without the shell text. CLAUDE.md: delete line 22. Gate lines 991 and 992 do not change: both carry *"FASTER on ALL CASES"*.
**Inventory:** none
**Validation:** each changed line carries ARCHITECT's words or a cited fact; no gate line is weakened.

### Step 2: Delete the shell module
**Scope:** `shell/` (8 files), `cast/cmake.cast`, `test/run.lua`, `benchmark/matrix-rows.sh`, `Source/CommandLine.h`, installed `~/.local/bin/saw_ShellModule.so`, `.sh`, `.old`
**Action:** Delete `shell/` and the three installed files. In `cmake.cast` delete the source block (334-353), the target (355-392), the install block (394-402), and lines 424, 475, 487; line 407 keeps the two other targets. In `run.lua` delete the shell arguments and branch (lines 2, 5-7, 512); the server branch stays. In `matrix-rows.sh` delete `module`, `runShell`, and each shell row (lines 10, 96-105, 219, 241, 250, 257, 264, 269, 283-288, 301-306, 351-353, 366-368, 391, 415, 420, 423). `CommandLine.h:7` loses the words that name the module. `Source/StandardStream.*`, `Stdio::Output` and the `input` parameters stay: the CLI and the server use them.
**Inventory:** none
**Validation:** no `ShellModule`, `HostRuntime` or `runShell` in the project outside `carol/SPRINT-LOG.md`, the RFC and the reports.

### Step 3: Resilient read-only map in JUCE
**Scope:** `jam/patch/juce-posix-file-hook.patch`, `jam/user-modules-info.md` (description cell of its row), `project-info.md`
**Action:** The patch adds `JUCE_USE_RESILIENT_MAPPING` (default 0) beside `JUCE_USE_FOUNDATION`. With 1 on macOS, `MemoryMappedFile::openInternal` maps a read-only file with `MAP_PRIVATE | MAP_RESILIENT_MEDIA`. Every other case keeps its flags. saw sets it to 1 in the `## macro` table, row `juceUseResilientMapping`.
**Inventory:** API patch lane, macro table; Names JUCE config macros
**Validation:** `git apply --check` clean against the canonical JUCE copy in scratch; with the default, the preprocessed code is the same as today.

### Step 4: One reader
**Scope:** `Source/Input.h`, `Source/Input.cpp`
**Action:** Delete `ScopedFileDescriptor`, `Input::fromDescriptor`, and both bodies of `Input::fromFile`. Write one `fromFile` for both systems. `std::filesystem::status` gives the type. Regular: size from `juce::File::getSize`; above `maxReadSize` it is `juce::MemoryMappedFile (file, readOnly)`, otherwise `juce::FileInputStream` into `buffer` with `InputStream::read`. Folder, named pipe, socket, missing: nullptr. Any other type: one `juce::FileInputStream` byte, 0 bytes gives an empty Input, else nullptr, as `ScopedFileDescriptor::getSize` did. `mappedFile` loses its `#ifdef`. `setMappedBytes`, `isStream` and the stdin map stay (Absence).
**Inventory:** API read, map, file type, path; Pattern one reader; Synthesis
**Validation:** no `::open`, `::fstat` on a path, or `::read` on a path in `Input.cpp`; one `fromFile`.

### Step 5: Search uses the one reader
**Scope:** `Source/Search.cpp`
**Action:** Delete both bodies of `Search::runFile` reading (194-253) and the `#ifndef _WIN32` includes. One body: the file is `juce::File::getCurrentWorkingDirectory().getChildFile (…)`, through `Paths::getLinkedTarget` when explicit; `Input::fromFile (file, Input::maxBufferedReadSize)`; nullptr → `getFailedSearch`; empty → `getEmptyFileOutcome`; else `run`.
**Inventory:** Pattern one reader, failure from a null input
**Validation:** no platform branch in `Search.cpp`; the messages and exit codes of the runner cases.

### Step 6: One folder listing
**Scope:** `Source/Paths.cpp`
**Action:** Delete `getEntryFlags`, both `getDirectoryListing` bodies, and the POSIX includes. One body with `std::filesystem::directory_iterator` and `std::error_code`: an open error gives `nullopt`, as today; symbolic links are skipped; folder and regular-file entries are kept with the same names, flags and `.gitignore` rule. Line 369 (`getDisplayPath` on Windows) stays.
**Inventory:** API list a folder, path; Pattern folder listing
**Validation:** same signature and result; `getFileNames` and the walk unchanged.

**Amendment to Steps 4 to 6 (audit and Step 7 measurements):**
- `std::filesystem::file_size` gives type and size in one call (`status` and `getSize` cost 0.95 ms more on the tree). The null device is compared with `std::filesystem::equivalent`; no other device is opened, so none can block.
- `Paths::getLinkedTarget (std::string_view)` uses `std::filesystem::canonical`. The earlier form removed `..` as text, so `<link>/..` named another file, also for the verbs that write.
- `Paths::getFiles` resolves each path one time and gives the resolved file. `Paths::applyToDirectories` walks all roots in one walk.
- Build 3 followed the two regressions that the rows of Step 9 showed (tree replace preview, several folders).

### Step 7: Build 1, correctness, touched rows
**Scope:** saw, jam patch lane
**Action:** Pathfinder copies today's `~/.local/bin/saw` and `saw_ToolServer` to scratch, builds one time, then runs:
1. The runner on the CLI and on the server: 173 PASS, 0 FAIL each.
2. `saw find` on a named pipe path gives its failure, with a time limit of 5 s, and no hang.
3. A writer that cuts a 100 MB file short during `saw find` in a loop: no signal exit.
4. One scout script, one run, medians, output equal to the reference first. Rows: `findLiteral`, `findFiles`, `findNoMatch` tree; `findLiteral` 1K, 100K, 10M, 100M; `findContext` and `findCount` 100M; `lines`, `fields`, `replacePreview` 100M; `findInput` 100M; server `findLiteral` 1K, 100K. Each row: the reference, today's copy, the new build.
A row that wins today and loses now is read, fixed, and rebuilt inside this step.
**Inventory:** all
**Validation:** items 1 to 4; `otool -L` shows `libc++` and `libSystem` only.

### Step 8: Audit, resolve, doxygen
**Scope:** files of Steps 2 to 6
**Action:** @Auditor once. COUNSELOR filters each finding; @Engineer resolves. Then Pathfinder writes the doxygen of `Input.h`, `Search.h`, `Paths.h`, `CommandLine.h`; COUNSELOR reads each block against the code.
**Inventory:** all
**Validation:** 0 open findings.

### Step 9: Build 2, final check, report
**Scope:** saw and jam
**Action:** One build. The runner on both lanes and the rows of Step 7 again. The chat report gives: the numbers, each CLI row that loses to Apple's tools, and the wiring entries for ARCHITECT: `"saw": { "command": "saw_ToolServer" }` beside `ling`, `whatdbg`, `scout` in `~/.claude.json`, `mcp__saw__*` in the agent tool lists, and the SPEC §7 hook rows.
**Inventory:** all
**Validation:** the same results as Step 7.

## Verification

Steps 7 and 9. Windows: ARCHITECT builds and runs the runner. The Windows code is read here, not run (R15: *"NO FUCKING BLOCKER"*).

## BLESSED Alignment

- **B:** `Input` owns its buffer or its map; no raw descriptor for a path remains.
- **L:** one reader, one listing, no module.
- **E (Explicit):** each read failure keeps its message.
- **S (SSOT):** the same job is written one time for both systems; one engine behind CLI and server.
- **S (Stateless):** the server holds nothing between calls.
- **E (Encapsulation):** platform behavior stays in JUCE and the patch lane.
- **D:** same output on both systems from the same code.

## Risks / Open Questions

- The type check and the open are two calls. A regular file that another process replaces with a named pipe between them would block, as it does today in `grep` and `sed`. No other open point.
