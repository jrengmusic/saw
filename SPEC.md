# SAW Specification

**SAW: Safe Atomic Writer**
**Version:** 0.0.1
**Repository:** `~/Documents/Poems/dev/saw/`
**Binary:** `~/.local/bin/saw`

---

## 1. Overview

**Purpose:** SAW replaces `sed`, `grep`, and file-oriented `awk` with one deterministic, dry-run-first, atomic CLI tool.
**Target End-User:** CAROL agents (frontier and local models), CAROL scripts (`bin/carol`, JANITOR `fix.lua`), and ARCHITECT in the terminal.
**Core Workflow:** Preview exact matches or diffs by default, apply edits only with `--apply --expect N`, and reverse any applied transaction with `saw undo`.

### 1.1 Scope and Authority

This document is the specification, and it governs every implementation detail.
`Source/HELP.md` derives from this document and holds no independent authority.
Where `Source/HELP.md` and this document disagree, this document is correct.

SAW operates on UTF-8 and raw-byte text files.
SAW knows nothing about C++, CAROL, `doxygen-protocol`, or version-control commits.
CAROL hooks enforce CAROL policy outside SAW.

### 1.2 Locked ARCHITECT Decisions (Verbatim)

Every design decision in this specification rests on ARCHITECT's verbatim rulings:

- **Repository and Name:** `"~/Documents/Poems/dev/saw/"`
- **Version:** `"v0.0.1"`
- **CLI-Only SSOT and Hook Policy:** `"why mcp? not just cli? i meant one SSOT tools for all file manipulation. sed awk immediately rejected hooks, python and lua must ask permission. editing file no need nvim. but use in-house cli as must use tools. i think juce already have ootb for mmap, swap, etc why do we need atomic file?"`
- **Scope (`awk`, `sed`, `grep` Replacement):** `"how can we make this as tiny as possible, as fast as possible, with easiest syntax as possible, can this replace awk, sed, grep simultaenously?"`
- **HELP Page Identity, Dry-Run Default, and Undo:** `"i still like the HELP page. it's our identity. ~10mb still acceptable. why not we make it always dry run by default. so destructive edit must be explicit. agents could still make mistake passing wrong args, so the default run is always safe and undoable"`
- **Verb Surface:** `"Full 6 verbs: find, replace, lines, fields, patch, undo — covers grep, sed, awk columns/ranges, fix.lua batch, and undo"`
- **Apply Gate:** `"--apply always requires --expect N (dry-run prints N; mismatch writes 0 bytes)"`
- **Regex and Directory Traversal:** `"Literal default + -r regex (extend jam::RegExp with findNext/spans) + recursive dir walk skipping .git/.gitignore"`
- **Undo Store:** `"Rolling cache in app-data dir (cap 32 transactions, verifies post-edit hashes before restoring)"`
- **Binary File Handling:** `"Skip binary files (NUL byte) in directory walks; fail with an explicit error when named directly"`
- **Line Endings, Permissions, and Symlinks:** `"Preserve each file's LF/CRLF line endings and execute bit; follow symlinks to target (matching cast/scout)"`
- **`--expect N` Counting Rule:** `"Exact edit operations applied (matches replaced in replace, edit entries in patch, line ops in lines)"`
- **Stdin Piping Rule:** `"Read stdin when no path is given and stdin is not a TTY (for find, lines, fields, dry-run replace); patch reads stdin or --file"`
- **Project Skeleton:** `"Run cast INIT.md ~/Documents/Poems/dev/saw from archetype/cli. Then add the generation layer (identifiers/text/banner/files/source.cast + spell.md rows) to saw, in the same shape as scout/cast/"`
- **Test Harness:** `"why unit test still need lua? scout whatdbg now native mcp"`, `"so what harness do we need to test? benchmark against sed awk grep rg"`, `"design comparison runner then. do we need nvim?"`, `"which is more deterministic? more objective? zsh or nvim with lua?"`, `"ok nvim lua"`
- **Screen Erase:** `"Erase only on the help/banner path (saw, --help, <verb> --help). Never on the six verbs or --version"`
- **Regex Scope:** `"Per line: find, replace, fields --match run the regex on each line's text without its line ending. Matches never cross a newline (sed/grep model). -i wraps the pattern as (?i:...)"`
- **Ignore Rules:** `"Full git semantics, including the global excludesFile and .git/info/exclude"`, bounded by `"Ignore sources: every .gitignore from the repo root down to the path, $GIT_DIR/info/exclude, and core.excludesFile. Precedence per git-scm gitignore DESCRIPTION"` and `"core.excludesFile lookup: $XDG_CONFIG_HOME/git/config, then ~/.gitconfig (last value wins), with ~/ expansion. Default $XDG_CONFIG_HOME/git/ignore, else ~/.config/git/ignore"`
- **Verb Help:** `"One embedded markdown per verb (Source/help/find.md … undo.md) as separate binary entries, plus HELP.md for the overview. Uses the existing API with no section extraction"`
- **Diff Format:** `"GNU-exact. ',1' is omitted for one-line ranges. An empty range uses the GNU start rule. '\ No newline at end of file' marks incomplete lines. Hunks merge when their 3-line contexts overlap. Output stays valid input for patch and `git apply`"`
- **Specification Amendment:** `"Yes. ORACLE amends SPEC.md first, quoting each of today's rulings verbatim. Then the PLAN is written against the amended SPEC"`
- **Test Undo Isolation:** `"The runner moves the real <app-data>/saw/undo aside before the run and puts it back after, also on failure. saw gets no new surface"`
- **Benchmark Corpus:** `"A copy of ~/Documents/Poems/JUCE/modules. It is real C++ text, it is present on every machine (CMake's JUCE path is ../../JUCE), and it is copied to a temporary folder before each mutating epoch"`
- **CAROL Wiring (§7):** `"carol init on saw repo by ARCHITECT"`

### 1.3 Explicit Non-Goals

- No MCP server (`--mcp`) and no daemon process (`"why mcp? not just cli? i meant one SSOT tools for all file manipulation."`).
- No embedded scripting language (`BEGIN`/`END` blocks, loops, associative arrays, or arithmetic expressions).
- No fuzzy matching, heuristic patching, or silent fallback.
- No version-control subprocess calls inside SAW.
- No AST parsing or language-specific linting inside SAW.

---

## 2. Technology Stack and Architecture Constraints

- **Language:** C++17 (`CMAKE_CXX_STANDARD 17`).
- **Project Skeleton:** Run `cast INIT.md ~/Documents/Poems/dev/saw` in `~/Documents/Poems/dev/archetype/cli` (`ARCHETYPE.md:93`). Then add the generation layer in the shape of `~/Documents/Poems/dev/scout/cast/`: `identifiers.md`, `text.md`, `banner.md`, `files.md`, `source.cast`, and the matching rows of `cast/spell.md` (`"Run cast INIT.md ~/Documents/Poems/dev/saw from archetype/cli. Then add the generation layer (identifiers/text/banner/files/source.cast + spell.md rows) to saw, in the same shape as scout/cast/"`).
- **Build System:** `cast`-generated `CMakeLists.txt`, CMake 3.25+, Ninja, Release LTO (`-O3 -flto=thin`, `-dead_strip` on macOS). Edit `project-info.md`, run `cast`, then configure. Never edit `CMakeLists.txt` by hand.
- **Frameworks:** JUCE `8.0.17` + JAM (`jam_core` for runtime operations; `jam_tui` for `--help` rendering: `"i still like the HELP page. it's our identity. ~10mb still acceptable."`).
- **Linked JUCE modules (`## juce module` table of `project-info.md`):** the six rows of `~/Documents/Poems/dev/scout/project-info.md:108-113` (`juce_core`, `juce_data_structures`, `juce_events`, `juce_graphics`, `juce_gui_basics`, `juce_audio_processors`), plus `juce_cryptography` for `juce::SHA256`. scout links `jam_gui`, whose declaration lists `juce_gui_extra` and `juce_animation` (`~/Documents/Poems/dev/jam/user-modules-info.md:90,97`), and scout builds without those two rows. Thus the table does not restate them.
- **Linked JAM modules (closure of `jam_tui`):** `jam_core`, `jam_document`, `jam_markdown`, `jam_freetype`, `jam_graphics`, `jam_data_structures`, `jam_animation`, `jam_web`, `jam_style`, `jam_gui`, `jam_mermaid_diagram`, `jam_markdown_graphics`, `jam_terminal`, `jam_tui`.
- **Platforms:** macOS (ARM64 and x86_64) and Windows (x64, MSVC / clang-cl).
- **Primary Dependencies (OOTB):**
  - `juce::MemoryMappedFile` (`readOnly`) for zero-copy file reads (`"i think juce already have ootb for mmap, swap, etc"`), with a POSIX `open`/`fstat`/thread-local buffered `read` + `mmap` fast path in `Find` for small files.
  - `juce::TemporaryFile` (`useHiddenFile`) and `overwriteTargetFileWithTemporary()` for atomic file swaps (`"i think juce already have ootb for mmap, swap, etc"`). `juce::FileOutputStream::flush()` returns `void`, so SAW reads `getStatus()` after `flush()`.
  - Parallel directory walks via POSIX `opendir`/`readdir` (`d_type`) on macOS/Linux and non-recursive `juce::RangedDirectoryIterator` on Windows, skipping `.git` and ignored directories without descending.
  - Lock-free `Jobs::run` (`std::atomic<int>` + `std::thread` workers, direct calling-thread execution for `count == 1`) for parallel per-file work and fused walk+search in `Find`.
  - `juce::SHA256` for content and transaction hashes.
  - Raw UTF-8 `jam::Strings` parsed by `Arguments` for the command line (avoiding `juce::ArgumentList` whitespace trimming, empty-argument removal, and short-option matching defects).
  - `jam::simd::findByte` for SIMD byte scanning.
  - `jam::RegExp` (extended in `jam_core` with offset-based `findNext` returning match spans and captures: `"Literal default + -r regex (extend jam::RegExp with findNext/spans) + recursive dir walk skipping .git/.gitignore"`).
  - `jam::JsonDocument` and `jam::JsonWriter` for `patch` inputs and undo metadata.
  - `jam::TerminalPage` and `jam::MarkdownDocument` for `--help` rendering.
- **Test runtime:** Neovim 0.10 or later (`vim.system` exists since 0.10). The runner is `nvim --headless -l test/run.lua` (§10).
- **Benchmark runtime:** `scout` (§10.3).

### Architecture Constraints

- ✅ Scope `juce::ScopedJuceInitialiser_GUI` strictly inside `printBannerAndHelp()`.
- ✅ Run all six verbs in plain `int main (int argc, char* argv[])` with zero GUI initialization.
- ✅ Erase the screen only on the help path (§4.1).
- ✅ Use `juce::TemporaryFile` directly and check every `FileOutputStream` write result and the stream status after `flush()` before calling `overwriteTargetFileWithTemporary()`.
- ✅ Follow BLESSED (`MANIFESTO.md`), `CODING.md`, and `NAMES.md`.
- ❌ Never call `juce::File::replaceWithData()` (it deletes 0-byte targets and ignores `appendData()` errors).
- ❌ Never spawn external subprocesses (`sed`, `awk`, `grep`, `diff`, `cmp`, `cp`, or any version-control program).
- ❌ Never write files on a default run without `--apply` (`"why not we make it always dry run by default. so destructive edit must be explicit."`).
- ❌ Never store persistent daemon state or repository-local cache files.

---

## 3. Core Principles

1. **Dry-Run by Default:** Every mutating verb previews changes and writes zero bytes unless `--apply` appears on the command line (`"why not we make it always dry run by default. so destructive edit must be explicit. agents could still make mistake passing wrong args, so the default run is always safe and undoable"`).
2. **Explicit Two-Gate Write:** `--apply` always requires `--expect N` on `replace`, `lines`, and `patch` (`"--apply always requires --expect N (dry-run prints N; mismatch writes 0 bytes)"`).
3. **Literal by Default:** Search and replacement treat patterns as exact bytes unless `-r` enables regex (`"Literal default + -r regex (extend jam::RegExp with findNext/spans) + recursive dir walk skipping .git/.gitignore"`).
4. **Atomic All-or-Nothing Transactions:** Every write swaps through `juce::TemporaryFile`. If file `k + 1` fails, SAW restores files `0..k` immediately.
5. **Verified Reversible Undo:** Every `--apply` saves pre-edit bytes and post-edit SHA-256 hashes in a bounded rolling cache (`"Rolling cache in app-data dir (cap 32 transactions, verifies post-edit hashes before restoring)"`).
6. **Deterministic Output:** Parallel per-file work emits results sorted by normalized UTF-8 path, then line number, then byte offset.
7. **Exact Preservation:** SAW preserves each file's existing line endings (`LF` or `CRLF`), trailing newline presence, symlink target resolution, and POSIX execute permission bit (`"Preserve each file's LF/CRLF line endings and execute bit; follow symlinks to target (matching cast/scout)"`).

---

## 4. Global CLI Contract

### 4.1 Invocation

Decision: `"Full 6 verbs: find, replace, lines, fields, patch, undo — covers grep, sed, awk columns/ranges, fix.lua batch, and undo"`.

```
saw
saw --help
saw <verb> --help
saw --version
saw find <pattern> [path...] [options]
saw replace <find> <with> [path...] [options]
saw lines <range> [path...] [options]
saw fields <spec> [path...] [options]
saw patch [options]
saw undo [tx-id] [options]
```

- `saw` with no arguments prints the banner and `HELP.md` via `jam::TerminalPage` and exits `0`.
- `saw --help` and `saw -h` print the banner and `HELP.md` and exit `0`.
- `saw <verb> --help` prints `Source/help/<verb>.md` via `jam::TerminalPage` and exits `0`. Each verb document is one separate binary data entry (`"One embedded markdown per verb (Source/help/find.md … undo.md) as separate binary entries, plus HELP.md for the overview. Uses the existing API with no section extraction"`).
- When stdout is a terminal, the three help invocations above erase the screen first with `jam::TerminalEncoder::writeEraseScreen`. The six verbs and `--version` never erase the screen (`"Erase only on the help/banner path (saw, --help, <verb> --help). Never on the six verbs or --version"`).
- `saw --version` and `saw -V` print `saw 0.0.1 (<commit>)` and exit `0` (`"v0.0.1"`).
- `--` terminates option parsing. Every token after `--` is a positional argument.

### 4.2 Exit Codes

| Code | Meaning |
|---|---|
| `0` | Success (match found on `find`/`fields`; dry-run preview completed; `--apply` succeeded; `undo` succeeded). |
| `1` | No match found on `find` (matches `grep` convention) or `--expect N` mismatch on a mutating verb. |
| `2` | Usage error, unknown flag, invalid regex, invalid JSON, missing file, binary file named directly, edit conflict, undo drift, or I/O error. |

### 4.3 Diagnostic Format

- All errors write to `stderr` with the prefix `saw: `.
- Successful command data, dry-run diffs, and the summary lines (`saw: dry-run …`, `saw: applied …`, `saw: undone …`, `saw: dry-run undo …`, `saw: no content changed; nothing written`) write to `stdout`.
- Each error writes one concise line to `stderr` and exits with code `1` or `2` per §4.2.

### 4.4 Input Sources and Stdin Rules

Decisions:
- `"Literal default + -r regex (extend jam::RegExp with findNext/spans) + recursive dir walk skipping .git/.gitignore"`
- `"Preserve each file's LF/CRLF line endings and execute bit; follow symlinks to target (matching cast/scout)"`
- `"Read stdin when no path is given and stdin is not a TTY (for find, lines, fields, dry-run replace); patch reads stdin or --file"`

Rules:
- **Explicit file paths:** Resolve relative paths against the current working directory. Resolve symbolic links to their target with `juce::File::getLinkedTarget()`. Ignore rules (§4.4.1) never skip an explicit file path.
- **Explicit directory paths:** Walk recursively in parallel across top-level subdirectories (`Paths::applyToDirectory`, using POSIX `opendir`/`readdir` on macOS/Linux and non-recursive `juce::RangedDirectoryIterator` on Windows), and process files in parallel via `Jobs::run` (fused single-pass walk+search in `Find`).
  - Skip `.git` entries always.
  - Skip files and directories that the ignore rules (§4.4.1) exclude, unless `--no-ignore` is set. Never descend into an excluded directory.
  - Skip dot-prefixed entries (hidden files and folders) unless `--hidden` is set.
  - Sort all collected file paths in ascending byte-lexicographic UTF-8 order before emitting output or writing.
- **Piped stdin:** When no `path` argument is given and stdin is not a TTY:
  - `find`, `lines` (read mode), `fields`, and `replace` (dry-run mode) read all bytes from stdin as a virtual file named `<stdin>`.
  - Passing `--apply` with stdin input fails with exit `2`: `saw: cannot --apply on stdin; specify file paths`.
- **No path and stdin is a TTY:**
  - `find` walks the current working directory `.`.
  - `replace`, `lines`, and `fields` fail with exit `2`: `saw: missing path argument`.
  - `patch` reads from `--file <path>` when given, otherwise fails with exit `2`: `saw: patch requires stdin or --file <path>`.

#### 4.4.1 Ignore Rules

Decisions:
- `"Full git semantics, including the global excludesFile and .git/info/exclude"`
- `"Ignore sources: every .gitignore from the repo root down to the path, $GIT_DIR/info/exclude, and core.excludesFile. Precedence per git-scm gitignore DESCRIPTION"`
- `"core.excludesFile lookup: $XDG_CONFIG_HOME/git/config, then ~/.gitconfig (last value wins), with ~/ expansion. Default $XDG_CONFIG_HOME/git/ignore, else ~/.config/git/ignore"`

Source: git-scm.com/docs/gitignore (DESCRIPTION, PATTERN FORMAT) and git-scm.com/docs/git-config (FILES, Syntax, `core.excludesFile`, pathname values).

**Top level.** The top level is the nearest ancestor of the walk root, the walk root included, that holds an entry named `.git`. When no ancestor holds one, the walk root is the top level.

**Sources, from highest to lowest precedence.** Within one level, the last matching pattern decides the outcome.

1. Each `.gitignore` file from the top level down to the directory of the path. A pattern in a lower-level file overrides a pattern in a higher-level file. Each pattern matches relative to the folder of its `.gitignore` file.
2. `<top level>/.git/info/exclude`, when that file exists.
3. The file that `core.excludesFile` names.

Patterns from sources 2 and 3 act as if they were at the top level. A leading `/` in them anchors at the top level.

**`core.excludesFile` lookup.**

1. Read `$XDG_CONFIG_HOME/git/config`. When `XDG_CONFIG_HOME` is not set or is empty, read `$HOME/.config/git/config`.
2. Then read `~/.gitconfig`.
3. The last `excludesFile` value in a `[core]` section wins. Section names and key names compare case-insensitively.
4. A value that starts with `~/` expands `~` to `$HOME`.
5. When no file sets the key, use `$XDG_CONFIG_HOME/git/ignore`. When `XDG_CONFIG_HOME` is not set or is empty, use `$HOME/.config/git/ignore`.
6. SAW reads no other configuration file. SAW does not follow `[include]` or `[includeIf]`. SAW reads no system or repository configuration.

**Configuration syntax that SAW reads.** A section header is `[name]` or `[name "subsection"]`. A variable line is `name = value`, or `name` alone. `#` and `;` start a comment to the end of the line. Whitespace around the name, the `=`, and the value is discarded. A value in double quotes keeps its whitespace and decodes `\"`, `\\`, `\n`, `\t`, and `\b`. A backslash at the end of a line continues the value on the next line.

**Pattern format.**

- A blank line matches no file.
- A line that starts with `#` is a comment. `\#` starts a pattern with a literal `#`.
- Trailing spaces are ignored unless a backslash quotes them.
- A backslash escapes the next character. A backslash at the end of a pattern makes a pattern that never matches.
- A leading `!` negates the pattern. A path that a previous pattern excluded becomes included again. A file cannot be included again when a parent directory of the file is excluded. `\!` starts a pattern with a literal `!`.
- A `/` at the beginning or in the middle makes the pattern relative to the folder of its source. Otherwise the pattern can match at any level below that folder.
- A `/` at the end makes the pattern match directories only.
- `*` matches anything except `/`. `?` matches one character except `/`. `[a-zA-Z]` matches one character of the range.
- A leading `**/` matches in all directories. A trailing `/**` matches everything inside. `/**/` matches zero or more directories. Other consecutive asterisks act as single asterisks.
- Matching is case-sensitive. SAW compares paths with `/` as the separator on every platform.

### 4.5 Binary Files, Empty Files, Line Endings, and Permissions

Decisions:
- `"Skip binary files (NUL byte) in directory walks; fail with an explicit error when named directly"`
- `"Preserve each file's LF/CRLF line endings and execute bit; follow symlinks to target (matching cast/scout)"`

Rules:
- **Binary detection:** Scan the full mapped buffer with `jam::simd::findByte (data, size, 0, '\0')`. Any file containing a NUL byte (`0x00`) is binary.
  - During a directory walk: skip binary files silently.
  - When named explicitly on the command line or in a `patch` entry: abort before any write and exit `2` with `saw: binary file: <path>`.
- **Empty (0-byte) files:** `juce::MemoryMappedFile` returns `getData() == nullptr` and `getSize() == 0`. Treat as a valid empty text buffer with 0 lines and no error.
- **Line endings:** Detect the file's dominant line ending from its first newline (`\r\n` vs `\n`). Default to `\n` for single-line or empty files. Insertions and line replacements normalize `\n` in user-supplied text to the file's detected line ending.
- **Trailing newline:** Preserve whether the file ends with a newline unless a line deletion removes the final line.
- **Permissions:** On macOS, read POSIX execute mode bits before swap and restore the original file mode on the `juce::TemporaryFile` before calling `overwriteTargetFileWithTemporary()`.

### 4.6 Line Hash Format (`--hash`)

- Line content hash is a 4-character lowercase hexadecimal prefix of the line's SHA-256 digest computed over the line text without its trailing `\r` or `\n`.
- Formatted prefix for line `L` with hash `hhhh`: `<L>:<hhhh>|<line-text>`.
- Example: `42:a7c1|    return result;`

### 4.7 Unified Diff Format (Dry-Run Preview)

Decision: `"GNU-exact. ',1' is omitted for one-line ranges. An empty range uses the GNU start rule. '\ No newline at end of file' marks incomplete lines. Hunks merge when their 3-line contexts overlap. Output stays valid input for patch and `git apply`"`.

Every mutating dry-run writes unified diff hunks to `stdout`:

```
--- <path>
+++ <path> (planned)
@@ -<oldStart>[,<oldCount>] +<newStart>[,<newCount>] @@
 <context>
-<removed>
+<added>
```

- Context is 3 unchanged lines around each change.
- Two changes whose context ranges overlap or touch go into one hunk.
- A range that holds exactly one line writes its start line only: `@@ -1 +1 @@`.
- A range that holds zero lines writes `<line>,0`, where `<line>` is the number of the line before the range: an insertion after line 3 writes `@@ -3,0 +4 @@`, and an insertion into an empty file writes `@@ -0,0 +1 @@`.
- A last line that has no line ending is followed by the line `\ No newline at end of file`.
- Each diff line carries the original line text without its line ending. Each diff line ends with `\n`.
- Runtime evidence for these rules: `diff -U0` and `diff -u` output of the system `diff` ("Apple diff (based on FreeBSD diff)") on 2026-11-22, which writes `@@ -3,0 +4 @@`, `@@ -0,0 +1 @@`, `@@ -1 +1 @@`, and `\ No newline at end of file`.
- After all file hunks, dry-run prints one summary footer line:
  - When edits > 0: `saw: dry-run — <E> edit(s) in <F> file(s); nothing written. Pass --apply --expect <E> to write.`
  - When edits == 0: `saw: dry-run — 0 edits; nothing written.`

### 4.8 Pattern Matching

Decisions:
- `"Literal default + -r regex (extend jam::RegExp with findNext/spans) + recursive dir walk skipping .git/.gitignore"`
- `"Per line: find, replace, fields --match run the regex on each line's text without its line ending. Matches never cross a newline (sed/grep model). -i wraps the pattern as (?i:...)"`

Rules:
- `find`, `replace`, and `fields --match` match each line on its own. The line text excludes its `\n` or `\r\n` ending. No match crosses a line ending.
- **Literal mode (default):** the pattern is exact bytes. `-i` compares ASCII letters `A`–`Z` and `a`–`z` without case. Every other byte compares exactly.
- **Regex mode (`-r`):** the pattern is a `jam::RegExp` pattern. `^` matches at the start of the line text. `$` matches at the end of the line text.
- **Regex mode with `-i`:** SAW compiles `(?i:` + pattern + `)`.
- An invalid regex fails with exit `2`: `saw: invalid regex: <detail>`, where `<detail>` is the text of the parser error.

---

## 5. Feature Specifications

### 5.1 Feature: `find`

Search files or stdin for literal strings or regex patterns (`"Literal default + -r regex (extend jam::RegExp with findNext/spans) + recursive dir walk skipping .git/.gitignore"`).

#### Invocation and Options

```
saw find <pattern> [path...] [options]
```

| Option | Effect |
|---|---|
| `-r`, `--regex` | Interpret `<pattern>` as a `jam::RegExp` pattern instead of literal bytes. |
| `-i`, `--ignore-case` | Match ASCII letters case-insensitively. |
| `-x`, `--exact` | Match only when `<pattern>` covers the entire line (excluding line ending). |
| `-v`, `--invert` | Emit lines that do not match `<pattern>`. |
| `-c`, `--count` | Print `<path>:<count>` (or `<count>` for single input) instead of matching lines. |
| `-q`, `--quiet` | Emit nothing to stdout; exit `0` on first match or `1` on no match. |
| `-n`, `--line-number` | Prefix output with 1-based line numbers (default: on). |
| `--no-line-number` | Omit line numbers from output. |
| `--hash` | Prefix each matched line with `<line>:<hhhh>|` instead of `<line>:`. |
| `-C <N>`, `--context <N>` | Show `N` lines of context before and after each match (`N >= 0`). |
| `--after <N>` | Inspect only lines with 1-based line number strictly greater than `N` (`line > N`). |
| `--lines <A:B>` | Restrict search to 1-based inclusive line range `A` through `B`. |
| `--first` | Stop after the first match per input file (or first overall when one input). |
| `--include <glob>` | Include only files whose name or relative path matches `<glob>`. Repeatable. |
| `--exclude <glob>` | Exclude files whose name or relative path matches `<glob>`. Repeatable. |
| `--hidden` | Include dot-prefixed files and directories in directory walks. |
| `--no-ignore` | Do not apply `.gitignore` filters in directory walks. |

#### Happy Path

1. User runs `saw find "DEBT-" DEBT.md`.
2. SAW memory-maps `DEBT.md`, checks for NUL bytes, and scans for `"DEBT-"` via `jam::simd::findByte` + `std::memcmp`.
3. SAW computes 1-based line numbers lazily for matched byte offsets.
4. SAW writes `<line>:<text>` (single file) or `<path>:<line>:<text>` (multiple files or directory walk) to stdout and exits `0`.

#### Edge Cases

- **No match found:** Write nothing to stdout (or `0` with `-c`) and exit `1`.
- **Empty pattern `""`:** Reject with exit `2`: `saw: pattern cannot be empty`.
- **Invalid regex with `-r`:** Reject with exit `2`: `saw: invalid regex: <detail>`.
- **Multiple matches on one line:** Emit that line once. With `-c`, count matching lines (same as `grep -c`).
- **`--after N` beyond EOF:** Emit nothing and exit `1`.
- **Line without trailing newline at EOF:** Match and emit normally, terminating stdout line with `\n`.

#### Error Handling

| Condition | Stderr Message | Exit |
|---|---|---|
| Missing `<pattern>` | `saw: find requires <pattern>` | `2` |
| Empty `<pattern>` | `saw: pattern cannot be empty` | `2` |
| Invalid regex | `saw: invalid regex: <detail>` | `2` |
| Invalid `-C` or `--after` integer | `saw: invalid integer for <flag>: <value>` | `2` |
| Named path does not exist | `saw: not found: <path>` | `2` |
| Named file is binary | `saw: binary file: <path>` | `2` |

#### Acceptance Criteria

- [ ] `saw find "needle" file.txt` finds literal matches without escaping regex metacharacters.
- [ ] `saw find -x "---" --after 12 --first DEBT.md` returns the exact line number of the first `---` after line 12.
- [ ] `saw find -q -x "entry" .gitignore` exits `0` when present and `1` when absent with empty stdout.
- [ ] `saw find -v -x "entry" .gitignore` emits every line except exact `"entry"`.
- [ ] A directory walk with parallel per-file work emits identical output order across repeated runs.

---

### 5.2 Feature: `replace`

Replace literal strings or regex matches across one or more files, with dry-run by default and explicit two-gate apply (`"--apply always requires --expect N (dry-run prints N; mismatch writes 0 bytes)"`).

#### Invocation and Options

```
saw replace <find> <with> [path...] [options]
```

| Option | Effect |
|---|---|
| `-r`, `--regex` | Interpret `<find>` as `jam::RegExp`. Expand `$0`..`$9` captures and `$$` (literal `$`) in `<with>`. |
| `-i`, `--ignore-case` | Match ASCII letters case-insensitively. |
| `--lines <A:B>` | Restrict replacements to 1-based inclusive line range `A` through `B`. |
| `--with-stdin` | Read `<with>` payload verbatim from stdin (requires explicit file paths; `<with>` positional arg omitted). |
| `--with-file <path>` | Read `<with>` payload verbatim from `<path>` (`<with>` positional arg omitted). |
| `--include <glob>` | Include only matching files when a directory path is given. Repeatable. |
| `--exclude <glob>` | Exclude matching files when a directory path is given. Repeatable. |
| `--apply` | Write planned edits to disk. Requires `--expect N`. |
| `--expect <N>` | Exact count of match replacements expected across all target files (`N >= 0`, integer: `"Exact edit operations applied (matches replaced in replace, edit entries in patch, line ops in lines)"`). |

#### Happy Path (Dry-Run Default)

1. User runs `saw replace "DBG" "debug::Log" src/Foo.cpp`.
2. SAW memory-maps `src/Foo.cpp`, locates all non-overlapping literal matches in left-to-right order, and builds the planned buffer in memory.
3. SAW prints unified diff hunks and summary: `saw: dry-run — 3 edit(s) in 1 file(s); nothing written. Pass --apply --expect 3 to write.`
4. Zero bytes change on disk. Exit `0`.

#### Happy Path (Apply)

1. User runs `saw replace "DBG" "debug::Log" src/Foo.cpp --apply --expect 3`.
2. SAW computes all planned buffers and counts total replacement operations (`actual == 3`).
3. Because `actual == expected`, SAW records an undo snapshot (§6), writes each modified file through `juce::TemporaryFile`, preserves permissions, swaps atomically, and verifies post-write SHA-256 hashes.
4. SAW prints `saw: applied 3 edit(s) in 1 file(s) (tx: <tx-id>)` and exits `0`.

#### Edge Cases

- **`--apply` without `--expect`:** Fail before touching disk with exit `2`: `saw: --apply requires --expect <N>`.
- **`--expect N` mismatch:** Print the dry-run unified diff to stdout, print `saw: expected <N> edit(s), found <actual>; nothing written` to stderr, write zero bytes, and exit `1`.
- **`--apply --expect 0` when 0 matches exist:** Write zero files, create no undo transaction, print `saw: applied 0 edit(s) in 0 file(s)`, and exit `0`.
- **`<find>` equals `<with>` (no-op replacement):** Count matches normally on dry-run; with `--apply`, skip disk write for byte-identical files and print `saw: no content changed; nothing written` (exit `0`).
- **Overlapping matches (e.g. `"aa"` in `"aaa"`):** Advance cursor past the end of each matched span (non-overlapping, left-to-right; `"aaa"` yields 1 match).
- **Regex zero-length match (e.g. `^` or `$`):** Apply at current position and advance cursor by 1 byte to prevent infinite loops.
- **Invalid capture reference (`$5` when regex has 2 groups):** Fail before any write with exit `2`: `saw: invalid capture group $5 in replacement`.
- **Target file modified externally during apply:** Roll back all written files from pre-edit snapshots and exit `2`.

#### Error Handling

| Condition | Stderr Message | Exit |
|---|---|---|
| Empty `<find>` | `saw: pattern cannot be empty` | `2` |
| `--apply` without `--expect` | `saw: --apply requires --expect <N>` | `2` |
| `--expect` not a non-negative integer | `saw: invalid integer for --expect: <value>` | `2` |
| `--expect N` mismatch | `saw: expected <N> edit(s), found <actual>; nothing written` | `1` |
| `--apply` on stdin | `saw: cannot --apply on stdin; specify file paths` | `2` |
| Named file is binary | `saw: binary file: <path>` | `2` |
| Invalid regex | `saw: invalid regex: <detail>` | `2` |
| Invalid capture group | `saw: invalid capture group $<k> in replacement` | `2` |
| File not writable or swap failed | `saw: write failed for <path>: <reason>; rolled back` | `2` |

#### Acceptance Criteria

- [ ] Default `saw replace` writes zero bytes on disk and prints unified diff plus exact `--apply --expect N` hint.
- [ ] `saw replace "a/b" "c/d" file.txt --apply --expect 2` requires zero slash escaping.
- [ ] `saw replace -r "^model: .*" "model: opus" agent.md --apply --expect 1` works identically on macOS and Windows.
- [ ] Multi-file failure on file 2 restores file 1 to bit-identical pre-edit bytes.

---

### 5.3 Feature: `lines`

Read, delete, replace, or insert exact 1-based line ranges in one or more files.

#### Range Grammar

| Form | Meaning |
|---|---|
| `A` | Single 1-based line `A`. |
| `A:B` | Inclusive 1-based lines `A` through `B` (`A <= B`). |
| `A:` | From line `A` through the final line of the file. |
| `:B` | From line `1` through line `B`. |

#### Invocation and Options

```
saw lines <range> [path...] [options]
```

| Option | Mode | Effect |
|---|---|---|
| `-n`, `--line-number` | Read | Prefix each line with `<line>\t`. |
| `--hash` | Read | Prefix each line with `<line>:<hhhh>|` (§4.6). |
| `--delete` | Mutate | Delete all lines in `<range>`. |
| `--replace <text>` | Mutate | Replace lines in `<range>` with `<text>`. |
| `--insert-before <text>` | Mutate | Insert `<text>` before line `A` (requires single line `A`). |
| `--insert-after <text>` | Mutate | Insert `<text>` after line `A` (requires single line `A`, or `0` to prepend at top of file). |
| `--stdin` | Mutate | Read replacement or inserted text verbatim from stdin (omits inline `<text>`). |
| `--verify-hash <hhhh>` | Mutate | Require line `A` to have content hash `<hhhh>` before mutating. |
| `--apply` | Mutate | Write planned edits to disk. Requires `--expect N`. |
| `--expect <N>` | Mutate | Exact count of line operations applied (`"Exact edit operations applied (matches replaced in replace, edit entries in patch, line ops in lines)"`). |

Mutating flags (`--delete`, `--replace`, `--insert-before`, `--insert-after`) are mutually exclusive.

#### Happy Path (Read)

1. User runs `saw lines 10:25 --hash Source/Main.cpp`.
2. SAW memory-maps `Source/Main.cpp`, locates lines 10 through 25 using `jam::simd::findByte` on `\n`, computes 4-hex SHA-256 hashes per line, writes `10:a1b2|...` through `25:c3d4|...`, and exits `0`.

#### Happy Path (Mutate)

1. User runs `saw lines 15:22 --delete DEBT.md` → prints unified diff and `saw: dry-run — 1 edit(s) in 1 file(s); nothing written. Pass --apply --expect 1 to write.`
2. User runs `saw lines 15:22 --delete DEBT.md --apply --expect 1` → removes lines 15–22 atomically, saves undo snapshot, prints `saw: applied 1 edit(s) in 1 file(s) (tx: <tx-id>)`, exits `0`.

#### Edge Cases

- **Range out of bounds on read:** If `A > lineCount`, emit nothing and exit `0`. If `B > lineCount`, clamp `B` to `lineCount`.
- **Range out of bounds on mutate:** Fail before any write with exit `2`: `saw: line range <range> out of bounds for <path> (<lineCount> lines)`.
- **`--insert-after 0` on empty 0-byte file:** Valid; inserts text as line 1 of the file (counts as 1 edit).
- **`--insert-before` / `--insert-after` with range `A:B`:** Fail with exit `2`: `saw: insert requires a single line number, got <range>`.
- **`--verify-hash` mismatch:** Fail before any write with exit `2`: `saw: hash mismatch at <path>:<A>: expected <expected>, got <actual>`.
- **Multiple mutating flags together:** Fail with exit `2`: `saw: specify at most one of --delete, --replace, --insert-before, --insert-after`.
- **Pure insertion (`--insert-after`):** Counts as `1` edit (fixes `diff | grep -c '^<'` counting `0`).

#### Error Handling

| Condition | Stderr Message | Exit |
|---|---|---|
| Invalid range syntax or `A > B` or `A < 1` (except `0` for `--insert-after`) | `saw: invalid line range: <range>` | `2` |
| Out-of-bounds range on mutate | `saw: line range <range> out of bounds for <path> (<lineCount> lines)` | `2` |
| Hash precondition failed | `saw: hash mismatch at <path>:<line>: expected <expected>, got <actual>` | `2` |
| `--apply` without `--expect` | `saw: --apply requires --expect <N>` | `2` |
| `--expect` mismatch | `saw: expected <N> edit(s), found <actual>; nothing written` | `1` |

#### Acceptance Criteria

- [ ] `saw lines 1:10 DEBT.md` replaces `sed -n '1,10p' DEBT.md`.
- [ ] `saw lines 15:22 --delete DEBT.md --apply --expect 1` replaces `sed '15,22d' DEBT.md`.
- [ ] `saw lines 4 --insert-after --stdin DEBT.md --apply --expect 1` inserts multiline text cleanly and counts as 1 edit.
- [ ] `saw lines 10:20 --hash file.cpp` emits deterministic 4-hex content hashes matching `patch` and `--verify-hash`.

---

### 5.4 Feature: `fields`

Extract and format delimited columns from lines (replaces file-oriented `awk -F` and `cut`). Read-only.

#### Invocation and Options

```
saw fields <spec> [path...] [options]
```

- `<spec>`: Comma-separated 1-based field indices. Negative indices count from the end (`-1` equals `awk`'s `$NF`, `-2` equals `$(NF-1)`). Example: `1,3,-1`.

| Option | Effect |
|---|---|
| `-d <delim>`, `--delimiter <delim>` | Split lines on literal `<delim>`. When omitted, split on runs of ASCII space/tab and trim leading/trailing whitespace (matches `awk` default). |
| `-o <out>`, `--output-delimiter <out>` | Join extracted fields with `<out>`. Defaults to single space when `-d` is omitted, or `<delim>` when `-d` is given. |
| `--match <pattern>` | Process only lines matching `<pattern>` (literal default; `-r` for regex). |
| `-r`, `--regex` | Interpret `--match <pattern>` as `jam::RegExp`. |
| `--lines <A:B>` | Restrict processing to 1-based line range `A:B`. |
| `--after <N>` | Process only lines after line `N`. |
| `--first` | Stop after the first emitted output line per file. |
| `--strict` | Fail with exit `2` if any processed line lacks a requested field index. Default: omit missing fields as empty strings. |

#### Happy Path

1. User runs `saw fields 1,-1 -d ":" /path/to/file.txt`.
2. SAW splits each line on `:`, selects field 1 and the last field, joins with `:`, writes to stdout, and exits `0`.

#### Edge Cases

- **Empty line with default whitespace delimiter:** Produces 0 fields; emits empty line (or fails under `--strict`).
- **Index `0` in `<spec>`:** Fail with exit `2`: `saw: invalid field spec: <spec> (1-based or negative)`.
- **Negative index magnitude exceeds field count:** Emits empty string (or fails under `--strict` with `saw: field <idx> out of range at <path>:<line>`).
- **No lines matched `--match`:** Emit nothing and exit `1`.

#### Error Handling

| Condition | Stderr Message | Exit |
|---|---|---|
| Invalid `<spec>` | `saw: invalid field spec: <spec>` | `2` |
| Empty `-d ""` | `saw: delimiter cannot be empty` | `2` |
| Missing field under `--strict` | `saw: field <idx> out of range at <path>:<line>` | `2` |
| Named file is binary | `saw: binary file: <path>` | `2` |

#### Acceptance Criteria

- [ ] `saw fields 2 -d '"' version.txt` extracts quoted field 2 identically on macOS and Windows.
- [ ] `saw fields 1,-1 data.txt` splits on whitespace runs and returns first and last column without `awk`.
- [ ] `--match` + `--first` extracts the target field in one single pass.

---

### 5.5 Feature: `patch`

Apply a structured, multi-file batch of exact column-span, line-range, delete, or insert edits in one atomic transaction (`"Read stdin when no path is given and stdin is not a TTY (for find, lines, fields, dry-run replace); patch reads stdin or --file"`).

#### Invocation and Options

```
saw patch [options]
```

| Option | Effect |
|---|---|
| `--file <path>` | Read JSON edit batch from `<path>` instead of stdin. |
| `--apply` | Write planned edits to disk. Requires `--expect N`. |
| `--expect <N>` | Exact number of edit entries in the JSON array (`N == array.length`: `"Exact edit operations applied (matches replaced in replace, edit entries in patch, line ops in lines)"`). |

#### Batch JSON Schema

Input is a JSON array of edit objects (validated via `jam::JsonDocument`). Every object requires `"file"` (string path) and one of four mutually exclusive operation shapes:

1. **Column-span edit** (matches `fix.lua:26-60`, 1-based line, 0-based byte columns `[startCol, endCol)`):
   ```json
   { "file": "src/Foo.cpp", "line": 42, "startCol": 8, "endCol": 10, "text": "not " }
   ```
   Optional precondition: `"hash": "a7c1"` (verified against original line 42 before applying).

2. **Line delete** (1-based single line or inclusive range):
   ```json
   { "file": "src/Foo.cpp", "line": 12, "delete": true }
   ```
   or `{ "file": "src/Foo.cpp", "startLine": 12, "endLine": 15, "delete": true }` with optional `"hash"` on the starting line.

3. **Line-range replace** (1-based inclusive):
   ```json
   { "file": "src/Foo.cpp", "startLine": 10, "endLine": 12, "text": "new line 1\nnew line 2", "hash": "a7c1" }
   ```

4. **Line insert** (1-based line; `0` allowed for `insertAfter` to prepend at top):
   ```json
   { "file": "src/Foo.cpp", "insertAfter": 10, "text": "// banner", "hash": "a7c1" }
   ```
   or `"insertBefore": 10`.

#### Validation and Application Rules

1. Parse and validate the entire JSON array before reading or writing any file.
2. Check `--expect N` against the total number of edit entries (`entries.size()`).
3. Group edits by resolved target file path (preserving deterministic sorted file order).
4. For each target file, verify all bounds (`line`, `startLine <= endLine`, `0 <= startCol <= endCol <= lineByteLength`), verify optional `"hash"` against original lines, and reject conflicts:
   - Two column-span edits on the same line with `prev.endCol > curr.startCol` or `prev.startCol == curr.startCol`.
   - Any column-span edit combined with a delete or range-replace on the same line.
   - Overlapping line ranges (`delete` or `replace`) in the same file.
5. Apply non-overlapping column-span edits on each line right-to-left (highest `startCol` first), then assemble the full planned file buffer in memory.
6. **Dry-run (default):** Print unified diff hunks for every modified file + summary `saw: dry-run — <E> edit(s) in <F> file(s); nothing written. Pass --apply --expect <E> to write.` Exit `0`.
7. **`--apply --expect N`:** Record one undo transaction covering all touched files, write and swap every file atomically via `juce::TemporaryFile`, verify post-write SHA-256 hashes, print `saw: applied <E> edit(s) in <F> file(s) (tx: <tx-id>)`, exit `0`.

#### Edge Cases

- **Empty JSON array `[]`:** Dry-run prints `saw: dry-run — 0 edits; nothing written.` (exit `0`). `--apply --expect 0` writes nothing and exits `0`.
- **Same file referenced via relative path and symlink:** Resolve with `juce::File::getLinkedTarget()` so all edits to the same canonical file group together and check conflicts accurately.
- **Cross-file rename batch (e.g. `fix.lua` + `ling`):** All touched files apply in one transaction and roll back together if any single file fails.
- **Multi-line `"text"` inside column-span edit:** Allowed (matches `namespace-enclosure-format.lua` inserting `\n` on a line). Line endings normalize to the file's detected line ending.

#### Error Handling

| Condition | Stderr Message | Exit |
|---|---|---|
| Invalid JSON or root is not an array | `saw: invalid patch JSON: <detail>` | `2` |
| Missing/unknown/conflicting fields on entry `i` | `saw: invalid patch entry <i>: <reason>` | `2` |
| Overlapping edits on same line/range | `saw: conflicting edits in <path> at line <line>` | `2` |
| Column or line out of bounds | `saw: out of bounds in <path> at entry <i>` | `2` |
| Hash precondition failed | `saw: hash mismatch at <path>:<line>: expected <expected>, got <actual>` | `2` |
| `--apply` without `--expect` | `saw: --apply requires --expect <N>` | `2` |
| `--expect N` mismatch | `saw: expected <N> edit(s), found <actual>; nothing written` | `1` |
| Named file missing or binary | `saw: not found: <path>` / `saw: binary file: <path>` | `2` |

#### Acceptance Criteria

- [ ] `fix.lua` applies an entire multi-file batch in one `saw patch --apply --expect N` invocation with zero `sed` or `diff` subprocesses.
- [ ] Overlapping column spans on line `L` fail cleanly with exit `2` and zero bytes written.
- [ ] A write failure on file 3 of a 5-file batch restores files 1 and 2 to exact pre-edit bytes.

---

### 5.6 Feature: `undo`

Inspect and safely reverse applied SAW transactions from the bounded rolling undo cache (`"Rolling cache in app-data dir (cap 32 transactions, verifies post-edit hashes before restoring)"`).

#### Invocation and Options

```
saw undo [tx-id] [options]
```

| Option | Effect |
|---|---|
| `--list` | Print stored transactions (newest first) as `<tx-id>\t<timestamp>\t<files> file(s)\t<command>` and exit `0`. |
| `--apply` | Execute the restore on disk. Without `--apply`, `saw undo` runs as dry-run and prints the reverse unified diff. |
| `--force` | Skip post-edit hash verification on `--apply` (explicit override when user chooses to overwrite newer disk edits). |

#### Happy Path (Dry-Run Default)

1. User runs `saw undo` (selects the latest active transaction) or `saw undo <tx-id>`.
2. SAW loads `<undo-root>/<tx-id>/meta.json` and verifies each target file's current SHA-256 matches `postHash`.
3. SAW prints the reverse unified diff (`current -> original`) and summary: `saw: dry-run undo (<tx-id>) — <F> file(s); nothing written. Pass --apply to restore.`
4. Exit `0`.

#### Happy Path (Apply)

1. User runs `saw undo --apply` or `saw undo <tx-id> --apply`.
2. SAW verifies current disk SHA-256 equals `postHash` for every file in the transaction (unless `--force`).
3. SAW restores every file's original bytes and POSIX execute bit atomically via `juce::TemporaryFile`. If file `k + 1` fails, files `0..k` revert to their pre-undo bytes.
4. SAW removes the restored transaction directory from the undo cache.
5. SAW prints `saw: undone <tx-id> — restored <F> file(s)` and exits `0`.

#### Edge Cases

- **No transactions stored:** Print `saw: no undo transactions available` to stderr and exit `2` (for `--list`, print nothing and exit `0`).
- **Unknown `<tx-id>`:** Print `saw: unknown transaction: <tx-id>` to stderr and exit `2`.
- **File modified after `--apply` (hash drift):** Refuse restoration before writing any file, print `saw: cannot undo <tx-id>: <path> changed since apply (pass --force to overwrite)` to stderr, and exit `2`.
- **Target file deleted after `--apply`:** Under default check, fail with hash drift (`<path> missing since apply`). Under `--force`, recreate the file with original bytes via `juce::TemporaryFile`.

#### Error Handling

| Condition | Stderr Message | Exit |
|---|---|---|
| Undo cache empty | `saw: no undo transactions available` | `2` |
| Unknown `<tx-id>` | `saw: unknown transaction: <tx-id>` | `2` |
| Corrupt `meta.json` or missing blob | `saw: corrupt transaction <tx-id>: <detail>` | `2` |
| File drifted since `--apply` | `saw: cannot undo <tx-id>: <path> changed since apply (pass --force to overwrite)` | `2` |
| Restore write failure | `saw: undo failed for <path>: <reason>; rolled back` | `2` |

#### Acceptance Criteria

- [ ] `saw undo` defaults to dry-run and writes zero bytes.
- [ ] `saw undo --apply` restores bit-identical original files and POSIX execute bits after any `replace`, `lines`, or `patch` apply.
- [ ] Editing a file externally after `saw replace --apply` causes `saw undo --apply` to refuse with exit `2` and leave the newer edit intact.
- [ ] Rolling cache never exceeds 32 stored transactions.

---

## 6. Transaction and Undo Cache Model

Decision: `"Rolling cache in app-data dir (cap 32 transactions, verifies post-edit hashes before restoring)"`.

### 6.1 Cache Location and Bound

- **Root path:** `jam::File::getUserApplicationDataDirectory().getChildFile ("saw").getChildFile ("undo")`.
  - macOS: `~/Library/Application Support/saw/undo/`
  - Windows: `%APPDATA%\saw\undo\`
- **Bound:** Maximum `32` transaction directories. After each successful `--apply`, SAW deletes the oldest directories beyond 32 (ordered by UTC timestamp prefix).

### 6.2 Transaction Directory Layout

Each transaction owns one immutable directory named `<YYYYMMDDTHHMMSS>-<6-hex-id>`:

```
<undo-root>/20261122T103000-a3f9c1/
  meta.json
  0.blob
  1.blob
```

`meta.json` fields:
- `"id"`: string transaction ID.
- `"timestamp"`: ISO-8601 UTC string.
- `"command"`: short verb summary (e.g. `"replace"`, `"lines"`, `"patch"`).
- `"files"`: array of `{ "blob": "0.blob", "path": "<canonical-absolute-path>", "preHash": "<sha256>", "postHash": "<sha256>", "executable": true|false }`.

### 6.3 Two-Phase Commit Protocol

1. **Plan phase (memory only):** Map and read every target file, verify preconditions (`--expect N`, hashes, bounds, non-binary), compute planned buffers, `preHash`, and `postHash`. Any error aborts with zero disk touches.
2. **Stage phase:** Create transaction directory in `<undo-root>`, write every `<i>.blob` (exact pre-edit bytes), write `meta.json` via `juce::TemporaryFile`.
3. **Commit phase:** For each file `0..M-1`:
   - Write planned bytes to `juce::TemporaryFile (target, TemporaryFile::useHiddenFile)` via `FileOutputStream`. Verify `write()` and `flush()` succeed.
   - Copy original execute permission bit to temp file on macOS.
   - Call `overwriteTargetFileWithTemporary()`.
   - Re-read and verify target SHA-256 equals `postHash`.
   - On any failure at index `k`: immediately restore files `0..k-1` from `0.blob..(k-1).blob` via `juce::TemporaryFile`, delete the staged transaction directory, and exit `2`.
4. **Prune phase:** Delete oldest transaction directories beyond 32.

---

## 7. CAROL Integration Contract (Downstream — Outside SAW Binary)

Decision: `"sed awk immediately rejected hooks, python and lua must ask permission. editing file no need nvim. but use in-house cli as must use tools."`

Ownership: `"carol init on saw repo by ARCHITECT"`. ARCHITECT runs this wiring. The SAW implementation plan does not include it.

SAW contains zero CAROL-specific logic. After SAW builds and installs to `~/.local/bin/saw`, CAROL wires SAW in one follow-up pass:

1. **`~/.carol/hooks/register.js` and `~/.carol/agy/hooks/agy-gate.mjs`:**
   - Deny `sed` and `awk` invocations in Bash and direct agents to `saw`.
   - Add `saw find` to `TEXT_SEARCH_PATTERN` so C++ source files remain governed by `doxygen-protocol` (only string literals `"` and `#` directives allowed).
2. **`~/.config/claude/settings.json` (`permissions.ask`):**
   - Add `Bash(python*)`, `Bash(python3*)`, and `Bash(lua*)` so disposable script runners require explicit ARCHITECT approval.
3. **`~/.carol/bin/carol`:**
   - Delegate `cmd_apply` and internal `sed`/`awk`/`grep` file operations to `saw`.
4. **`~/.carol/local/janitor/fix.lua`:**
   - Replace `getSedScript`, `sed` dry-run, `diff` line counting, and `carol apply` (`fix.lua:82-192`) with one `saw patch --apply --expect N` invocation.

---

## 8. Global Error Reference

| Condition | Exact Stderr String | Exit |
|---|---|---|
| Unknown verb | `saw: unknown command: <verb>` | `2` |
| Unknown flag | `saw: unknown flag: <flag>` | `2` |
| Missing flag value | `saw: flag requires a value: <flag>` | `2` |
| Missing path argument | `saw: missing path argument` | `2` |
| Missing file or directory | `saw: not found: <path>` | `2` |
| Binary file named directly | `saw: binary file: <path>` | `2` |
| Empty search pattern | `saw: pattern cannot be empty` | `2` |
| Invalid regex | `saw: invalid regex: <detail>` | `2` |
| `--apply` without `--expect` | `saw: --apply requires --expect <N>` | `2` |
| `--apply` with stdin | `saw: cannot --apply on stdin; specify file paths` | `2` |
| `--expect` mismatch | `saw: expected <N> edit(s), found <actual>; nothing written` | `1` |
| Hash precondition mismatch | `saw: hash mismatch at <path>:<line>: expected <expected>, got <actual>` | `2` |
| Write or verify failure | `saw: write failed for <path>: <reason>; rolled back` | `2` |

---

## 9. Success Criteria

### Functional

- [ ] `saw`, `saw --help`, and `saw <verb> --help` render their documents via `jam::TerminalPage` and exit `0`.
- [ ] `saw --version` prints `saw 0.0.1 (<commit>)` and exits `0`.
- [ ] All 6 verbs (`find`, `replace`, `lines`, `fields`, `patch`, `undo`) pass every case of `test/cases.md` through the runner (§10) on macOS and Windows.
- [ ] Every mutating verb defaults to dry-run and touches zero bytes without `--apply`.
- [ ] `--apply` without `--expect N` fails with exit `2` and writes zero bytes on `replace`, `lines`, and `patch`.
- [ ] Every `--expect N` mismatch writes zero bytes and exits `1`.
- [ ] Multi-file failure during commit rolls back all prior files to bit-identical pre-edit bytes.
- [ ] `saw undo --apply` reverses any applied transaction cleanly and refuses drifted files unless `--force` is passed.
- [ ] `saw` preserves LF/CRLF line endings, trailing newline state, 0-byte files, symlink targets, and POSIX execute bits.
- [ ] Directory walks apply the ignore rules of §4.4.1.

### Performance and Quality

- [ ] Non-help commands run without `juce::ScopedJuceInitialiser_GUI`.
- [ ] Literal `find` and `replace` scan via `juce::MemoryMappedFile` + `jam::simd::findByte` with zero whole-file string copies on read.
- [ ] `jam::RegExp` exposes offset-based `findNext` returning match spans and captures. Runner cases on `find -r` and `replace -r` cover several matches on one line, capture groups, and zero-length matches.
- [ ] One scout report exists for each benchmark pair of §10.3.
- [ ] Downstream, after §7: JANITOR v2 full-contract benchmark (`fix.lua` calling `saw patch --apply --expect N`) matches `truth-v2` with `0` wrong edits and faster wall time than `sed` + `carol apply`.
- [ ] Zero BLESSED, `CODING.md`, or `NAMES.md` violations at post-implementation audit.

---

## 10. Test and Benchmark Harness

Decisions:
- `"why unit test still need lua? scout whatdbg now native mcp"`
- `"so what harness do we need to test? benchmark against sed awk grep rg"`
- `"design comparison runner then. do we need nvim?"`
- `"which is more deterministic? more objective? zsh or nvim with lua?"`
- `"ok nvim lua"`
- `"The runner moves the real <app-data>/saw/undo aside before the run and puts it back after, also on failure. saw gets no new surface"`
- `"A copy of ~/Documents/Poems/JUCE/modules. It is real C++ text, it is present on every machine (CMake's JUCE path is ../../JUCE), and it is copied to a temporary folder before each mutating epoch"`

### 10.1 Runner

- **Files:** `test/run.lua`, `test/cases.md`, `test/fixture/<fixture>/`, `test/stdin/<file>`, `test/baseline/<name>/`.
- **Invocation:** `nvim --headless -l test/run.lua <saw-binary>`. Neovim 0.10 or later. `-l` skips the user configuration and disables plugins.
- **Output:** one line per case: `PASS <name>` or `FAIL <name>: <first difference>`.
- **Exit code:** `0` when every case passes, `1` when one case fails. The runner calls `os.exit`.
- **Process calls:** `vim.system` with `text = false`, `stdin` set to a string, and a `cwd`. The runner always passes a stdin string, empty when the `stdin` cell is empty. Thus saw never sees a terminal on stdin in a test run. The terminal branches of §4.4 have no runner case.
- **Byte comparison:** Lua string `==`. Lua strings are 8-bit clean.
- **Undo isolation:** before the first case, the runner renames `<app-data>/saw/undo` to `<app-data>/saw/undo-runner-<pid>` when it exists. After the last case, also after a Lua error, the runner deletes the `undo` folder that the run made and renames the saved folder back. `<app-data>` is `$HOME/Library/Application Support` on macOS and `%APPDATA%` on Windows.

### 10.2 Case Manifest

`test/cases.md` holds one grid table under `## cases`. Each row is one line of text between two border lines. The runner reads each cell by the column offsets of the `+---+` border line.

| Column | Meaning |
|---|---|
| `name` | Case name. Rows with the same name are steps of one case. They run in row order, in one temporary folder. |
| `fixture` | Folder under `test/fixture/`. The runner copies it to a new temporary folder before the first step of a case. |
| `command` | The saw arguments. Whitespace separates arguments. A span in double quotes stays one argument, and the runner removes the quote marks. |
| `stdin` | File under `test/stdin/` whose bytes go to stdin, or empty. |
| `exit` | The expected exit code. |
| `reference` | Empty, or a reference command (program and arguments, same split rule). |
| `compare` | Empty, `stdout`, or `file:<relative path>`. |

**Baseline mode (`reference` empty).**

1. The runner compares the stdout of step `<k>` (1-based) with `test/baseline/<name>/<k>/stdout`, and its stderr with `test/baseline/<name>/<k>/stderr`.
2. The runner compares the exit code with the `exit` cell.
3. After the last step, the runner compares the temporary folder with `test/baseline/<name>/files/`, file by file, byte by byte. When `files/` does not exist, the expected folder is the fixture without change.
4. Before the comparison, the runner replaces each transaction id (`<8 digits>T<6 digits>-<6 hex digits>`) with `<tx-id>` and the temporary folder path with `<root>`.

**Reference mode (`reference` set).**

1. The runner copies the fixture again to a second temporary folder and runs the reference command there.
2. The reference process gets `clear_env = true` and an `env` with only `LC_ALL=C`, `PATH`, and `HOME` from the runner.
3. `compare = stdout`: saw stdout must equal reference stdout.
4. `compare = file:<path>`: the bytes of `<path>` in the saw folder after the run must equal reference stdout.
5. The saw exit code must equal the `exit` cell. The reference exit code must equal the `exit` cell when `compare = stdout`.
6. Reference commands use only syntax that BSD and GNU tools share. They never use `sed -i`, BRE `\+` or `\?`, or `grep -P`.

### 10.3 Benchmark

- **Tool:** `scout`. One manifest per pair in `benchmark/`, with `## run` keys `epochs` (11), `command`, `samples`, `output`, and `baseline`.
- **Command script:** a script per manifest. The script runs the workload once, then writes an empty samples file: `: > "$(dirname "$1")/<name>-samples.md"` (pattern of `scout/benchmark/mcp.sh:8`).
- **Corpus:** a copy of `~/Documents/Poems/JUCE/modules`. A mutating workload copies the corpus to a new temporary folder at the start of each epoch.
- **Baseline:** each saw manifest names the report of its reference tool as `baseline`. The saw report then holds `change` and `probability` columns.
- **Pairs:**

| Workload | saw | Reference |
|---|---|---|
| Literal search, tree | `saw find <literal> <corpus>` | `grep -rn <literal> <corpus>`, `rg -n <literal> <corpus>` |
| Regex search, tree | `saw find -r <regex> <corpus>` | `grep -rnE <regex> <corpus>`, `rg -n <regex> <corpus>` |
| Literal replace, tree | `saw replace <a> <b> <corpus> --apply --expect <N>` | `sed` with in-place edit on each corpus file |
| Line range, one file | `saw lines <A:B> <file>` | `sed -n '<A>,<B>p' <file>`, `awk 'NR>=<A> && NR<=<B>' <file>` |
| Fields, one file | `saw fields 1,-1 -d <d> <file>` | `awk -F<d> '{print $1 "<d>" $NF}' <file>` |
