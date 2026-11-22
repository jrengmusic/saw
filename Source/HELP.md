# SAW: Safe Atomic Writer

**Version 0.0.1**

SAW replaces `sed`, `grep`, and file-oriented `awk` with one deterministic CLI tool. SAW previews matches or diffs by default. Pass `--apply --expect N` to write edits. Run `saw undo` to reverse any applied transaction.

`SPEC.md` governs every implementation detail. SAW operates on UTF-8 and raw-byte text files.

## Core Principles

1. **Dry-run by default:** Mutating verbs preview changes. They write zero bytes unless you pass `--apply`.
2. **Explicit two-gate write:** `--apply` requires `--expect N` on `replace`, `lines`, and `patch`.
3. **Literal by default:** Search and replacement match exact bytes. Pass `-r` to enable regex.
4. **Atomic transactions:** Every write swaps through `juce::TemporaryFile`. When file `k + 1` fails, SAW restores files `0..k` at once.
5. **Verified undo:** Every `--apply` saves pre-edit bytes and post-edit SHA-256 hashes. The rolling cache holds 32 transactions.
6. **Deterministic output:** SAW sorts results by normalized UTF-8 path, line number, and byte offset.
7. **Exact preservation:** SAW preserves each file's `LF` or `CRLF` line endings and trailing newline. SAW preserves symlink targets and POSIX execute bits.

## Command Line

| Command | Result | Exit |
| :--- | :--- | :--- |
| `saw` | Prints the banner and this guide. | `0` |
| `saw --help`, `saw -h` | Prints the banner and this guide. | `0` |
| `saw <verb> --help` | Prints the help guide for `<verb>`. | `0` |
| `saw --version`, `saw -V` | Prints `saw 0.0.1 (<commit>)`. | `0` |
| `saw <verb> ...` | Runs one of the six verbs below. | `0`, `1`, or `2` |

When stdout is a terminal, `saw`, `saw --help`, and `saw <verb> --help` erase the screen first. The six verbs and `--version` never erase the screen.

`--` ends option parsing. SAW treats every token after `--` as a positional argument.

### Verbs

| Verb | Invocation | Purpose |
| :--- | :--- | :--- |
| `find` | `saw find <pattern> [path...] [options]` | Search files or stdin for literal strings or regex patterns. |
| `replace` | `saw replace <find> <with> [path...] [options]` | Replace literal strings or regex matches across files. |
| `lines` | `saw lines <range> [path...] [options]` | Read, delete, replace, or insert 1-based line ranges. |
| `fields` | `saw fields <spec> [path...] [options]` | Extract and format delimited columns from lines. |
| `patch` | `saw patch [options]` | Apply a JSON batch of edits in one atomic transaction. |
| `undo` | `saw undo [tx-id] [options]` | Inspect and reverse applied SAW transactions. |

## Exit Codes and Diagnostics

| Code | Meaning |
| :--- | :--- |
| `0` | Success: `find` or `fields` matched, dry-run completed, `--apply` succeeded, or `undo` succeeded. |
| `1` | No match on `find` or `fields --match`, or `--expect N` mismatch on a mutating verb. |
| `2` | Usage error, bad flag, bad regex, bad JSON, missing or binary file, conflict, drift, or I/O error. |

SAW writes all errors to `stderr` with the prefix `saw: <message>`. Each error writes one concise line and exits with `1` or `2`.

SAW writes command data, dry-run diffs, and summary lines to `stdout`. Summary lines follow these forms: `saw: dry-run …`, `saw: applied …`, `saw: undone …`, `saw: dry-run undo …`, and `saw: no content changed; nothing written`.

## Input Sources and Stdin

- **Explicit file paths:** Relative paths resolve against the current working directory. Symbolic links resolve to their target via `juce::File::getLinkedTarget()`. Ignore rules never skip an explicit file path.
- **Explicit directory paths:** SAW walks directories one level at a time. Worker threads process files in parallel.
- **Walk filters:** Directory walks skip `.git` always. Walks skip ignored paths unless you pass `--no-ignore`. Walks skip dot-prefixed entries unless you pass `--hidden`.
- **Sort order:** SAW sorts collected paths in ascending byte-lexicographic UTF-8 order.
- **Piped stdin:** When you pass no path and stdin is not a TTY, verbs read stdin as `<stdin>`. This applies to `find`, `lines` (read mode), `fields`, and `replace` (dry-run). Passing `--apply` with stdin exits `2`.
- **No path on a TTY:** `find` walks `.`. `replace`, `lines`, and `fields` exit `2`. `patch` requires `--file <path>`.

### Ignore Rules

The top level is the nearest ancestor of the walk root (or the root itself) holding `.git`. When no ancestor holds `.git`, the walk root is the top level.

SAW reads ignore sources in this order, from highest to lowest precedence:

1. Every `.gitignore` file from the top level down to the path's directory.
2. `<top level>/.git/info/exclude`, when that file exists.
3. The file that `core.excludesFile` names.

Within one level, the last matching pattern wins. SAW reads `core.excludesFile` from `$XDG_CONFIG_HOME/git/config` (or `$HOME/.config/git/config`), then `~/.gitconfig`. The last `excludesFile` value in `[core]` wins. SAW expands a leading `~/` to `$HOME`. Default is `$XDG_CONFIG_HOME/git/ignore`, or `$HOME/.config/git/ignore`.

Patterns support `#` comments, backslash escapes, and `!` negation. Patterns support trailing `/` for directories, `*`, `?`, `[...]`, `**/`, `/**`, and `/**/`. Matching is case-sensitive with `/` separators.

## Files, Line Endings, and Line Hashes

- **Binary detection:** Any file containing a NUL byte (`0x00`) is binary. Directory walks skip binary files silently. Naming a binary file directly or in `patch` exits `2`.
- **Empty files:** SAW treats 0-byte files as valid text with 0 lines and no error.
- **Line endings:** SAW detects `\r\n` or `\n` from the first newline (default `\n`). Insertions and line replacements normalize `\n` in your text to match. SAW preserves trailing newline presence unless a deletion removes the final line.
- **Permissions:** On macOS, SAW reads POSIX execute mode bits. SAW restores them on the temporary file before swapping.
- **Line hash (`--hash`):** The hash is a 4-character lowercase hexadecimal prefix of the line's SHA-256 digest. Computation excludes trailing `\r` or `\n`. Format: `<L>:<hhhh>|<line-text>` (example: `42:a7c1|    return result;`).

## Unified Diff Format

Every mutating dry-run writes GNU-exact unified diff hunks to `stdout`:

```
--- <path>
+++ <path> (planned)
@@ -<oldStart>[,<oldCount>] +<newStart>[,<newCount>] @@
 <context>
-<removed>
+<added>
```

- Context is 3 unchanged lines around each change. SAW merges hunks whose context ranges overlap or touch.
- One-line ranges omit `,1` (`@@ -1 +1 @@`). Empty ranges write `<line>,0` (`@@ -3,0 +4 @@`, `@@ -0,0 +1 @@`).
- A final line without a newline emits `\ No newline at end of file`.
- After all hunks, SAW prints `saw: dry-run — <E> edit(s) in <F> file(s); nothing written. Pass --apply --expect <E> to write.`. When zero edits match, SAW prints `saw: dry-run — 0 edits; nothing written.`.

## Pattern Matching

`find`, `replace`, and `fields --match` match each line without its `\n` or `\r\n` ending. Matches never cross a line ending.

- **Literal mode (default):** SAW compares exact bytes. `-i` compares ASCII letters `A`–`Z` and `a`–`z` without case.
- **Regex mode (`-r`):** SAW compiles a `jam::RegExp` pattern. `^` matches line start and `$` matches line end. `-i` compiles `(?i:<pattern>)`.

## Transaction and Undo Cache

SAW stores up to 32 undo transactions under `<app-data>/saw/undo/`. On macOS this is `~/Library/Application Support/saw/undo/`. On Windows this is `%APPDATA%\saw\undo\`.

Each directory uses the name `<YYYYMMDDTHHMMSS>-<6-hex-id>`. It holds `meta.json` and pre-edit `<i>.blob` files.

Every `--apply` runs a two-phase commit protocol:

1. **Plan:** Map target files in memory. Verify `--expect N`, hashes, bounds, and non-binary content. Any error aborts with zero disk writes.
2. **Stage:** Write pre-edit `<i>.blob` files and `meta.json` in the transaction directory.
3. **Commit:** Swap each file through `juce::TemporaryFile`. Restore execute bits on macOS and verify post-write SHA-256 hashes. If file `k` fails, SAW restores files `0..k-1`, deletes the staged directory, and exits `2`.
4. **Prune:** Delete the oldest transaction directories beyond 32.

## Global Error Reference

| Condition | Exact Stderr String | Exit |
| :--- | :--- | :--- |
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
