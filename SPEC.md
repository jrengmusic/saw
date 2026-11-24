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
- **Premise:** `"must exclusively using saw, since it was the fastest, the most reliable, and the safest way to find, edit file?"`, `"we should fix saw first, so it live to its premise. then we could enforce the protocol."`
- **Platforms:** `"reminder, must be CROSSPLATFORM"`
- **Contract:** `"your job is to fix SAW contract, to replace sed, awk, grep, rg to be ALWAYS DETERMINISTICALLY CORRECT and FASTER on ALL CASES. NON-NEGOTIABLE, NO EXCEPTIONS."`, `"SPEED, SAFETY, CORRECTNESS is not mutually exclusive, ALL OF THEM ARE THE CONTRACT. there are no discussion about using mutex everywhere."`
- **Lock-Free:** `"why cant we have lock free architecture? lock free architecture isnt aspirational, ALL of our project are multi-thread with audio architecture discipline, lock free on hot path, PROVEN TO BE FASTEST"`, `"we have always use lock-free on hot path in this domain. FACTUALLY ALWAYS FASTEST AND BETTER"`
- **Excerpt Lane:** `"the current sed, awk, grep, rg parity STAYS"`, `"but we have a dedicated lane: formatted to format you EXACTLY need. so agents never wrongly formatted, return verbatim as is"`
- **Slim Linking:** `"we might be able to design jam_core properly for CLI/TUI project. for example, all of of our CLI actually bring markdown renderer and text editor, what if we have explicit macro to flag CLI/TUI so they can be more SLIM?"`

### 1.3 Explicit Non-Goals

- No daemon process. The CLI stays the single surface for scripts and for ARCHITECT (`"why mcp? not just cli? i meant one SSOT tools for all file manipulation."`). The server (§5.8) gives the same command lines to agents on demand (`"cli isn't enough? need lazy load mcp that never inject anything when idle? never eat context when not being used? always on demand? fine, add one."`). It is a child of its client on stdio, and it holds no state between calls.
- No embedded scripting language (`BEGIN`/`END` blocks, loops, associative arrays, or arithmetic expressions).
- No fuzzy matching, heuristic patching, or silent fallback.
- No version-control subprocess calls inside SAW.
- No AST parsing or language-specific linting inside SAW.

---

## 2. Technology Stack and Architecture Constraints

- **Language:** C++17 (`CMAKE_CXX_STANDARD 17`).
- **Project Skeleton:** Run `cast INIT.md ~/Documents/Poems/dev/saw` in `~/Documents/Poems/dev/archetype/cli` (`ARCHETYPE.md:93`). Then add the generation layer in the shape of `~/Documents/Poems/dev/scout/cast/`: `identifiers.md`, `text.md`, `banner.md`, `files.md`, `source.cast`, and the matching rows of `cast/spell.md` (`"Run cast INIT.md ~/Documents/Poems/dev/saw from archetype/cli. Then add the generation layer (identifiers/text/banner/files/source.cast + spell.md rows) to saw, in the same shape as scout/cast/"`).
- **Build System:** `cast`-generated `CMakeLists.txt`, CMake 3.25+, Ninja, Release LTO (`-O3 -flto=thin`, `-dead_strip` on macOS). Edit `project-info.md`, run `cast`, then configure. Never edit `CMakeLists.txt` by hand.
- **Frameworks:** JUCE `8.0.17` + JAM (`jam_core` for runtime operations; `jam_tui` renders the `--help` frames at build time: `"i still like the HELP page. it's our identity. ~10mb still acceptable."`).
- **Linked modules of saw (`## program module` table of `project-info.md`):** `juce_core`, `juce_cryptography` (for `juce::SHA256`), `jam_core`. No framework other than the C and C++ system libraries is loaded (`JUCE_USE_FOUNDATION=0`). The parts of `jam_core` that use a GUI module compile only when that module is linked (`JUCE_MODULE_AVAILABLE_*`). The two tables below list the modules of the `saw_HelpFrames` target.
- **Server target:** `saw_ToolServer` links the same verb sources and `jam_model_context_protocol` (§5.8).
- **Linked JUCE modules (`## juce module` table of `project-info.md`):** the six rows of `~/Documents/Poems/dev/scout/project-info.md:108-113` (`juce_core`, `juce_data_structures`, `juce_events`, `juce_graphics`, `juce_gui_basics`, `juce_audio_processors`), plus `juce_cryptography` for `juce::SHA256`. scout links `jam_gui`, whose declaration lists `juce_gui_extra` and `juce_animation` (`~/Documents/Poems/dev/jam/user-modules-info.md:90,97`), and scout builds without those two rows. Thus the table does not restate them.
- **Linked JAM modules (closure of `jam_tui`):** `jam_core`, `jam_document`, `jam_markdown`, `jam_freetype`, `jam_graphics`, `jam_data_structures`, `jam_animation`, `jam_web`, `jam_style`, `jam_gui`, `jam_mermaid_diagram`, `jam_markdown_graphics`, `jam_terminal`, `jam_tui`.
- **Platforms:** macOS (ARM64 and x86_64) and Windows (x64, MSVC / clang-cl).
- **Primary Dependencies (OOTB):**
  - `Input` for file reads on macOS and Windows: `juce::MemoryMappedFile` (`readOnly`) for a file above 256 KB, and `juce::FileInputStream` for a smaller one, for a file that cannot be mapped, and for every file that a verb writes (`"i think juce already have ootb for mmap, swap, etc"`, `"why not crossplatfrom using juce? and jam?"`). `std::filesystem::file_size` gives the size and refuses a named pipe, a socket and a device before the open, because `juce::File::existsAsFile` accepts a named pipe. A folder is refused there on macOS and at the open on Windows. The null device is an empty file. A file that becomes shorter during a read gives no fault: on macOS a missing byte of the map reads as NUL, and the file is then binary (§4.5). Windows refuses to shorten a mapped file. A regular file on standard input is mapped from its descriptor on macOS.
  - `jam::File::swapFiles` for the atomic exchange of a target and its blob. Where it is not possible: `juce::TemporaryFile` (`useHiddenFile`) with `jam::File::moveFileTo`, and `overwriteTargetFileWithTemporary()` on Windows (`"i think juce already have ootb for mmap, swap, etc"`). `juce::FileOutputStream::flush()` returns `void`, so SAW reads `getStatus()` after `flush()`.
  - Parallel directory walks via `std::filesystem::directory_iterator` on macOS and Windows, skipping `.git` and ignored directories without descending (`"why not crossplatfrom using juce? and jam?"`). `juce::DirectoryEntry` has no entry type, so it cannot skip a named pipe or a symbolic link.
  - Lock-free `Jobs::runRecursive` (atomics and `std::thread` workers) for the directory walk: each item is one folder or one batch of files of a folder, and it adds its subfolders as items. `Find` fuses the walk and the search.
  - Lock-free `Jobs::run` (`std::atomic<int>` + `std::thread` workers, direct calling-thread execution for `count == 1`) for parallel work on files and on the chunks of one large file.
  - No lock anywhere. Each worker owns its state and its result slot. The undo cache publishes by one move of a complete record (§6.3).
  - `juce::SHA256` for content and transaction hashes.
  - Raw UTF-8 `jam::Strings` parsed by `Arguments` for the command line (avoiding `juce::ArgumentList` whitespace trimming, empty-argument removal, and short-option matching defects).
  - `jam::simd` (`findByte`, `findBytePair`, `findAnyByte`, `findFingerprint`, `countByte`) and `jam::LiteralSet` for SIMD scanning. A literal search looks for its two rarest bytes. A set of literals uses one fingerprint pass.
  - `jam::RegExp` (extended in `jam_core` with offset-based `findNext` returning match spans and captures: `"Literal default + -r regex (extend jam::RegExp with findNext/spans) + recursive dir walk skipping .git/.gitignore"`).
  - `jam::RegExpProgram` and `jam::RegExpAutomaton` for matching in linear time (§4.8).
  - `jam::JsonDocument` and `jam::JsonWriter` for `patch` inputs and undo metadata.
  - `jam::TerminalPage` and `jam::MarkdownDocument` in `saw_HelpFrames`, for the help frames. saw prints the embedded frames.
- **Test runtime:** Neovim 0.10 or later (`vim.system` exists since 0.10). The runner is `nvim --headless -l test/run.lua` (§10).
- **Benchmark runtime:** `scout` (§10.3).

### Architecture Constraints

- ✅ Render every help frame and the erase sequence at build time, in the target `saw_HelpFrames`. The frames are binary data of saw.
- ✅ Run saw in plain `int main (int argc, char* argv[])` with zero GUI initialization.
- ✅ Use one mechanism for macOS and Windows: JAM first, then JUCE, then the C++17 standard library.
- ✅ Erase the screen only on the help path (§4.1).
- ✅ Check every `FileOutputStream` write result and the stream status after `flush()` before an exchange, a move, or `overwriteTargetFileWithTemporary()`.
- ✅ Follow BLESSED (`MANIFESTO.md`), `CODING.md`, and `NAMES.md`.
- ❌ Never call `juce::File::replaceWithData()` (it deletes 0-byte targets and ignores `appendData()` errors).
- ❌ Never spawn external subprocesses (`sed`, `awk`, `grep`, `diff`, `cmp`, `cp`, or any version-control program).
- ❌ Never write files on a default run without `--apply` (`"why not we make it always dry run by default. so destructive edit must be explicit."`).
- ❌ Never store persistent daemon state or repository-local cache files.
- ❌ Never use a lock, a condition variable, or a sleep on a search, walk, output, or transaction path.

---

## 3. Core Principles

1. **Dry-Run by Default:** Every mutating verb previews changes and writes zero bytes unless `--apply` appears on the command line (`"why not we make it always dry run by default. so destructive edit must be explicit. agents could still make mistake passing wrong args, so the default run is always safe and undoable"`).
2. **Explicit Two-Gate Write:** `--apply` always requires `--expect N` on `replace`, `lines`, and `patch` (`"--apply always requires --expect N (dry-run prints N; mismatch writes 0 bytes)"`).
3. **Literal by Default:** Search and replacement treat patterns as exact bytes unless `-r` enables regex (`"Literal default + -r regex (extend jam::RegExp with findNext/spans) + recursive dir walk skipping .git/.gitignore"`).
4. **Atomic All-or-Nothing Transactions:** Every write is one atomic exchange or one atomic replace (§6.3). If file `k + 1` fails, SAW restores files `0..k` immediately.
5. **Verified Reversible Undo:** Every `--apply` keeps the pre-edit file and the post-edit SHA-256 hashes in a bounded rolling cache (`"Rolling cache in app-data dir (cap 32 transactions, verifies post-edit hashes before restoring)"`).
6. **Deterministic Output:** Parallel per-file work emits results sorted by normalized UTF-8 path, then line number, then byte offset.
7. **Exact Preservation:** SAW preserves the line ending of each untouched line, trailing newline presence, symlink target resolution, and the metadata of each target: file mode, access control list, extended attributes. A target with more than one hard link is refused, because a swap breaks the link (`"Preserve each file's LF/CRLF line endings and execute bit; follow symlinks to target (matching cast/scout)"`).

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
saw undo <tx-id> [options]
saw undo --list
saw excerpt [target...] [options]
```

- `saw` with no arguments prints the banner and the frame of `HELP.md` and exits `0`.
- `saw --help` and `saw -h` print the banner and the frame of `HELP.md` and exit `0`.
- `saw <verb> --help` prints the frame of `Source/help/<verb>.md` and exits `0`. Each verb frame is one separate binary data entry (`"One embedded markdown per verb (Source/help/find.md … undo.md) as separate binary entries, plus HELP.md for the overview. Uses the existing API with no section extraction"`).
- When stdout is a terminal, the three help invocations above erase the screen first with the embedded erase sequence (`jam::TerminalEncoder::writeEraseScreen`, rendered at build time). The verbs and `--version` never erase the screen (`"Erase only on the help/banner path (saw, --help, <verb> --help). Never on the six verbs or --version"`).
- `saw --version` and `saw -V` print `saw 0.0.1 (<commit>)` and exit `0` (`"v0.0.1"`).
- `--` terminates option parsing. Every token after `--` is a positional argument.
- A value option takes the next token verbatim, whatever it starts with. `--`, `--help` and `-h` have their meaning only outside an option value.
- One dash with several letters is a group of short flags (`-rn` is `-r -n`). The last flag of a group can take a value.
- A positional that starts with a dash needs `--` before it. A dash directly followed by a digit is a positional (`fields -1`).
- Arguments are raw bytes on macOS. On Windows, SAW converts the UTF-16 command line to UTF-8.

### 4.2 Exit Codes

| Code | Meaning |
|---|---|
| `0` | Success (match found on `find`/`fields`; dry-run preview completed; `--apply` succeeded; `undo` succeeded). |
| `1` | No match found on `find` (matches `grep` convention) or on `fields --match`, an empty `find --files` list, or `--expect N` mismatch on a mutating verb. |
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
- **Explicit file paths:** Resolve relative paths against the current working directory. Resolve symbolic links to their final target with `std::filesystem::canonical`, which follows a link before it applies a `..` that comes after it. Ignore rules (§4.4.1) never skip an explicit file path.
- **Explicit directory paths:** Walk in parallel with one item for each folder (`Paths::applyToDirectories` and `Jobs::runRecursive`, using `std::filesystem::directory_iterator`, one walk for all given directories). `Find` searches each file in the walk, also when several paths are given. The other verbs process the collected files in parallel via `Jobs::run`.
  - Skip symbolic links.
  - Skip `.git` entries always.
  - Skip files and directories that the ignore rules (§4.4.1) exclude, unless `--no-ignore` is set. Never descend into an excluded directory.
  - Skip dot-prefixed entries (hidden files and folders) unless `--hidden` is set.
  - Sort all collected file paths in ascending byte-lexicographic UTF-8 order before emitting output or writing.
- **Piped stdin:** When no `path` argument is given and stdin is not a TTY:
  - `find`, `lines` (read mode), `fields`, and `replace` (dry-run mode) read all bytes from stdin as a virtual file named `<stdin>`.
  - Passing `--apply` with stdin input fails with exit `2`: `saw: cannot --apply on stdin; specify file paths`.
  - Empty stdin fails with exit `2`: `saw: missing path argument`.
- **Pipes:** A path that names a pipe fails as an unreadable file. Use stdin.
- **Unreadable entries:** `find` prints one error for each missing or unreadable file or folder, prints the matches of the readable files, and exits `2`. Every other verb fails on the first one before any output or write.
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
- **Binary detection:** A file is binary when the bytes that the command reads contain a NUL byte (`0x00`). SAW scans them with `jam::simd::findByte (data, size, 0, '\0')`.
  - A command that writes, and `excerpt`, read the whole file.
  - `find`, `lines` and `fields` read the file to the end of the last line that the answer needs. `find -q`, `find -l`, `--first` and a bounded line range stop early, as `grep -q`, `grep -l`, `grep -m 1` and `sed 20q` do. Every other search reads the whole file.
  - During a directory walk: skip binary files silently.
  - When named explicitly on the command line or in a `patch` entry: abort before any write and exit `2` with `saw: binary file: <path>`.
- **Empty (0-byte) files:** `Input` holds no bytes. Treat as a valid empty text buffer with 0 lines and no error.
- **Line endings:** Each untouched line keeps its own ending. A replaced line keeps the ending of the line that it replaces. Every other inserted line takes the dominant ending: the ending of the file's first line (`\r\n` or `\n`), `\n` when the first line has no ending.
- **Trailing newline:** Preserve whether the file ends with a newline unless a line deletion removes the final line.
- **Metadata:** SAW writes the planned bytes to a new file, flushes it one time, sets the metadata of the target on it (`jam::File::setMetadata`: mode, access control list, extended attributes; on Windows the replace call keeps them), then puts it in place (§6.3).
- **Hard links:** A target with more than one hard link fails before any write with exit `2`: `saw: hard-linked file: <path>`. A dry run fails the same way.
- **Read-only files:** A target without write access fails before any write with exit `2`: `saw: read-only file: <path>`.

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
- A pattern that holds a newline can never match. It fails with exit `2`: `saw: pattern cannot contain a newline`.
- A regex matches in time linear in the text, with three exceptions: a backreference, a lookaround that holds more than one character, and a pattern that compiles to more than 2048 instructions. For the exceptions, and for the match spans of a repeated group that can be empty (`(a*)*`), the backtracker runs. It has a step limit for each subject byte and a depth limit. Over a limit, SAW exits `2`: `saw: regex exceeded its step limit: <pattern>`.
- A regex with more than 250 nested groups is invalid.

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
| `-c`, `--count` | Print `<path>:<count>` (or `<count>` for single input) instead of matching lines. A walked file with count 0 prints no row. |
| `-l`, `--files-with-matches` | Print only the path of each input with a match. |
| `-o`, `--only-matching` | Print only each matched text, one for each output line. |
| `-w`, `--word` | Match only where no word byte (`A`–`Z`, `a`–`z`, `0`–`9`, `_`) is directly before or after the match. |
| `-e <pattern>`, `--pattern <pattern>` | Add a pattern. Repeatable. With `-e`, every positional is a path. A line matches when one pattern matches. |
| `--files` | Print the files that the walk reads, sorted. Takes paths only. With `-q`, print nothing. Exit `0` when the list holds a file, `1` when it is empty. |
| `-q`, `--quiet` | Emit nothing to stdout; exit `0` on first match or `1` on no match. |
| `-n`, `--line-number` | Prefix output with 1-based line numbers (default: on). |
| `--no-line-number` | Omit line numbers from output. |
| `--hash` | Prefix each matched line with `<line>:<hhhh>|` instead of `<line>:`. |
| `-C <N>`, `--context <N>` | Show `N` lines of context before and after each match (`N >= 0`). |
| `-A <N>`, `--after-context <N>` | Show `N` lines of context after each match. Overrides `-C` for that side. |
| `-B <N>`, `--before-context <N>` | Show `N` lines of context before each match. Overrides `-C` for that side. |
| `--after <N>` | Inspect only lines with 1-based line number strictly greater than `N` (`line > N`). |
| `--lines <A:B>` | Restrict search to 1-based inclusive line range `A` through `B`. |
| `--first` | Stop after the first match per input file (or first overall when one input). |
| `--include <glob>` | Include only files whose name, path relative to the walk root, or printed path matches `<glob>`. Repeatable. |
| `--exclude <glob>` | Exclude files whose name, path relative to the walk root, or printed path matches `<glob>`. Repeatable. |
| `--hidden` | Include dot-prefixed files and directories in directory walks. |
| `--no-ignore` | Do not apply `.gitignore` filters in directory walks. |

#### Happy Path

1. User runs `saw find "DEBT-" DEBT.md`.
2. SAW memory-maps `DEBT.md`, checks for NUL bytes, and scans for `"DEBT-"` via `jam::simd::findByte` + `std::memcmp`.
3. SAW computes 1-based line numbers lazily for matched byte offsets.
4. SAW writes `<line>:<text>` (single file) or `<path>:<line>:<text>` (multiple files or directory walk) to stdout and exits `0`.

#### Edge Cases

- **No match found:** Write nothing to stdout (or `0` with `-c`) and exit `1`.
- **`-q` with a match and an unreadable entry:** Print no error and exit `0`. Without a match, print the errors and exit `2`.
- **`-c`, `-l`, `-o`, `-q` together:** the order `-q`, `-l`, `-c`, `-o` decides. `-o` prints no context lines. `-v` with `-o` prints nothing.
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
| Newline in `<pattern>` | `saw: pattern cannot contain a newline` | `2` |
| Invalid regex | `saw: invalid regex: <detail>` | `2` |
| Regex over its step limit | `saw: regex exceeded its step limit: <pattern>` | `2` |
| Invalid `-A`, `-B`, `-C` or `--after` integer | `saw: invalid integer for <flag>: <value>` | `2` |
| Named path does not exist | `saw: not found: <path>` | `2` |
| Unreadable file or folder | `saw: cannot read <path>` | `2` |
| Named file is binary | `saw: binary file: <path>` | `2` |

#### Acceptance Criteria

- [ ] `saw find "needle" file.txt` finds literal matches without escaping regex metacharacters.
- [ ] `saw find -x --after 12 --first -- "---" DEBT.md` returns the exact line number of the first `---` after line 12.
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
| `-r`, `--regex` | Interpret `<find>` as `jam::RegExp`. Expand `$0`..`$9` and `${n}` captures and `$$` (literal `$`) in `<with>`. A backslash is literal. |
| `-i`, `--ignore-case` | Match ASCII letters case-insensitively. |
| `--lines <A:B>` | Restrict replacements to 1-based inclusive line range `A` through `B`. |
| `--with-stdin` | Read `<with>` payload verbatim from stdin (requires explicit file paths; `<with>` positional arg omitted). |
| `--with-file <path>` | Read `<with>` payload verbatim from `<path>` (`<with>` positional arg omitted). |
| `--include <glob>` | Include only matching files when a directory path is given. Repeatable. |
| `--exclude <glob>` | Exclude matching files when a directory path is given. Repeatable. |
| `--hidden` | Include dot-prefixed files and directories in directory walks. |
| `--no-ignore` | Do not apply `.gitignore` filters in directory walks. |
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
3. Because `actual == expected`, SAW stages and commits one transaction (§6.3): the planned bytes are written one time, the metadata is kept, the files are exchanged, and the bytes are verified.
4. SAW prints `saw: applied 3 edit(s) in 1 file(s) (tx: <tx-id>)` and exits `0`.

#### Edge Cases

- **`--apply` without `--expect`:** Fail before touching disk with exit `2`: `saw: --apply requires --expect <N>`.
- **`--expect N` mismatch:** Print the dry-run unified diff to stdout, print `saw: expected <N> edit(s), found <actual>; nothing written` to stderr, write zero bytes, and exit `1`.
- **`--apply --expect 0` when 0 matches exist:** Write zero files, create no undo transaction, print `saw: applied 0 edit(s) in 0 file(s)`, and exit `0`.
- **`<find>` equals `<with>` (no-op replacement):** Count matches normally on dry-run; with `--apply`, skip disk write for byte-identical files and print `saw: no content changed; nothing written` (exit `0`).
- **One file named two times (`h1 ./h1`):** SAW edits and counts it one time.
- **Overlapping matches (e.g. `"aa"` in `"aaa"`):** Advance cursor past the end of each matched span (non-overlapping, left-to-right; `"aaa"` yields 1 match).
- **Regex zero-length match (e.g. `^` or `$`):** Apply at current position and advance cursor by 1 byte to prevent infinite loops.
- **Invalid capture reference (`$5` when regex has 2 groups):** Fail before any write with exit `2`: `saw: invalid capture group $5 in replacement`.
- **Target file modified externally after its plan:** `preHash` is the hash of the bytes before the plan. A changed file is not written. Roll back all written files from pre-edit snapshots and exit `2`.

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
| `--stdin` | Mutate | Read replacement or inserted text verbatim from stdin. A text option directly followed by `--stdin`, or after a parsed `--stdin`, takes no `<text>`. |
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
7. **`--apply --expect N`:** Stage and commit one transaction that covers all touched files (§6.3), print `saw: applied <E> edit(s) in <F> file(s) (tx: <tx-id>)`, exit `0`.

#### Edge Cases

- **Empty JSON array `[]`:** Dry-run prints `saw: dry-run — 0 edits; nothing written.` (exit `0`). `--apply --expect 0` writes nothing and exits `0`.
- **Same file referenced via relative path and symlink:** Resolve to the final link target (§4.4) so all edits to the same canonical file group together and check conflicts accurately.
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
saw undo <tx-id> [options]
saw undo --list
```

| Option | Effect |
|---|---|
| `--list` | Print stored transactions (newest first) as `<tx-id>\t<timestamp>\t<files> file(s)\t<command>` and exit `0`. |
| `--apply` | Execute the restore on disk. Without `--apply`, `saw undo` runs as dry-run and prints the reverse unified diff. |
| `--force` | Skip post-edit hash verification on `--apply` (explicit override when user chooses to overwrite newer disk edits). |

#### Happy Path (Dry-Run Default)

1. User runs `saw undo <tx-id>`.
2. SAW loads `<undo-root>/<tx-id>.json` and verifies each target file's current SHA-256 matches `postHash`.
3. SAW prints the reverse unified diff (`current -> original`) and summary: `saw: dry-run undo (<tx-id>) — <F> file(s); nothing written. Pass --apply to restore.`
4. Exit `0`.

#### Happy Path (Apply)

1. User runs `saw undo <tx-id> --apply`.
2. SAW verifies current disk SHA-256 equals `postHash` for every file in the transaction (unless `--force`).
3. SAW exchanges every file with its backup (§6.3). The file keeps its current mode, access control list and extended attributes. A file that `--force` creates again gets the recorded POSIX execute bit. If file `k + 1` fails, files `0..k` revert to their pre-undo bytes.
4. SAW removes the record and the backups of the restored transaction from the undo cache.
5. SAW prints `saw: undone <tx-id> — restored <F> file(s)` and exits `0`.

#### Edge Cases

- **No `<tx-id>`:** Print `saw: undo requires <tx-id>` to stderr and exit `2`. The cache is shared by every folder, so the newest transaction is not a safe default.
- **No transactions stored:** `--list` prints nothing and exits `0`.
- **Corrupt transaction in `--list`:** Print its error, print the other transactions, and exit `2`.
- **Rollback of a failed restore fails:** Print `saw: undo failed for <path>: <reason>; rollback failed for <paths>; backup kept in <folder>` to stderr and exit `2`.
- **Unknown `<tx-id>`:** Print `saw: unknown transaction: <tx-id>` to stderr and exit `2`.
- **File modified after `--apply` (hash drift):** Refuse restoration before writing any file, print `saw: cannot undo <tx-id>: <path> changed since apply (pass --force to overwrite)` to stderr, and exit `2`.
- **Target file deleted after `--apply`:** Under default check, fail with hash drift (`<path> missing since apply`). Under `--force`, recreate the file with original bytes via `juce::TemporaryFile`.

#### Error Handling

| Condition | Stderr Message | Exit |
|---|---|---|
| Missing `<tx-id>` | `saw: undo requires <tx-id>` | `2` |
| Unknown `<tx-id>` | `saw: unknown transaction: <tx-id>` | `2` |
| Pending `<tx-id>` without `--force` | `saw: transaction <tx-id> is pending (pass --force to restore it)` | `2` |
| Corrupt record or missing blob | `saw: corrupt transaction <tx-id>: <detail>` | `2` |
| File drifted since `--apply` | `saw: cannot undo <tx-id>: <path> changed since apply (pass --force to overwrite)` | `2` |
| Restore write failure | `saw: undo failed for <path>: <reason>; rolled back` | `2` |

#### Acceptance Criteria

- [ ] `saw undo` defaults to dry-run and writes zero bytes.
- [ ] `saw undo <tx-id> --apply` restores bit-identical original files after any `replace`, `lines`, or `patch` apply, and keeps the mode of each file.
- [ ] Editing a file externally after `saw replace --apply` causes `saw undo --apply` to refuse with exit `2` and leave the newer edit intact.
- [ ] Rolling cache never exceeds 32 published transactions.

### 5.7 `saw excerpt`

Decision: `"but we have a dedicated lane: formatted to format you EXACTLY need. so agents never wrongly formatted, return verbatim as is"`. The other verbs do not change: `"the current sed, awk, grep, rg parity STAYS"`.

```
saw excerpt [target...] [--context <N>]
```

Targets come from the arguments, or one per line from stdin when no argument is given. A target is one of:

| Target | Gives |
|---|---|
| `<path>` | The whole file. |
| `<path>:<A>` or `<path>:<A>-<B>` | Line `A`, or lines `A` through `B`. `B` beyond the last line clamps to it. |
| `<path> ~ <literal>` | Each line that holds the literal, with `--context` lines on each side (default `0`). Overlapping ranges merge. |

The range suffix is the last `:` followed only by digits, or by digits, `-` and digits. Thus a Windows drive letter is part of the path.

Output on stdout, and nothing else:

```
# <n> targets, <f> found, <m> missing
## <i> <path>:<A>-<B> of <lines> sha256:<16 hex>
<line number><TAB><line bytes>
## <i> <path> NOT FOUND
```

- The receipt line comes first. `<n>` is the count of targets given.
- Blocks keep the target order. `<i>` counts targets from 1. A literal target prints one block for each merged range, with the same `<i>`.
- The hash is the first 16 hex digits of the SHA-256 of the bytes of the block lines, line endings included.
- In place of a range: `NOT FOUND`, `UNREADABLE`, `BINARY`, `EMPTY` (a 0-byte file), `NO MATCH` (a literal target), `OUT OF RANGE` (`A` beyond the last line).
- A line that is not valid UTF-8 prints each invalid byte as `\xNN`, and the header of its block ends with ` escaped`. Every other line prints its bytes without its line ending, then `\n`.
- Exit `0` when every target gives a block of lines, `1` when one target does not, `2` on a usage error. No target and empty stdin: `saw: excerpt requires <target>`, exit `2`.

### 5.8 Server Lane (`saw_ToolServer`)

Decisions: `"FASTER on ALL CASES. NON-NEGOTIABLE, NO EXCEPTIONS."`, `"cli isn't enough? need lazy load mcp that never inject anything when idle? never eat context when not being used? always on demand? fine, add one."`

`saw_ToolServer` is a Model Context Protocol server on stdio, built on `jam::ModelContextServer`. It gives one tool, `run`:

| Argument | Meaning |
|---|---|
| `arguments` | Array of strings: the saw command line, verb first. |
| `directory` | Absolute path of the working directory for relative paths. |
| `stdin` | Optional text for standard input. |

The result is one JSON object: `exitCode`, `output` and `errors`, the exit code, the standard output and the standard error of the same command line in the CLI, byte for byte. Text that is not valid UTF-8 cannot be a JSON string, so its member is `outputBase64` or `errorsBase64` in place of `output` or `errors`. The server runs one call at a time and holds no state between calls. `--apply` needs `--expect`, as in the CLI.

---

## 6. Transaction and Undo Cache Model

Decision: `"Rolling cache in app-data dir (cap 32 transactions, verifies post-edit hashes before restoring)"`.

### 6.1 Cache Location and Bound

- **Root path:** `jam::File::getUserApplicationDataDirectory().getChildFile ("saw").getChildFile ("undo")`.
  - macOS: `~/Library/Application Support/saw/undo/`
  - Windows: `%APPDATA%\saw\undo\`
- **Pending path:** the folder `pending` beside `undo`. It holds the record of each transaction that is not yet published and of each transaction that an `undo` run has claimed. `undo --list` lists each complete one, and `undo <tx-id>` reads it, so the backup of a process that stopped is always reachable.
- **Blob path:** the folder `blobs` beside `undo`. It holds the backups.
- **Bound:** Maximum `32` published transactions. After each committed `--apply`, SAW deletes the oldest published transactions beyond 32 (ordered by UTC timestamp prefix). Prune never reads the pending folder.

### 6.2 Transaction Layout

Each transaction has one id, `<YYYYMMDDTHHMMSS>-<6-hex-id>`, one immutable record, and one backup for each file. The name of a file gives the id and the index, so the record does not repeat them.

```
undo/20261122T103000-a3f9c1.json
blobs/20261122T103000-a3f9c1-0.blob
blobs/20261122T103000-a3f9c1-1.blob
```

Record fields:
- `"timestamp"`: ISO-8601 UTC string.
- `"command"`: short verb summary (e.g. `"replace"`, `"lines"`, `"patch"`).
- `"files"`: array of `{ "path": "<canonical-absolute-path>", "preHash": "<sha256>", "postHash": "<sha256>", "executable": true|false }`. The backup of entry `i` is `blobs/<tx-id>-<i>.blob`.

### 6.3 Two-Phase Commit Protocol

No lock exists. Ownership is one exclusive creation. The creation of a name that exists fails, so one process wins.

1. **Plan phase (memory only):** Read every target file, verify preconditions (`--expect N`, hashes, bounds, non-binary), and compute planned buffers. Any error aborts with zero disk touches.
2. **Stage phase:** Create `pending/<tx-id>.json` exclusively. The hex id is the microsecond of the second. When `pending/<tx-id>.json` or `undo/<tx-id>.json` exists, the id advances by 1 and the creation runs again. Then write the record, with one `preHash` and one `postHash` for each file, and every `blobs/<tx-id>-<i>.blob`, in parallel. Flush each file. When the target and the cache are on one volume of a system with an atomic file exchange (macOS), the blob holds the planned bytes and the metadata of the target. When the group of the target folder differs from the group of the cache, SAW creates the blob in the target folder and moves it. In every other case the blob holds the pre-edit bytes. A pending record that is not complete, or whose blobs do not all exist, is a transaction that changed no file. `undo --list` does not list it, so a reader never sees a partial transaction.
3. **Commit phase:** Release every mapping of a target. For each file `0..M-1`:
   - **Exchange:** exchange the target and its blob in one system call. Thus the original file becomes the backup, and no byte of it is copied. Verify that the bytes of the blob equal the pre-edit bytes: a write by another process before the exchange is found here, and SAW exchanges the files back. Verify that the bytes of the target equal the planned bytes.
   - **Replace** (no exchange on this volume or system, or the exchange fails): the blob holds the pre-edit bytes. Verify that the bytes of the target equal the pre-edit bytes. Write the planned bytes to a new `juce::TemporaryFile (target, TemporaryFile::useHiddenFile)`, set the metadata, flush one time, and replace the target by one `rename` (Windows: `overwriteTargetFileWithTemporary()`). Verify that the bytes of the target equal the planned bytes.
   - After the last file, publish: move `pending/<tx-id>.json` to `undo/<tx-id>.json`.
   - On any failure at index `k`: restore files `0..k-1` by the same exchange or replace, and verify each target and each blob. A target that another process changed after the exchange keeps that change, and the restore counts as failed, so its backup is kept. A blob that does not hold the pre-edit bytes is never discarded. When every restore holds, delete the record and the blobs and exit `2`. When one restore fails, keep them and exit `2` with `saw: write failed for <path>: <reason>; rollback failed for <paths>; backup kept in <folder>`.
4. **Prune phase:** Delete the oldest published transactions beyond 32, each record before its blobs. The delete of the record is the claim: when the record is gone (an `undo` run moved it), SAW keeps the blobs.

`undo --apply` creates `pending/<tx-id>.json` exclusively and moves `undo/<tx-id>.json` over it first. The process that wins the creation restores the files and deletes the record and the blobs. A failed restore moves the record back. A transaction that is already in the pending folder can belong to a running process, so `undo --apply` refuses it with `saw: transaction <tx-id> is pending (pass --force to restore it)` and exit `2`. `--force` restores it in place.

A process that stops between stage and commit leaves blobs in one of two states. The hash of the blob tells which one: `preHash` is a backup, so `undo` restores it; `postHash` is a file that was not exchanged, so its target was not changed and `undo` does nothing to it. A blob with another hash, whose target has `preHash`, is also restored already. Any other hash is a corrupt transaction.

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
| Extra argument | `saw: unexpected argument: <argument>` | `2` |
| `replace` without its arguments | `saw: replace requires <find> and <with>` | `2` |
| Two replacement sources | `saw: specify at most one of --with-stdin, --with-file` | `2` |
| `lines` without a range | `saw: lines requires <range>` | `2` |
| `fields` without a spec | `saw: fields requires <spec>` | `2` |
| Unreadable stdin | `saw: cannot read stdin` | `2` |
| Unwritable stdout | `saw: cannot write stdout` | `2` |
| `excerpt` without a target | `saw: excerpt requires <target>` | `2` |
| Missing file or directory | `saw: not found: <path>` | `2` |
| Unreadable file or directory | `saw: cannot read <path>` | `2` |
| Binary file named directly | `saw: binary file: <path>` | `2` |
| Hard-linked target | `saw: hard-linked file: <path>` | `2` |
| Read-only target | `saw: read-only file: <path>` | `2` |
| Empty search pattern | `saw: pattern cannot be empty` | `2` |
| Newline in search pattern | `saw: pattern cannot contain a newline` | `2` |
| Invalid regex | `saw: invalid regex: <detail>` | `2` |
| Regex over its step limit | `saw: regex exceeded its step limit: <pattern>` | `2` |
| `--apply` without `--expect` | `saw: --apply requires --expect <N>` | `2` |
| `--apply` with stdin | `saw: cannot --apply on stdin; specify file paths` | `2` |
| `--expect` mismatch | `saw: expected <N> edit(s), found <actual>; nothing written` | `1` |
| Hash precondition mismatch | `saw: hash mismatch at <path>:<line>: expected <expected>, got <actual>` | `2` |
| `undo` of a pending transaction without `--force` | `saw: transaction <tx-id> is pending (pass --force to restore it)` | `2` |
| Write or verify failure | `saw: write failed for <path>: <reason>; rolled back` | `2` |
| Rollback failure | `saw: write failed for <path>: <reason>; rollback failed for <paths>; backup kept in <folder>` | `2` |

---

## 9. Success Criteria

### Functional

- [ ] `saw`, `saw --help`, and `saw <verb> --help` print the frames that `jam::TerminalPage` rendered at build time and exit `0`.
- [ ] `saw --version` prints `saw 0.0.1 (<commit>)` and exits `0`.
- [ ] All 7 verbs (`find`, `replace`, `lines`, `fields`, `patch`, `undo`, `excerpt`) pass every case of `test/cases.md` through the runner (§10) on macOS and Windows, from the command line and from the server.
- [ ] Every mutating verb defaults to dry-run and touches zero bytes without `--apply`.
- [ ] `--apply` without `--expect N` fails with exit `2` and writes zero bytes on `replace`, `lines`, and `patch`.
- [ ] Every `--expect N` mismatch writes zero bytes and exits `1`.
- [ ] Multi-file failure during commit rolls back all prior files to bit-identical pre-edit bytes.
- [ ] `saw undo --apply` reverses any applied transaction cleanly and refuses drifted files unless `--force` is passed.
- [ ] `saw` preserves the line ending of each untouched line, trailing newline state, 0-byte files, symlink targets, file modes, access control lists, and extended attributes. It refuses hard-linked and read-only targets.
- [ ] Directory walks apply the ignore rules of §4.4.1.

### Performance and Quality

- [ ] saw runs without `juce::ScopedJuceInitialiser_GUI`. The help frames are rendered at build time.
- [ ] Every row of the §10.3 gate holds, with the same output as its reference.
- [ ] The saw binary loads no macOS or Windows GUI framework (`otool -L`, `dumpbin /dependents`).
- [ ] No lock exists in `Source/` and in the JAM code that saw links.
- [ ] Literal `find` and `replace` scan the file bytes through views with zero whole-file string copies on read.
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
- **Invocation:** `nvim --headless -l test/run.lua <saw-binary>`. `nvim --headless -l test/run.lua <saw-binary> <server-binary>` runs each saw step through `saw_ToolServer`, and expects the same bytes. A step whose arguments or stdin are not UTF-8 runs on the CLI. Neovim 0.10 or later. `-l` skips the user configuration and disables plugins.
- **Output:** one line per case: `PASS <name>` or `FAIL <name>: <first difference>`.
- **Exit code:** `0` when every case passes, `1` when one case fails. The runner calls `os.exit`.
- **Process calls:** `vim.system` with `text = false`, `stdin` set to a string, and a `cwd`. The runner always passes a stdin string, empty when the `stdin` cell is empty. Thus saw never sees a terminal on stdin in a test run. The terminal branches of §4.4 have no runner case.
- **Byte comparison:** Lua string `==`. Lua strings are 8-bit clean.
- **Undo isolation:** before the first case, the runner renames `<app-data>/saw/undo` to `<app-data>/saw/undo-runner-<pid>` when it exists, and `pending` and `blobs` in the same way. After the last case, also after a Lua error, the runner deletes the `undo`, `pending` and `blobs` folders that the run made and renames the saved folders back. `<app-data>` is `$HOME/Library/Application Support` on macOS and `%APPDATA%` on Windows.

### 10.2 Case Manifest

`test/cases.md` holds one grid table under `## cases`. Each row is one line of text between two border lines. The runner reads each cell by the column offsets of the `+---+` border line.

| Column | Meaning |
|---|---|
| `name` | Case name. Rows with the same name are steps of one case. They run in row order, in one temporary folder. |
| `fixture` | Folder under `test/fixture/`. The runner copies it to a new temporary folder before the first step of a case. |
| `command` | The saw arguments. Whitespace separates arguments. A span in double quotes stays one argument, and the runner removes the quote marks. `{n}` is the process index, `{tx}` the newest transaction id, `{newline}` a line feed, `{xff}` the byte `0xff`. |
| `stdin` | File under `test/stdin/` whose bytes go to stdin, or empty. |
| `exit` | The expected exit code. |
| `reference` | Empty, or a reference command (program and arguments, same split rule). |
| `compare` | Empty, `stdout`, `lines` (sorted lines of stdout), or `file:<relative path>`. |
| `setup` | Actions before the step, in both folders: `mode:<octal>:<path>`, `link:<source>:<path>`, `mkdir:<path>`, `xattr:<name>:<value>:<path>`, `copy:<source>:<path>`. |
| `check` | Checks after the step: `mode:<octal>:<path>`, `links:<n>:<path>`, `xattr:<name>:<value>:<path>`, `bytes:<path>:<path>`, `transactions:<n>`. |
| `parallel` | Process count, `1` when empty. The runner compares the exit codes and the checks, not stdout. |
| `platform` | Empty, `posix`, or `mac`. Another platform skips the case and prints `SKIP <name>`. |

**Baseline mode (`reference` empty).**

1. The runner compares the stdout of step `<k>` (1-based) with `test/baseline/<name>/<k>/stdout`, and its stderr with `test/baseline/<name>/<k>/stderr`.
2. The runner compares the exit code with the `exit` cell. A step that a signal ends fails.
3. After the last step, the runner compares the temporary folder with `test/baseline/<name>/files/`, file by file, byte by byte. When `files/` does not exist, the expected folder is the fixture without change.
4. Before the comparison, the runner replaces each transaction id (`<8 digits>T<6 digits>-<6 hex digits>`) with `<tx-id>` and the temporary folder path with `<root>`.

**Reference mode (`reference` set).**

1. The runner copies the fixture again to a second temporary folder and runs the reference command there.
2. The reference process gets `clear_env = true` and an `env` with only `LC_ALL=C`, `PATH`, and `HOME` from the runner.
3. `compare = stdout`: saw stdout must equal reference stdout.
4. `compare = file:<path>`: the bytes of `<path>` in the saw folder after the run must equal reference stdout.
5. The saw exit code must equal the `exit` cell. The reference exit code must equal the `exit` cell when `compare` is `stdout` or `lines`.
6. Reference commands use only syntax that BSD and GNU tools share. They never use `sed -i`, BRE `\+` or `\?`, or `grep -P`.

### 10.3 Benchmark

- **Tool:** `scout`. One manifest per pair in `benchmark/`, with `## run` keys `epochs` (11), `command`, `samples`, `output`, and `baseline`.
- **Command script:** a script per manifest. The script runs the workload once, then writes an empty samples file: `: > "$(dirname "$1")/<name>-samples.md"` (pattern of `scout/benchmark/mcp.sh:8`).
- **Corpus:** a copy of `~/Documents/Poems/JUCE/modules`, and files of 1 KB, 10 KB, 50 KB, 100 KB, 1 MB, 10 MB and 100 MB cut from it. A mutating workload copies the corpus to a new temporary folder at the start of each epoch.
- **Gate:** correctness first. The output of each row equals the output of its reference, with 0 differences, before its time counts. Then, in the same run:
  - the CLI is faster than each reference in every row, at every size (`"FASTER on ALL CASES. NON-NEGOTIABLE, NO EXCEPTIONS."`),
  - the server lane (§5.8) is faster than each reference in every row, at every size.
- **Baseline:** each saw manifest names the report of its reference tool as `baseline`. The saw report then holds `change` and `probability` columns.
- **Pairs:**

| Workload | saw | Reference |
|---|---|---|
| Literal search, tree | `saw find <literal> <corpus>` | `grep -rn <literal> <corpus>`, `rg -n <literal> <corpus>` |
| Regex search, tree | `saw find -r <regex> <corpus>` | `grep -rnE <regex> <corpus>`, `rg -n <regex> <corpus>` |
| Regex without a literal, tree | `saw find -r '[0-9]{3,}' <corpus>` | `rg -n '[0-9]{3,}' <corpus>` |
| Alternation, tree | `saw find -r 'TODO\|FIXME' <corpus>` | `rg -n 'TODO\|FIXME' <corpus>` |
| Ignore case, tree | `saw find -i juce_api <corpus>` | `rg -n -i juce_api <corpus>` |
| Context, tree | `saw find -C 2 JUCE_API <corpus>` | `rg -n -C 2 JUCE_API <corpus>` |
| Literal search, one small file | `saw find <literal> <file>` | `grep -n`, `rg -n` |
| Line range, one small file | `saw lines <A:B> <file>` | `sed -n '<A>,<B>p' <file>` |
| Literal replace, one small file | `saw replace <a> <b> <file> --apply --expect <N>` | `sed` to a temporary file, then `mv` |
| Batch, one small file | one `saw patch` of N edits | N `sed` calls |
| Literal replace, tree | `saw replace <a> <b> <corpus> --apply --expect <N>` | `sed` on the files that `rg -l` lists |
| Line range, one file | `saw lines <A:B> <file>` | `sed -n '<A>,<B>p' <file>`, `awk 'NR>=<A> && NR<=<B>' <file>` |
| Fields, one file | `saw fields 1,-1 -d <d> <file>` | `awk -F<d> '{print $1 "<d>" $NF}' <file>` |
