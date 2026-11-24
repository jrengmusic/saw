# RFC — SAW must live to its premise
Date: 2026-11-24
Status: Ready for COUNSELOR handoff
Author: MACHINIST
Targets: `saw` v0.0.1 (`7a7039f`), and `jam_core` where this document names it

## Problem Statement

ARCHITECT's words that govern this RFC:

- *"can we enforce CAROL with hooks, or whatever claude code tools, so using sed, awk, grep, rg, writing custom py script for batch editing, any editing without saw are forbidden? must exclusively using saw, since it was the fastest, the most reliable, and the safest way to find, edit file?"*
- *"either way, verify first."*
- *"we should fix saw first, so it live to its premise. then we could enforce the protocol."*
- *"what is the defect?"*, *"what is currently lacking?"*, *"what it cant do?"*
- *"write comprehensive RFC with solution to saw repo"*

The premise has three claims. Each claim fails today in part.

| Claim | Verdict | Evidence |
|---|---|---|
| Fastest | Fails on one small file, on a regex tree search with no required literal, and on tree replace against python. Holds on literal tree search against `rg` and `grep`, and against `sed` run for each matching file. | Section S1 to S5 |
| Most reliable | Fails. One unreadable file ends a tree search with no output. A search with no path reports no match. | R1 to R9 |
| Safest | Fails. A write breaks hard links, widens file modes, drops extended attributes, and changes line endings outside the edit. Parallel applies lose undo history. | D1 to D8 |

What holds (observed):

- The functional suite passes 91/91 (`nvim --headless -l test/run.lua ~/.local/bin/saw`, exit 0).
- For 10 of 11 patterns on a 3,079-file corpus, `saw find` gives the same lines as `rg`, after CR removal, path prefix removal and a sort. The other pattern is `(?i)juce_api` (R7).
- A wrong or missing `--expect` writes zero bytes. A failed second file leaves the first file at its original bytes.
- `undo` refuses a file that changed after the apply.
- Eight parallel applies to one file: one applied, seven refused at `preHash`, no corruption.

## Research Summary

### Method

- Machine: Apple M4, 10 cores, macOS 27.2.1, zsh. Release binary `~/.local/bin/saw` (universal, signed).
- Timing: `scout`, 11 epochs, medians, run by Pathfinder one at a time. "Per call" is the epoch divided by 100 calls in a bash loop.
- Corpus: a copy of `JUCE/modules` without binary assets: 3,079 files, 50 MB. Small file: a copy of `CAROL.md`, 23,063 bytes, 452 lines.
- `ugrep` is the `grep` that Claude Code runs in its shell (shell function, seen with `type grep`). Its tree output goes through a pipe, because `/dev/null` gave a false 8.3 ms.
- Behavior: a battery of commands in a scratch folder, compared with `grep`, `rg`, or `sed` where the table says so.
- scout's `probability` column is not used. It gave 0.071 for tree replace, where the two sample ranges do not overlap (saw max 1.263 s, reference min 1.774 s). The cause is not known. `saw/carol/SPRINT-LOG.md:146` cites a probability from this column.

### What the sprint benchmark measured

- The sprint log claims parity with `rg`, not a win: *"reaches statistical parity with `rg` (+2.4%, `p = 0.082`)"* (`carol/SPRINT-LOG.md:146`).
- `benchmark/ref-sed-replace.sh` runs one `sed` for each file of the tree. That run takes 9.33 s. `sed` on the files that `rg -l` lists takes 1.79 s.
- `benchmark/saw-find-regex.sh` uses `class +JUCE_API`. This pattern has a required literal, so it takes the fast path (`Pattern.cpp:140-168`). No pair covers alternation, a pattern with no literal, `-i`, `-C`, or a small file.

### S — Speed

#### S1. Small-file calls are slow (per call)

| Task | saw | sed | awk | grep | rg | ugrep | perl | python | carol apply |
|---|---|---|---|---|---|---|---|---|---|
| Search | 5.66 ms | | | 1.47 | 2.75 | 5.49 | | | |
| Read 41 lines | 5.59 ms | 1.45 | 1.80 | | | | | | |
| Replace and write | 14.4 ms | 4.73 | | | | | 6.16 | 14.0 | 31.3 |

The replace row includes one `cp` (1.63 ms). `sed` and `perl` write a temporary file, then run `mv`.

#### S2. Where the startup goes (per call, the same loop)

| Probe | ms | What it adds |
|---|---|---|
| `/usr/bin/true` | 1.28 | |
| Empty C program | 1.58 | Process start |
| Empty program + libc++ | 1.57 | |
| Empty program + Foundation, IOKit, Security | 2.30 | |
| Empty program + the frameworks that `juce_core` declares (`Cocoa Foundation IOKit Security`) | 2.46 | |
| Empty program + the 17 frameworks of saw's link line | 2.79 | +1.20 over empty |
| saw relinked, first initializer calls `_exit` | 4.66 | +1.87: load of the 22 MB image |
| saw relinked, control, `--version` | 5.57 | +0.91: 286 static initializers, `main`, exit |
| saw relinked with `-Wl,-dead_strip_dylibs` | 5.22 | −0.34 |
| saw installed, `--version` | 5.50 | |

- The binary links 24 libraries, WebKit, CoreMIDI, Metal, MetalKit and AppKit among them (`otool -L`). `scout`, `cast` and `ling` each link 24 too.
- The binary runs 286 of its own initializers before `main` (`DYLD_PRINT_INITIALIZERS` count; `__mod_init_func` is 2288 bytes).
- scout sampling of the replace loop finds `jam_terminal.mm`, `jam_core.mm`, `ColourId` and string pool initializers. `find` uses none of the terminal, style, or colour code.
- The image load is not split further. The page map, the code signature and the fixups are candidates. None of them was measured alone.
- The cause: one help page. `Main.cpp:35-60` renders `--help` through `jam::TerminalPage`. The `## user module` table lists 14 JAM modules (`project-info.md:121-135`). The verb sources name only `jam_core` types: `Strings`, `Array`, `Document`, `JsonDocument`, `JsonValidator`, `JsonWriter`, `Format`, `simd`, `RegExp`, `HashMap`, `HashSet`, `Hash`, `Function`, `File`. `MarkdownDocument`, `TerminalPage` and `TerminalEncoder` occur only in `Main.cpp`, on the help path. SPEC §2 already forbids GUI initialization on the six verbs (`SPEC.md:91`). The modules still load.

**Facts that bound the target.**

- A `juce_core` program cannot start in less than about 2.46 ms in this loop. `rg` takes 2.75 ms for a whole search. `sed` takes 1.45 ms and `grep` takes 1.47 ms.
- An empty C program takes 1.58 ms. That is more than `grep` takes for a whole search. A third-party binary cannot beat Apple's `grep` and `sed` on one call to one small file in this loop.
- Thus "fastest" on one small file is out of reach. "Fastest for one batch in one process" is in reach.

#### S3. Regex without a required literal is slow (tree, output to a pipe)

| Pattern | saw | rg | Ratio |
|---|---|---|---|
| Literal `JUCE_API` (no pipe) | 37.9 ms | 43.1 | 0.88 |
| `[0-9]{3,}` | 312.9 ms | 53.5 | 5.8 |
| `TODO\|FIXME` | 325.6 ms | 40.0 | 8.1 |
| `-i juce_api` | 45.8 ms | 43.2 | 1.06 |
| `-C 2 JUCE_API` | 45.8 ms | 43.1 | 1.06 |

- `getRequiredLiteral` returns empty for any pattern with a `|` (`Pattern.cpp:149`), and for any pattern with no literal run of 2 bytes (`Pattern.cpp:167`).
- `-i` disables the regex prefilter (`Pattern.cpp:276`).
- With no prefilter, saw leaves the fused path (`Find.cpp:565-575`) and runs `jam::RegExp` on each line. `RegExp::findNext` starts the backtracking matcher at each position, and each step goes through a continuation and a hash lookup (`jam_RegExp.cpp:23-72`).

Literal tree search for reference: `ugrep` 31.7 ms (piped), saw 37.9, rg 43.1, `grep` 141.0.

#### S4. The matcher is exponential

- `saw find -r '(a+)+$'` on one line of 40 `a` and one `b` did not finish in 10 s. An alarm stopped it. `(a|aa)+$` gave the same result.
- `rg -c '(a+)+$'` on the same file finished in 10 ms.
- `RegExp::findNext` has no step limit (`jam_RegExp.cpp:23-42`). One bad pattern hangs an agent until its tool times out.

#### S5. The apply phase costs 6.9 ms

| Step | ms |
|---|---|
| Replace, dry run | 6.04 |
| Replace, apply (the 1.63 ms `cp` subtracted from 14.58) | 12.95 |

- `fsync` is not the cause. 300 write-and-rename cycles of a 23 KB file take 37.3 ms without `fsync` and 37.7 ms with it.
- SHA-256 is not the cause. Its frames have value 0 in the sample.
- The sampled apply frames are `Transaction::run` 5, `UndoCache::addTransaction` 3, `getSortedDirectoryNames` 3. The counts are low. They point at the undo folder listing.
- Each apply lists the undo folder twice: `getNextHexSuffix` (`UndoCache.cpp:26`) and `removeOldestTransactions` (`UndoCache.cpp:323`).
- The commit is serial over files, and each file is read and hashed two more times (`Transaction.cpp:99-109`, `:125-145`).
- Tree replace, 912 edits in 494 files, with a clone copy of the tree (476 ms alone): python 825 ms, saw 921 ms, `rg` + `sed` 1,790 ms. saw gives hashes, undo and atomic swaps in that time. Python gives none.

#### S6. Undo preview memory is quadratic

| Lines changed | Peak memory | Time |
|---|---|---|
| 1,000 | 37 MB | 0.01 s |
| 2,000 | 136 MB | 0.04 s |
| 4,000 | 532 MB | 0.12 s |

`getMyersTrace` stores a full copy of `furthestX` for each edit distance (`LineDiff.cpp:77-83`).

### D — Safety defects (data is changed or lost)

| ID | Defect | Observed | Cause |
|---|---|---|---|
| D1 | A write breaks hard links | `ln hard hard2`, then replace on `hard`: `hard` = `X`, `hard2` = `alpha`, link count 1 | `Writer.cpp:11,28` swaps with `juce::TemporaryFile`, which calls `rename` (`juce_SharedCode_posix.h`, `moveInternal`) |
| D2 | A write changes the file mode | 600→644, 640→644, 750→755, 444→644 | The temporary file gets the default mode. `Writer.cpp:26` restores only the execute bit. `SPEC.md:232` requires the original mode |
| D3 | A write drops extended attributes | `com.test.k` set before, absent after | Same swap. No metadata copy |
| D4 | A write changes line endings outside the edit | `alpha CRLF, beta LF, alpha LF, gamma CRLF` → every line CRLF. The line `beta` was not edited | `LineDiff::getPlannedText` writes one ending after every line (`LineDiff.cpp:402-418`). `TextDocument.cpp:37-38` sets it from the first line |
| D5 | Parallel applies share one transaction id | 8 applies to 8 files: one id. 64 applies to 64 files: 5 ids | `getNextHexSuffix` lists, then adds 1 (`UndoCache.cpp:24-46`). `getOrCreateDirectory` accepts an existing folder (`UndoCache.cpp:261`). `juce::File::createDirectory` returns `ok` for an existing folder |
| D6 | Undo history is lost under D5 | 8 applies reported "applied". The list shows one row with 1 file. The undo data of 7 edits does not exist | Each process writes `0.blob` and `meta.json` over the others |
| D7 | Rollback failure is silent, and the backup is deleted | Read, not run. The failure needs a write error during the rollback | `rollbackFiles` discards every result (`Transaction.cpp:70`). Then `txDir.deleteRecursively` removes the only backup (`Transaction.cpp:139`), and the message says "rolled back". `Undo.cpp:265-271` also discards the result |
| D8 | `undo` with no id crosses projects | Apply in `pa`, then in `pb`, then `undo --apply` in `pa`: it restored `pb/f` | `getSelectedTransactionId` takes the newest id of one machine-wide cache (`Undo.cpp:101-115`) |

Live impact of D1: `~/.carol/MANIFESTO.md` and `~/.carol/CODING.md` each have 24 links (`stat -f %l`). Commit `a808ac3` made project reference documents hard links. One saw edit of `CODING.md` detaches one copy and leaves the other 23 old. No message tells the agent.

### R — Reliability defects (the result is wrong or missing)

| ID | Defect | Observed | Cause |
|---|---|---|---|
| R1 | No path and stdin not a TTY: saw searches empty stdin | In a folder with matches: saw exit 1 with no output. `rg -n needle` and `grep -rn needle` find 3 lines. Agent shells have no TTY on stdin | `Find.cpp:895`. The rule is SPEC §4.4, from ARCHITECT's ruling *"Read stdin when no path is given and stdin is not a TTY"* |
| R2 | One unreadable file ends a tree search | 3 files with a match, one with mode 000: saw prints only `saw: not found: tree/b`, exit 2. `grep` and `rg` print the 2 good files and exit 2 | `Find.cpp:724-730` stops the walk. `Find.cpp:747-754` prints no match |
| R3 | One missing path ends a multi-path search | `find needle tree/a missing tree/c`: no match printed. `grep` prints both matches and exits 2 | `Paths.cpp:620-627`, `Find.cpp:624-631` |
| R4 | An unreadable folder is skipped with no message | saw exit 0. `rg` prints `Permission denied` and exits 2 | A failed `opendir` falls through (`Paths.cpp:191-193`). A missing result looks like no match |
| R5 | A permission error says "not found" | `find z np` on a mode-000 file: `saw: not found: np` | `Find.cpp:450-455` maps every `open` error to `failNotFound` |
| R6 | Option values and `--` collide | `lines 1 --replace "--" dd`: `flag requires a value`. `lines 1 --replace "-h" dd` prints the help page. The SPEC example `find -x "---" --after 12 --first` fails: `unknown flag: ---` | The terminator and help scans ignore option values (`Arguments.cpp:9`, `:69-82`, `:213-222`). `isOptionArgument` rejects any leading dash (`Arguments.cpp:224-229`). `SPEC.md:343` requires the example |
| R7 | Some regex syntax is rejected | `(?i)foo`, `[[:digit:]]+`, `\A`, `(?m)`, `(?x)`: `invalid regex`, exit 2. Lookaround, backreference, named group, `\p{L}`, `\b` and lazy quantifiers work | `jam::RegExp` grammar |
| R8 | A raw non-UTF-8 byte in a pattern does not match | `find $'caf\xe9' lat`: exit 1, the bytes are in the file | Arguments go through `juce::String::fromUTF8` (`Main.cpp:153-154`). SPEC §1.1 says raw-byte text. A replace on such a file keeps the byte |
| R9 | The undo cache ignores `HOME` | `HOME=<scratch> saw undo --list` shows the real cache | Path from `juce::File::getSpecialLocation`, through `jam::File::getUserApplicationDataDirectory` (`UndoCache.cpp:218`). A test or benchmark that makes 33 applies evicts the user's history. The sprint benchmark left 22 transactions of 494 files in it |

R9 also affected this research: the battery evicted the cache. The cache was restored from a copy taken first, and `diff -rq` reports no difference.

### L — What saw lacks (the tool it replaces has it)

| ID | Missing | Today | Replaces |
|---|---|---|---|
| L1 | Match across lines | A literal with a newline: `find` exit 1, `replace` 0 edits, exit 0. No message says the pattern can never match | The `Edit` tool's multi-line string, `rg -U` |
| L2 | List files that match | `unknown flag: -l` | `grep -l`, `rg -l` |
| L3 | Print only the match | `unknown flag: -o` | `grep -o` |
| L4 | Word match | `unknown flag: -w` | `grep -w` |
| L5 | Context before, after | `-A`, `-B` unknown. Only `-C` | `grep -A`, `-B` |
| L6 | Combined short flags | `unknown flag: -rn` | `grep -rn` |
| L7 | More than one pattern | Only `-r 'a\|b'`, which is 8.1× slower than rg (S3) | `grep -e a -e b` |
| L8 | Count without zero rows | `-c` on a tree prints one row for each file. 2,573 of 3,067 rows were `:0` | `rg -c` |
| L9 | `--hidden`, `--no-ignore` on `replace` | `unknown flag: --hidden`. A tree replace cannot reach `.claude/` or an ignored file. `Replace.cpp:344-348` passes `false, false` | `sed` on a file list |
| L10 | Capture names in a replacement | `${2}` is literal text. `$10` and `$1x` are ambiguous | `sed`, python `re` |
| L11 | Escapes in a replacement | `\n` is literal text | `sed`, `perl` |

### N — What saw cannot do

| ID | Cannot do | Note |
|---|---|---|
| N1 | Create a file | `saw: not found: newfile`. `--insert-after 0` needs an existing file. `Write` stays necessary |
| N2 | Delete, move, or rename a file | No verb. Outside its scope: `SPEC.md:14` names edits |
| N3 | List files in a tree | `--files` unknown. `rg --files`, `fd` |
| N4 | Compute (sum, group, join) | SPEC §1.3 excludes an embedded language. `awk` programs and python stay outside |
| N5 | Change a file that it must keep hard-linked | D1 |
| N6 | Beat `grep` or `sed` on one small file | S2 bounds |
| N7 | Reach a role with no Bash | `agents/counselor.md:6` lists no Bash. SPEC §1.3 excludes MCP: *"why mcp? not just cli?"* |
| N8 | Run on Windows today | `dev/Release` and `___builds___` hold only a macOS saw. The Windows walk is one branch and serial (`Paths.cpp:588`). Nothing on Windows was run here. SPEC §9 requires macOS and Windows |

### T — What the tests do not cover

`test/cases.md` holds no case for a hard link, a file mode other than the execute bit, an extended attribute, mixed line endings, two processes at once, undo across folders, an unreadable file or folder, a missing path among several, an option value that starts with a dash, or an exponential regex. Each of D1 to D6, D8, R1 to R6 passed the 91 cases unseen.

`saw/CLAUDE.md` is stale. It says the folder *"holds no source code yet"* and that the plans are `pending`.

## Principles and Rationale

| Pillar | Application |
|---|---|
| **B — Bound** | A transaction owns its folder. The owner creates it exclusively, so two processes never share it (D5, D6). The backup lives until every restore is verified (D7). |
| **L — Lean** | The verbs pay for nothing that they do not use. The help page must not load 13 other JAM modules into a search (S2). |
| **E — Explicit** | No silent result. A read that fails says so (R2 to R5). A pattern that can never match says so (L1). A failed rollback says so (D7). |
| **S — SSOT** | The file is the truth for its mode, its attributes and its line endings. saw restates none of them (D2, D3, D4). |
| **S — Stateless** | The undo cache is state shared by every session on the machine. `undo` with no id then depends on other sessions (D8). |
| **E — Encapsulation** | Untouched bytes are data in transit. MANIFESTO: *"No copy … No mutation"*. D4 rebuilds every line ending. |
| **D — Deterministic** | The same input gives the same output. A search result that depends on one unrelated unreadable file (R2), or an undo target that depends on another session (D8), is not deterministic. An exponential matcher has no time bound (S4). |

Considered and rejected:

- **Remove `fsync` to speed up the apply.** Rejected by measurement: it costs nothing here (S5).
- **`-dead_strip_dylibs` alone.** It saves 0.34 ms of 5.57 (S2). It does not remove the image or the initializers.
- **A daemon.** `SPEC.md:57` excludes it.
- **`sed`-style in-place write for every file.** It loses the atomic swap. Only a hard-linked file needs it (Open Question 1).
- **A defensive retry in `undo`.** MANIFESTO D: the invariant belongs at the owner. The owner is the exclusive folder create (D5).

### Proposed solutions, in order

**Phase 1 — Safety. Nothing else starts before it.**

1. **D2, D3:** copy the metadata of the target to the temporary file before the swap. On macOS one call does mode, ACL and attributes: `copyfile (target, temp, nullptr, COPYFILE_SECURITY | COPYFILE_XATTR)`. On Windows JUCE already calls `ReplaceFile`. Its effect on ACLs and attributes was not run. The owner is `Writer::toFile`.
2. **D1:** Open Question 1.
3. **D4:** `LineDiff::getPlannedText` copies each untouched run as one byte range of the original, each line with its own ending. Only inserted lines take an ending. This removes the rebuild of unchanged lines.
4. **D5, D6:** create the transaction folder exclusively. The system call that fails on an existing folder is the owner of the invariant: `mkdir` (POSIX), `CreateDirectoryW` (Windows). 64 parallel `mkdir` calls gave one winner in this session. On failure the suffix advances by 1. The id format and the LIFO order within one second stay (`undo-lifo-two-transactions`).
5. **D7:** check each rollback result. Keep the transaction folder when a restore fails. State the folder in the message (Open Question 2).
6. **D8:** Open Question 3.
7. **T:** one case for each of D1 to D8. The exclusive create and the mode checks need the runner to run two processes and to read modes.

**Phase 2 — Reliability.**

1. **R2, R3:** continue after a failed file. Print each error on stderr and the matches on stdout. Exit 2 when any error happened. `grep` and `rg` do this (observed).
2. **R4:** report a failed `opendir` the same way.
3. **R5:** one message for each cause. The text is ARCHITECT's (Open Question 2).
4. **R1:** Open Question 4.
5. **R6:** one parse pass. A value option takes the next argument verbatim. The terminator and the help flag count only outside a value. The SPEC example is Open Question 5.
6. **R8:** keep arguments as raw bytes (`std::string_view` over `argv`) for patterns and replacements. Windows needs its own conversion from UTF-16.
7. **R9:** Open Question 6.

**Phase 3 — Speed.**

1. **S2, S1:** render the help pages at build time and embed the frames. `jam::TerminalPage` has a fixed width (*"The page width is the Markdown column width"*, `jam_TerminalPage.h`), and two piped `--help` runs gave identical bytes (55,222). The runtime prints one embedded frame. `--help` keeps its look. saw then drops every JAM module except `jam_core`. `writeScreenErase` (`Main.cpp:18-27`) uses `jam::TerminalEncoder`, so the erase sequence joins the embedded data. Open Question 7.
2. **S2:** `jam_core` includes `juce_graphics` (`jam_core.h:53-56`), which declares Cocoa and QuartzCore. All that the verbs use from JAM is in `jam_core` (S2). Open Question 8.
3. **S3:** a prefilter for an alternation is the set of required literals of each branch. `TODO|FIXME` has two. A prefilter for `-i` uses `findIgnoreCaseBytes`, which exists (`Pattern.cpp:198`). Both keep the fused path.
4. **S4, S3:** the matcher. Open Question 9.
5. **S5:** one listing of the undo folder for each apply, after the commit. The exclusive create needs no listing.
6. **S6:** linear-space Myers (E. Myers 1986, the linear-space refinement), in place of the stored trace.
7. **Literal tree search against `ugrep`:** the walk gives one job to each top-level subfolder (`Paths.cpp:544-557`). One large subfolder then limits the speed-up. This is a hypothesis. A scout `## sample` run confirms or rejects it before any change.
8. **Benchmark:** SPEC §10.3 gets the pairs of S1, S3 and S5, with `sed` on the `rg -l` list in place of one `sed` for each file.

**Phase 4 — Coverage.** L1 to L11 and N1, N3 as ARCHITECT selects (Open Questions 10 to 13).

**Phase 5 — Platform.** The Windows build, the suite, and the benchmark (N8, Open Question 14).

**Phase 6 — CAROL wiring (SPEC §7).** It starts after Phases 1, 2 and 5.

## Scaffold

### Metadata-preserving write (ran in this session, macOS)

```c
struct stat info;
if (stat (target, &info) != 0) return 2;
if (info.st_nlink > 1)
{
    int fd = open (target, O_WRONLY | O_TRUNC);
    if (fd < 0 || write (fd, bytes, strlen (bytes)) < 0 || fsync (fd) != 0 || close (fd) != 0) return 2;
    return 0;
}
int fd = open (temp, O_CREAT | O_EXCL | O_WRONLY, 0600);
if (fd < 0 || write (fd, bytes, strlen (bytes)) < 0 || fsync (fd) != 0 || close (fd) != 0) return 2;
if (copyfile (target, temp, NULL, COPYFILE_SECURITY | COPYFILE_XATTR) != 0) return 2;
if (rename (temp, target) != 0) return 2;
```

Observed: modes 600, 640, 750 and 444 unchanged. The attribute and the ACL (`everyone deny delete`) unchanged. A hard-linked pair: both names hold the new bytes, link count 2. The in-place branch is option A of Open Question 1. It is not atomic.

### Reproduction commands

```sh
# D1
echo alpha > h; ln h h2; saw replace alpha X h --apply --expect 1; cat h2; stat -f %l h
# D2
echo alpha > m; chmod 600 m; saw replace alpha X m --apply --expect 1; stat -f %Lp m
# D3
echo alpha > x; xattr -w com.test.k v x; saw replace alpha X x --apply --expect 1; xattr -p com.test.k x
# D4
printf 'alpha\r\nbeta\nalpha\ngamma\r\n' > mix; saw replace alpha X mix --apply --expect 2; od -c mix
# D5, D6
for i in 1 2 3 4 5 6 7 8; do echo alpha > p$i; done
for i in 1 2 3 4 5 6 7 8; do saw replace alpha X p$i --apply --expect 1 & done; wait; saw undo --list
# R1
mkdir t; echo needle > t/a; cd t; saw find needle < /dev/null; echo $?
# R2
mkdir u; echo needle > u/a; echo needle > u/b; chmod 000 u/b; saw find needle u
# R6
saw lines 1 --replace "-h" file
# S4
python3 -c 'print("a"*40+"b")' > c; saw find -r '(a+)+$' c
```

The commands write to the live undo cache (R9). Copy the cache first.

## BLESSED Compliance Checklist

State of v0.0.1 (unchecked = a finding above breaks it):

- [ ] Bounds — D5, D6, D7
- [ ] Lean — S2
- [ ] Explicit — R2, R3, R4, R5, L1, D7
- [ ] SSOT — D2, D3, D4
- [ ] Stateless — D8, R9
- [ ] Encapsulation — D4
- [ ] Deterministic — R1, R2, D8, S4

## Open Questions

ARCHITECT settles each question. The questions touch ARCHITECT rulings in SPEC §1.2, a format, or a message text.

1. **Hard-linked target (D1).** The ruling *"juce already have ootb for mmap, swap, etc"* selects the swap. A swap cannot keep a link.
   - A. Write in place when the link count is above 1. The undo blob covers a crash. The write is not atomic.
   - B. Refuse with exit 2 when the link count is above 1. CAROL documents stay out of reach of saw.
2. **Message texts (D7, R5).** The text for a failed rollback, and for a permission error. The formats in `cast/text.md` own them.
3. **Default target of `undo` (D8).**
   - A. The newest transaction whose files are all below the working folder.
   - B. Require the id. MANIFESTO E (Explicit): *"nothing pulled from implicit context"*. This is the option that BLESSED selects. It changes SPEC §5.6, where `saw undo` selects the latest, and the `undo-lifo-two-transactions` case.
   - C. Keep the machine-wide newest, and print the file paths before the restore.
4. **No path, stdin not a TTY (R1).** The current rule is ARCHITECT's ruling.
   - A. Read stdin only when it is a pipe or a regular file. Walk `.` otherwise. `rg` walked the folder with `/dev/null` on stdin (observed).
   - B. No path and empty stdin: exit 2 `saw: missing path argument`.
   - C. Keep the rule, and let the CAROL hook deny a `saw find` with no path.
5. **A pattern that starts with a dash (R6).**
   - A. Amend SPEC §5.1 examples to `saw find -x --after 12 --first -- "---" DEBT.md`. Section 4.1 already defines `--`.
   - B. Accept a dash token as a positional when no flag matches it. A mistyped flag then becomes a pattern.
   MANIFESTO E (Explicit, fail fast) selects A.
6. **Undo cache of a test or a benchmark (R9).** SPEC §1.2 rules *"saw gets no new surface"*, and the runner moves the real cache. The benchmark scripts do not.
   - A. The benchmark scripts move the cache aside, as the runner does. This keeps the ruling.
   - B. An environment variable selects the cache root. This is new surface.
   MANIFESTO L (YAGNI) selects A.
7. **How the help frames are built (S2).** The output must stay identical to `jam::TerminalPage`.
   - A. A cast step renders each help document, and the frames are binary data.
   - B. A second target in the saw project renders them before the main target builds.
   Either one needs a host program that links `jam_tui`. Where that program lives is ARCHITECT's.
8. **`jam_core` depends on `juce_graphics` (S2).**
   - A. Split the modules that need no graphics (`RegExp`, `simd`, `Strings`, `Format`, `json`, `document`) into a module on `juce_core` alone.
   - B. Keep `jam_core` whole. saw keeps the graphics frameworks. Floor: between 2.46 and 2.79 ms plus the image load.
   This is a JAM decision. It also concerns `scout`, `cast` and `ling`, which link the same 24 libraries.
9. **The regex matcher (S3, S4).**
   - A. A step budget in `jam::RegExp`. A pattern over the budget ends with exit 2 and a message. No speed change for ordinary patterns.
   - B. A linear-time engine in `jam_core` for a pattern with no backreference and no lookaround. The current matcher, with the budget of A, stays for the others.
   MANIFESTO D requires a bound. A gives the bound. B gives the bound and the speed of S3.
10. **Multi-line match (L1).**
    - A. A flag that searches the whole file. The per-line ruling *"Matches never cross a newline (sed/grep model)"* stays the default.
    - B. Keep the ruling, and reject a pattern that holds a newline with exit 2. `patch` covers the edit.
    MANIFESTO E (Explicit) requires at least B.
11. **`find` flags (L2 to L8).** Which of them to add, and their names. L6 changes the parser.
12. **`replace` (L9 to L11).** Add `--hidden` and `--no-ignore`. Add `${n}`. Add escapes or keep them literal.
13. **File creation (N1), file list (N3).** SPEC §1.2 rules *"Full 6 verbs"*. Creation is a new operation of `lines` or `patch`, or stays with the `Write` tool.
14. **Windows (N8).** Who builds and runs the suite on Windows, and whether enforcement waits for it.
15. **Regex syntax (R7).** Add inline flags, POSIX classes and `\A`, or document the ECMAScript grammar in `find.md`.
16. **The speed premise (S2, N6).** The acceptance target. Candidates:
    - A. Not slower than `rg` on every tree pattern of S3, and not slower than `rg` on one small file (2.75 ms).
    - B. A, and fastest for a batch: one `patch` of N edits against N `sed` calls.
17. **Undo cap (S5, D6).** Keep 32 for the machine, or count per working folder. One parallel agent run of 33 applies evicts every other session's history.

## Handoff Notes

- **Order:** Phase 1 first. Until D1 is fixed, saw must not edit `~/.carol/*.md`, because a saw edit detaches one copy of a 24-link document.
- **Enforcement (CAROL side) waits.** ARCHITECT: *"we should fix saw first, so it live to its premise. then we could enforce the protocol."* Facts for that later pass:
  - COUNSELOR has no Bash (N7). A ban on `Edit` locks it out.
  - A hook reads command text, not intent. It does not see inside `bash script.sh`.
  - The gates exist three times: `~/.carol/hooks/register.js`, `~/.carol/agy/hooks/agy-gate.mjs`, `~/.carol/opencode/plugins/carol/gate.js`. SPEC §7 names two of them.
  - SPEC §7 uses `permissions.ask` for `python*` and `lua*`. `settings.json` runs `bypassPermissions`. Whether `ask` prompts there was not tested.
  - `carol apply` (31.3 ms) and `fix.lua` are the callers that SPEC §7 moves to saw.
- **Not run:** D7, every Windows claim, the composition of the 1.87 ms image load, the walk-balance hypothesis (Phase 3, item 7), and the effect of `ReplaceFile`.
- **scout:** the `probability` column contradicts disjoint samples (Method). It is a scout defect, outside this RFC.
- **Measurement artifacts** were in the session scratchpad and are not kept. The reproduction commands above give every D and R finding. The scout pairs of S1, S3 and S5 belong in `benchmark/` (Phase 3, item 8).
- **Session corrections:** `fsync` was the first suspect for S5 and was rejected by measurement. The first claim that the binary loads 22 frameworks was wrong. It loads 24 libraries, of which 21 are frameworks.
- **Working tree:** `CMakeLists.txt` was modified before this session (a patch comment). This RFC is the only file that this session added to the repository.
