# RFC — Exclusive SAW: Remaining File, Discovery, Stream, and Append Capabilities
Date: 2026-11-27
Status: Ready for COUNSELOR handoff
Author: MACHINIST
Targets: `saw` (`~/Documents/Poems/dev/saw`), CAROL gate (`~/.carol/hooks/saw-gate.mjs`)

## Problem Statement

CAROL now enforces whole-session `saw` usage across Claude Code, `agy`, and OpenCode for `sed`, `awk`, `grep`, `rg`, `jq`, `cat`, `bat`, `head`, `tail`, `cut`, `Edit`, `Write`, and `Grep` (`~/.carol/hooks/saw-gate.mjs:1-53`).
Agents still fall back to shell utilities for seven recurring file and text-stream tasks that `saw 0.0.1 (c8865a6)` does not yet cover, plus one crash in `saw page` on piped stdin with TTY stdout. One more defect makes `saw --mcp` lose its connection when one response exceeds the client message limit (added 2026-11-28).

ARCHITECT directed this RFC verbatim:
- *"any file tools (sed, awk, grep, rg, find, jq, Write, Read) could be exclusive to saw MCP first. probably the missing feature now is tail -F, that could be implemented later on saw engine."*
- *"what feature actually missing that could be implemented so we could use exclusive saw, for example - find with metadata - tail - etc"*
- *"1, 2, 3 append / write RFC to saw repo"*
- *"i mean we need to be able to use saw deterministically, which side of fix should be? saw side? config side?"* (2026-11-28)
- *"add your findings to saw latest RFC-exclusive-saw.md"* (2026-11-28)
- *"i mean, broad find is fine, but still we must prevent agents stupidly fill context windows with unnecessary payload, this will hurt usage limit, and must be applied to CAROL consistently for all harness cli"* (2026-11-28)
- *"A"* (2026-11-28). This selects option A of three: `saw` bounds its own output on both lanes by default; at the bound it ends the output with a fixed trailer that states the printed size and the omitted size; a flag lifts the bound for one call; no gate change. Principles and Rationale lists options B and C.
- Rulings on the output bound (2026-11-28), verbatim answers:
  - Bound scope: *"One value, both lanes"*.
  - Trailer unit: *"lines, but it 16mb for payload guard is too big no? 1MB already too much CMIIW"*. Research Summary §3 confirms it: the Claude Code inline limits are 128K characters (Bash) and 500K (MCP).
  - MCP signal: *"what's the most deterministic? BLESSED compliance"*. The answer is the trailer in `output` only (Principles and Rationale).
  - Other client limits: *"Measure first"*.
  - Bound unit: *"Byte bound, line cut"*.
  - Bound value: *"32 KB"*.
  - Trailer text: *"Pass --no-limit hint"*, with the preview `saw: output limit — 412 line(s) printed, 18093 line(s) omitted. Pass --no-limit to print all.`
- *"Entry= tuple<string, File, bool> ?? this is smell, why not metadata, or pointer only? why must carry materialised data?"* (2026-11-28)
- *"can you also check if there's any similar pattern violation that using temporary containers of materialised data (not metadata nor pointer/reference)? that potentially could be refactor hence 0 rules could be applied adherence to contract and potentially reduced resource usage/boost performance"* (2026-11-28)
- *"fold into RFC, need to be planned"* (2026-11-28)

Scope covers:
- Group 1: `saw page` piped-stdin crash fix, live follow (`tail -f` / `tail -F`), and file discovery with metadata (`find` / `fd` / `ls -lt` / `stat`).
- Group 2: counts (`saw count`), line sorting and deduplication (`saw sort`), two-file diff and byte check (`saw diff`), and field ranges in `saw fields`.
- Group 3 (append only): append mode (`saw write --append`). Atomic `mv`/`cp`/`rm` are explicitly out of scope.
- Group 4 (added 2026-11-28): a default bound on the output size of every `saw` call, on the CLI lane and on the MCP lane.
- Group 5 (added 2026-11-28): materialised copies in transit. Replace each copy, temporary container, and fake carrier of Research Summary §4 with a reference, an index, a view, or metadata into the owner, under the 0 rules (MANIFESTO E, Complete at Creation).

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

3. **`saw --mcp` loses the connection when one response exceeds the client message limit (observed 2026-11-28, Claude Code):**
   - Call: MCP `run` with `["find", "takeover", "/Users/jreng/.carol", "/Users/jreng/.claude"]` and `directory` `/Users/jreng/.carol`. The tool result was `Connection closed`. Claude Code then listed `saw` as a failed server, and the session lost every `mcp__saw__*` tool.
   - Client log `~/Library/Caches/claude-cli-nodejs/-Users-jreng--carol/mcp-logs-saw/2026-11-27T17-16-52-075Z.jsonl`:
     - Line 4: `"negotiatedProtocolVersion":"2026-07-28"`. The handshake succeeded.
     - Line 7: `wrote >16MB to stdout without a JSON-RPC message boundary. The server is likely writing logs or other non-protocol data to stdout instead of stderr. Disconnecting to prevent unbounded memory growth.`
     - Line 9: `Tool 'run' failed after 3s: Connection closed`.
   - Replay outside the client: the same `tools/call` request, with `_meta` `io.modelcontextprotocol/protocolVersion` `2026-07-28`, piped to `saw --mcp`. Result: exit `0`, `3.04` s real time, `335396864` bytes maximum resident set size, and `30981792` bytes of output for the one request.
   - Cause: the search walked `~/.claude` (`7.7G`; `file-history` alone is `212M`), which holds session transcripts and file history. The one response was about 31 MB. The client guard closed the connection at 16 MB.
   - Root path: `ToolServer::runTool` (`Source/ToolServer.cpp:79-125`) gives all output documents from `CommandLine::run` to one `server.sendResponse` call (`Source/ToolServer.cpp:115-120`). No step limits the size of the response.
   - The CLI lane does not fail: the same search through CLI `saw find`, piped to `saw page -r :80`, completed in the same session.

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
- Claude Code applies a fixed 16 MB guard to each stdio message (client log line 7, Research Summary §1 item 3). The `saw` entry in `~/.claude.json` (`mcpServers.saw`) holds only `type`, `command`, `args`, and `env`. The config and the log name no setting for the guard.
- `saw` is the only side that knows the size of a response before it writes the response. A bound in `saw` gives one response shape in every harness that runs `saw --mcp`.
- The message limits of `agy` and OpenCode are not measured.
- The CAROL gate cannot bound a payload. Claude Code (`~/.carol/hooks/register.js:7-14`), `agy` (`~/.carol/agy/hooks/agy-gate.mjs:338`), and OpenCode (`~/.carol/opencode/plugins/carol/gate.js:3-7`) import one gate, `~/.carol/hooks/saw-gate.mjs`. Its `saw` checks, `getSawFinds` (`saw-gate.mjs:156-159`) and `getRunDenial` (`saw-gate.mjs:165-170`), read the arguments before the call runs. They never see the output size.
- Pi does not import `saw-gate.mjs` (`~/.carol/pi/extensions/carol-gate.ts`).
- The CLI lane has a harness bound in Claude Code only, as observed: the Claude Code Bash tool cut one `saw find` output on 2026-11-28 and added `[8954 characters truncated]`. The CLI bounds of `agy`, OpenCode, and Pi are not measured.
- An unbounded payload fills the agent context window and uses the usage limit (ARCHITECT, Problem Statement). The 16 MB disconnect is the extreme case of the same defect.
- The 16 MB guard protects the transport, not the context window. Claude Code keeps tool output inline only up to much smaller limits, and saves larger output to a file:
  - Bash: `bashOutputMaxChars` raises the inline limit *"up to 128K characters"* (`~/.claude/cache/changelog.md:5600`).
  - MCP: `_meta["anthropic/maxResultSizeChars"]` overrides the result limit *"(up to 500K)"* (`~/.claude/cache/changelog.md:9234`).
- The MCP signal must be the same on both lanes. The CLI lane has no `status` member: it keeps the integer exit code for shell operators (Handoff Notes ruling 5). A signal in `status` or in a new result member exists on the MCP lane only, so one call reports the bound in two ways (SSOT, Deterministic violation). The trailer in `output` gives identical bytes on both lanes. It keeps ruling 5 unchanged: a cut output is not a mismatch and not an error.

### 4. Materialised copies in transit (found 2026-11-28)

Contract: data in transit has 0 copy, 0 allocation, 0 mutation, 0 temporary container, 0 new struct, and 0 shadow state (MANIFESTO E, Complete at Creation). A struct or a tuple that only carries data is a fake carrier (CODING.md, No fake-carrier structs). Three @Pathfinder sweeps read `Source/` on 2026-11-28. Each citation below comes from verbatim `saw excerpt` output.

1. **`Paths::Entry` carries copies of the walk listing (per file):**
   - `Entry` is `std::tuple<std::string, juce::File, bool>` (`Source/Paths.h:12`).
   - The walk callback receives the work buffers `fullPath` and `displayBuffer` (`Source/Paths.cpp:270`). The walk cuts the buffers back after the call (`Source/Paths.cpp:271`, `Source/Paths.cpp:274`). Thus `Source/Paths.cpp:504` copies both into a new `std::string` and a new `juce::File` for each file.
   - The listing already holds the data. `getListedFolder` builds one `shared_ptr<const ListedFolder>` for each folder (`Source/Paths.cpp:201`). It holds the folder prefixes (`Source/Paths.cpp:227`) and one names buffer with `(start, length, isDirectory)` slots (`Source/Paths.cpp:241-242`). No step changes it after creation. Work items share it (`Source/Paths.cpp:213`). The walk does not keep the listings after the callbacks.
   - Consumers drop carried fields with `juce::ignoreUnused`: `Source/Find.cpp:112`, `Source/Find.cpp:215`, `Source/Replace.cpp:323`, `Source/Paths.cpp:452`, `Source/Paths.cpp:529`, `Source/Paths.cpp:540`. Only the sort reads `isExplicit` (`Source/Paths.cpp:531`).
   - An empty `juce::File {}` marks standard input (`Source/Paths.cpp:453`, `Source/Paths.cpp:455`, `Source/Replace.cpp:324`). This is an implicit marker, not an explicit type (MANIFESTO E).
   - `workerEntries` stages the entries of each worker before a move into `entries` (`Source/Paths.cpp:491-519`). This is a temporary container.
   - The walk builds a `juce::File` for each entry before a verb needs the capability (MANIFESTO E, Materialisation of Concrete Objects).
   - Each subdirectory copies three strings into a queued `Folder` (`Source/Paths.cpp:263`). Each range of 64 entries copies the folder strings into work buffers (`Source/Paths.cpp:232-234`).
2. **The file path is copied again at each layer (per file and per document):**
   - Content search builds a new `juce::File` from the walk buffer for each file (`Source/Find.cpp:86`).
   - Each `TextDocument` copies its display path into its own `juce::String` (`Source/TextDocument.cpp:10`). The owner of the path already holds it: `Paths::Entry`, the undo record, or the patch JSON.
   - Undo turns one record path into three or four copies for each entry (`Source/Undo.cpp:120`, `142`, `196`, `225`, `237`). The record stays alive for the whole command (`Source/Undo.cpp:340`). Undo also copies each record folder into a `juce::File` (`Source/Undo.cpp:175-183`).
   - Patch grouping keeps a `juce::String` key copy (`Source/Patch.cpp:37`, `51`), a display path copy (`Source/Patch.cpp:55`), and a `juce::File` copy (`Source/Patch.cpp:56`). A map with view keys for the same lookup is in the same function (`Source/Patch.cpp:38`).
   - `StagedFile` keeps its own `juce::File targetFile` (`Source/StagedFile.h:176`). Its consumers are not traced yet.
3. **Edit bytes are copied twice (per matched line):**
   - Replace builds each line in the scratch `replacedLine` (`Source/Replace.cpp:204`, `220-226`). `LineDiff::appendWithEnding` copies it again into `plannedText` (`Source/LineDiff.cpp:323`, `381`).
   - Patch column groups do the same through `lineText` (`Source/Patch.cpp:237-250`) and `setColumnLineText` (`Source/Patch.cpp:184-189`).
4. **Output bytes are copied after creation (per output byte):**
   - `Source/Json.cpp:992` copies the `MemoryOutputStream` into a new `std::string` for `Stdio::getDocuments`.
   - `Source/Page.cpp:569-572` copies the whole rendered page through `NonPrintable::getWithoutSequences` (`Source/NonPrintable.cpp:345-354`). The copy also runs when the page holds no sequence.
   - Both contradict the contract statement in `Source/Stdio.h:31`: *"No copy is made between the creation of the output and its write."*
   - The output bound (Scaffold item 11) acts on these documents.
5. **Containers repeat values that an owner or a rule already holds:**
   - `getLineNumbers` builds the array `1..N` (`Source/Page.cpp:468-483`). `PageRanges::getNumberOfLine` already returns `lineIndex + firstLine` for an index outside the array (`Source/PageRanges.cpp:256`).
   - `PageRanges::getRealNumbers` makes a second complete copy of `originalNumbers` (`Source/PageRanges.cpp:259-267`, called from `Source/Page.cpp:597`).
   - `PageRanges::getShownChanges` gives an array equal to its input when no squeeze runs (`Source/PageRanges.cpp:308-319`, `Source/Page.cpp:582-583`).
   - `PatchValidator` copies JSON values into tuples, ranges, and integers, then sorts the copies (`Source/PatchValidator.cpp:406-418`, `427-443`, `451-465`). Consumers drop fields with `juce::ignoreUnused` (`Source/PatchValidator.cpp:491`, `501`). In the same module, `addTargetFile` sorts indexes and copies no value (`Source/Patch.cpp:228-232`).
   - `getValueLocation` builds a location string for each JSON input value (`Source/Json.cpp:685-693`, `884`, `920`). Only the failure path reads it (`Source/Json.cpp:834`).
6. **The same bytes are read or parsed twice:**
   - Undo reads the target to check a hash (`Source/Undo.cpp:129`), then reads the same file again (`Source/Undo.cpp:142`). This happens only when the blob hash differs from `preHash` (`Source/Undo.cpp:126`).
   - `UndoCache::getTransactionIds` parses each pending record and drops the result (`Source/UndoCache.cpp:468-469`). `runList` parses the same record again (`Source/Undo.cpp:85`).
7. **Not read yet (code in jam, outside `Source/`):**
   - Storage of `jam::JsonDocument::readValues` and `addValue` (`Source/Json.cpp:277-278`, `358-371`, `406-458`).
   - Copy behavior of `jam::ModelContextServer::Content::setText` (`Source/ToolServer.cpp:74`).
   - Copy behavior of `LanguageDocument::parse` (`Source/Page.cpp:616`).
8. **Lower frequency (once per command, or on the failure path):**
   - Failure paths copy display paths (`Source/FindOutcome.cpp:11`, `Source/Paths.cpp:298-299`).
   - The theme style sheet is copied from a temporary `Input` into a `std::string` (`Source/PageConfiguration.cpp:202-212`).
   - Each config line is copied at each step (`Source/PageConfiguration.cpp:80-96`).
9. **Examined and excluded (creation, or a permitted copy):**
   - `StandardStream::write` copies into its own buffer at the OS boundary (`Source/StandardStream.cpp:50`, `59`).
   - The `Replacement` tuple moves its data (`Source/Replace.h:36`).
   - `TextDocument::fromText` moves its text (`Source/TextDocument.cpp:154-160`).
   - Diff rendering builds new text (creation).
   - The standard input buffer grows by `realloc` while it reads (`Source/Input.cpp:196-200`). This is creation.
   - `JsonOptions` and `PageOptions` hold views into the argument list (`Source/JsonOptions.h:7-8`, `Source/PageOptions.h:79-101`).

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
- **Encapsulation (data in transit):** Data in transit has 0 copy, 0 allocation, 0 mutation, 0 temporary container, 0 new struct, and 0 shadow state (MANIFESTO E, Complete at Creation). One owner holds each path and each byte buffer. Each consumer reads the owner in place, through a reference, an index, or a view. A `juce::File` comes into existence only where a verb uses its capability (MANIFESTO E, Materialisation of Concrete Objects).

**Considered and rejected:**
- **Allowing `--follow` over MCP:** Rejected under Bounds and Deterministic; MCP tool calls must terminate. Follow is CLI-only (for human terminals and background watchdogs).
- **Folding `count` and two-file `diff` into `saw page`:** Rejected by ARCHITECT in favor of three dedicated verbs (`saw count`, `saw sort`, `saw diff`).
- **Adding a separate `--stat` TSV format alongside `--json`:** Rejected by ARCHITECT under SSOT (`--json` only).
- **Auto-inserting a newline on `saw write --append`:** Rejected by ARCHITECT under Deterministic (append exact raw bytes from `stdin`).
- **Adding `mv` / `cp` / `rm` in this RFC:** Excluded per ARCHITECT's explicit scope (*"1, 2, 3 append"*).
- **A client-side fix for the MCP disconnect (config, or a higher client limit):** Rejected under Deterministic. The config and the client log name no setting for the 16 MB guard. A higher limit only moves the failure point, because a broad query can exceed any limit. Each harness has its own client, so a client fix covers one harness only.
- **Option B, a gate rule that denies a `saw find` with no output mode (`-l`, `-c`, `--first`, `-q`) and no `--include`:** Not selected by ARCHITECT. The gate judges arguments, not results, so the rule also blocks valid content searches.
- **Option C, a CAROL.md rule: search for paths first (`-l`, `-c`), then read with `excerpt`:** Not selected by ARCHITECT. Every harness loads the rule as text, but nothing enforces it.

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
   - Depends on Scaffold item 12. The metadata fields (`size`, `mtime`, `mode`, `target`) go into the listing slot that `Paths::Entry` refers to. No second carrier holds them (ARCHITECT: *"why not metadata, or pointer only? why must carry materialised data?"*).

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

11. **Output bound on both lanes (`Source/ToolServer.cpp:79-125`, `Source/Stdio.h`):** all values settled by ARCHITECT on 2026-11-28 (Problem Statement).
   - `saw` bounds the output of every call by default, on the CLI lane and on the MCP lane. One value covers both lanes: 32 KB.
   - The bound is a byte bound with a line cut. `saw` prints whole lines while the printed bytes stay within 32 KB, then prints the trailer. A single line longer than 32 KB prints nothing, and the trailer reports it.
   - The trailer counts lines. Its format follows the dry-run message (`SPEC.md:306`): `saw: output limit — <P> line(s) printed, <O> line(s) omitted. Pass --no-limit to print all.`
   - `--no-limit` lifts the bound for one call. It joins the `--no-ignore` and `--no-line-number` family (`SPEC.md:356`, `SPEC.md:367`). "limit" is the existing `saw` word, as in "step limit" and "depth limit" (`SPEC.md:325`, `SPEC.md:818`).
   - MCP lane: the trailer goes in `output` only. The status of the verb does not change (ruling 5 keeps `"ok"`, `"mismatch"`, `"error"`). CLI lane: the trailer goes to stdout, so both lanes give identical bytes.
   - No CAROL gate change. One `saw` binary serves every harness, so the bound is one definition with one result everywhere (SSOT, Deterministic).
   - The bound covers every verb that writes output: `find`, `page`, `json`, `fields`, and the new verbs of this RFC.
   - Before implementation, COUNSELOR measures the message limits of `agy` and OpenCode, and confirms that each is above 32 KB plus the trailer. The only measured client limit is 16 MB (Claude Code).
   - Add a regression case on each lane in `test/run.lua`: output over 32 KB, a single line over 32 KB, and `--no-limit`.

12. **Materialised copies in transit (Research Summary §4):** ARCHITECT directed: *"fold into RFC, need to be planned"*.
   - Target for each item of §4: the consumer reads the owner in place, through a reference, an index, a view, or metadata (ARCHITECT: *"why not metadata, or pointer only?"*). The 0 rules apply (MANIFESTO E, Complete at Creation).
   - `Paths::Entry`: the walk listing (`ListedFolder`, `Source/Paths.cpp:201`) is the owner. An entry refers to a slot of the listing. A verb builds the `juce::File` only when it reads the file (MANIFESTO E, Materialisation of Concrete Objects). Standard input gets an explicit form in place of the `juce::File {}` marker (MANIFESTO E, no magic values).
   - Path copies (§4 item 2): each layer reads the same owner of the path.
   - Edit bytes (§4 item 3): build the planned bytes once, in `plannedText`.
   - Output bytes (§4 item 4): build the output once, in the document that is written (`Source/Stdio.h:31`).
   - Value containers (§4 item 5): each consumer reads the owner, or sorts indexes into the owner.
   - Double reads (§4 item 6): read or parse each file and each record once per command.
   - Lower frequency items (§4 item 8): the same rule applies.
   - Before the plan locks, read the jam code of §4 item 7.
   - Measure before and after with a `scout` manifest (profile skill). Workloads: `find` over a large tree, `replace` with many matches, and `page` and `json` with large output. ARCHITECT's goal: *"potentially reduced resource usage/boost performance"*.
   - The refactor keeps every output byte identical (Deterministic). The CLI lane and the MCP lane of `test/run.lua` must pass with no baseline change.

## BLESSED Compliance Checklist
- [ ] Bounds  - [ ] Lean  - [ ] Explicit  - [ ] SSOT
- [ ] Stateless  - [ ] Encapsulation  - [ ] Deterministic

## Open Questions

None for ARCHITECT. ARCHITECT settled every output bound decision on 2026-11-28 (Problem Statement, Handoff Notes rulings 6 to 10).

Research gaps to close before the plan locks:
1. The jam reads of Research Summary §4 item 7.
2. The consumers of `StagedFile::getTargetFile` (`Source/StagedFile.h:42`).

## Corrections (2026-11-28, plan session)

COUNSELOR read these facts during the plan session (`PLAN-exclusive-saw.md` §3). Each correction names the text that it supersedes and the read that replaces it. The superseded text stays above, unchanged.

1. **The page crash is not specific to standard input.**
   - Supersedes: Problem Statement *"one crash in `saw page` on piped stdin with TTY stdout"*, Research Summary §1 item 1, and Scaffold item 1 *"Fix null/lifetime defect in the rendered path (`Source/Page.cpp:580-648`)"*.
   - Every rendered run exits 139: `--color=always`, `--decorations=always`, `--paging=always`, and a file path (`SPEC.md -r 1:2 --color=always`), with stdout piped or to `/dev/null`. The plain lane exits 0.
   - The crash report (`~/Library/Logs/DiagnosticReports/saw-2026-11-27-105002.ips`, `EXC_BAD_ACCESS at 0x88`) shows `getPathOutput` → `ling_write_page` (libpage) → `TerminalPage::setFiles` → `MarkdownGraphics::addPropertyRow` at `jam_MarkdownGraphics.cpp:213`.
   - `addPropertyRow` reads `line.get<int> (Id::propertyType)` at line 201. It dereferences the result at lines 203 and 213 with no owner check.
   - The saw entry signature matches the library: `Source/Page.cpp:16-19` against `ling/Builds/languages/page/page.cpp:196-199`.
   - The report predates the current binary (`~/.local/bin/saw`, 27 Nov 23:00). macOS wrote no newer report.
   - The fix goes to the jam owner that sets `Id::propertyType` (PLAN Step E1). It does not go to `Source/Page.cpp:580-648`.
2. **Edit bytes are creation, not transit.**
   - Supersedes: Research Summary §4 item 3, and Scaffold item 12 *"Edit bytes (§4 item 3): build the planned bytes once, in `plannedText`"*.
   - `replacedLine` (`Source/Replace.cpp:204`, `220-226`) and `lineText` (`Source/Patch.cpp:237-250`) are work buffers of the planned bytes.
   - MANIFESTO E: *"Creation can use multiple passes, look-ahead, reverse steps, work buffers, and memory allocation"*.
   - No API writes into `plannedText` (`Source/LineDiff.h:180-185`, `301`).
3. **The page strip is a transform, thus creation.**
   - Supersedes: Research Summary §4 item 4 bullet 2.
   - `NonPrintable::getWithoutSequences` (`Source/NonPrintable.cpp:345-354`) builds new text. It is also the `--strip-ansi` transform (`Source/Page.cpp:243`).
   - New finding: the colour branch writes to `StandardStream` inside the verb (`Source/Page.cpp:562-567`). This contradicts `Source/Stdio.h:8-9`: *"A verb returns an Output, so that no verb writes to a process stream."* This output also bypasses the output bound (Scaffold item 11). PLAN Step A4 removes the write.
4. **The `PatchValidator` sorted values and `getValueLocation` are creation.**
   - Supersedes: Research Summary §4 item 5 bullets 4 and 5.
   - The sorted values (`Source/PatchValidator.cpp:406-465`) and `getValueLocation` (`Source/Json.cpp:685-693`) are derived results that creation builds. They are not a second copy of complete data.
5. **Most `juce::File` builds are materialisation at use.**
   - Supersedes: Research Summary §4 item 2 bullets 1 and 3, as copies in transit.
   - `Source/Find.cpp:86` builds the `juce::File` that `Search` opens. `Source/Undo.cpp:196`, `225`, and `175-183` build the files that undo writes and deletes.
   - MANIFESTO E: *"The owner builds the concrete object only when it needs the capability."*
   - Only the `Entry` build for `--files` materialises without a capability: `Source/Paths.cpp:504`, read at `Source/Find.cpp:215` with `juce::ignoreUnused (file, isExplicit)`.
6. **Harness message limits (supersedes Research Summary §3 *"The message limits of `agy` and OpenCode are not measured"*, and Scaffold item 11, last-but-one bullet).**
   - OpenCode v2.2.5: 16,777,216 bytes per message (`V=16777216`, minified `McpClient`, bundle line 265398).
   - Pi 1.6.0: `DEFAULT_MAX_MESSAGE_BYTES = 16 * 1024 * 1024` (`@earendil-works/pi-mcp/dist/transports/transport.js:2`). Pi has no saw MCP entry (`~/.carol/pi/`).
   - agy 1.5.18: the Go MCP SDK `frameLimitReader` is present. Strings and objdump do not give its value. PLAN Step 0.2 measures it.
   - agy 1.5.18, measured (PLAN Step 0.2): one `agy --print` call to saw MCP `page` on a 65536-byte fixture returned the last line `line-001024`, exit 0. The agy limit is above 32 KB plus the trailer.

## Handoff Notes

- **Baseline commit:** `saw 0.0.1 (c8865a6)` passes both `nvim --headless -l test/run.lua ~/.local/bin/saw` (CLI lane) and `... mcp` (MCP lane) with exit 0.
- **CAROL enforcement state:** `~/.carol` (`4cfe1a3`) and `~/.config` (`1843123`) enforce whole-session `saw` across Claude Code (`190/190` pass), `agy` (validated), and OpenCode (`196/196` pass).
- **Settled ARCHITECT rulings (all 11 decisions closed):**
  1. Three dedicated verbs: `saw count`, `saw sort`, `saw diff`.
  2. `--json` only for `saw find --files` metadata (`path`, `type`, `size`, `mtime`, `mode`, `target`).
  3. `saw write --append` appends exact raw bytes from `stdin`.
  4. Whole-session cross-platform denial of `find`, `wc`, `sort`, `uniq`, `diff`, `cmp`, and `tail -f/-F` (with `.exe` matching on Windows) in `~/.carol/hooks/saw-gate.mjs` once each `saw` suite passes.
  5. Replace MCP `"exitCode"` with `"status"` (`"ok"`, `"mismatch"`, `"error"`). CLI keeps implicit integer exit codes for shell operators plus clean stdout and stderr error messages.
  6. `saw` bounds the output of every call by default, on the CLI lane and on the MCP lane, with no CAROL gate change (option A).
  7. One bound for both lanes: 32 KB, a byte bound with a line cut. A single line longer than 32 KB prints nothing, and the trailer reports it.
  8. The trailer counts lines: `saw: output limit — <P> line(s) printed, <O> line(s) omitted. Pass --no-limit to print all.` `--no-limit` lifts the bound for one call.
  9. The MCP signal is the trailer in `output` only. The status of the verb does not change.
  10. COUNSELOR measures the message limits of `agy` and OpenCode before implementation.
  11. The materialised copies in transit of Research Summary §4 join the scope, to be planned (ARCHITECT: *"fold into RFC, need to be planned"*).
- **Suggested execution order for COUNSELOR:**
  1. Fix `saw page` piped-stdin TTY crash (`Source/Page.cpp`) first — live SIGSEGV defect blocking piped `saw page -r`.
  2. Implement `saw fields` ranges, `saw write --append`, and MCP `"status"` (`Source/ToolServer.cpp`) — smallest diffs on existing code.
  3. Implement `saw find --files` metadata + `--json`, `saw count`, `saw sort`, `saw diff`, and `saw page --follow` / `--follow-name`.
- **Downstream handoff:** After `saw` ships and passes CLI + MCP lanes on macOS and Windows, MACHINIST updates `~/.carol/hooks/saw-gate.mjs` and removes `TAIL_FOLLOW_PATTERN`.
- **Output bound (added 2026-11-28, MACHINIST):** After one broad query, defect 3 removes `saw` MCP for the rest of the session. An unbounded payload also fills the context window and uses the usage limit. ARCHITECT's goals: *"we need to be able to use saw deterministically"* and *"must be applied to CAROL consistently for all harness cli"*. Rulings 6 to 10 settle the design (Scaffold item 11). To reproduce the disconnect, run the replay in Research Summary §1 item 3 with a query whose output exceeds 16 MB.
- **Materialised copies (added 2026-11-28, COUNSELOR):** Scaffold item 12 is in scope (ARCHITECT: *"fold into RFC, need to be planned"*). Item 12 changes `Paths::Entry`, so item 12 comes before Scaffold item 3 (`find` metadata). The output copies of Research Summary §4 item 4 are in the same documents as the output bound (Scaffold item 11). Plan the two items together.
