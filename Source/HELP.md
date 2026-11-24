# SAW: Safe Atomic Writer

**Version 0.0.1**

SAW replaces `sed`, `grep`, and file-oriented `awk` with one deterministic CLI tool. SAW previews matches or diffs by default. Pass `--apply --expect N` to write edits. Run `saw undo` to reverse any applied transaction.

`SPEC.md` governs every implementation detail. SAW operates on UTF-8 and raw-byte text files.

## Core Principles

1. **Dry-run by default:** Mutating verbs preview changes. They write zero bytes unless you pass `--apply`.
2. **Explicit two-gate write:** `--apply` requires `--expect N` on `replace`, `lines`, and `patch`.
3. **Literal by default:** Search and replacement match exact bytes. Pass `-r` to enable regex.
4. **Atomic transactions:** Every write exchanges a complete new file with the target in one step. When file `k + 1` fails, SAW restores files `0..k` at once.
5. **Verified undo:** Every `--apply` keeps the pre-edit file and the SHA-256 hashes before and after the edit. The rolling cache holds 32 transactions.
6. **Deterministic output:** SAW sorts results by normalized UTF-8 path, line number, and byte offset.
7. **Exact preservation:** SAW preserves each file's `LF` or `CRLF` line endings and trailing newline. SAW preserves symlink targets and the metadata of each file.

## Command Line

| Command | Result | Exit |
| :--- | :--- | :--- |
| `saw` | Prints the banner and this guide. | `0` |
| `saw --help`, `saw -h` | Prints the banner and this guide. | `0` |
| `saw <verb> --help` | Prints the help guide for `<verb>`. | `0` |
| `saw --version`, `saw -V` | Prints `saw 0.0.1 (<commit>)`. | `0` |
| `saw <verb> ...` | Runs one of the seven verbs below. | `0`, `1`, or `2` |

When stdout is a terminal, `saw`, `saw --help`, and `saw <verb> --help` erase the screen first. The seven verbs and `--version` never erase the screen.

`--` ends option parsing. SAW treats every token after `--` as a positional argument.

- A value option takes the next token verbatim, even when that token starts with `-`.
- `--`, `--help`, and `-h` keep their meaning only outside a value.
- One dash with several letters is a group of short flags. `-rn` means `-r -n`. The last flag of a group can take a value.
- A positional that starts with `-` needs `--` before it. A `-` directly followed by a digit is a positional (`fields -1`).

### Verbs

| Verb | Invocation | Purpose |
| :--- | :--- | :--- |
| `find` | `saw find <pattern> [path...] [options]` | Search files or stdin for literal strings or regex patterns. |
| `replace` | `saw replace <find> <with> [path...] [options]` | Replace literal strings or regex matches across files. |
| `lines` | `saw lines <range> [path...] [options]` | Read, delete, replace, or insert 1-based line ranges. |
| `fields` | `saw fields <spec> [path...] [options]` | Extract and format delimited columns from lines. |
| `patch` | `saw patch [options]` | Apply a JSON batch of edits in one atomic transaction. |
| `undo` | `saw undo <tx-id> [options]` | Inspect and reverse applied SAW transactions. |
| `excerpt` | `saw excerpt [target...] [options]` | Return exact line blocks from files, with a receipt and a hash for each block. |

## Exit Codes and Diagnostics

| Code | Meaning |
| :--- | :--- |
| `0` | Success: `find` or `fields` matched, dry-run completed, `--apply` succeeded, or `undo` succeeded. |
| `1` | No match on `find` or `fields --match`, an empty `find --files` list, or `--expect N` mismatch on a mutating verb. |
| `2` | Usage error, bad flag, bad regex, bad JSON, missing or binary file, hard-linked or read-only target, conflict, drift, or I/O error. |

SAW writes all errors to `stderr` with the prefix `saw: <message>`. Each error writes one concise line and exits with `1` or `2`.

SAW writes command data, dry-run diffs, and summary lines to `stdout`. Summary lines follow these forms: `saw: dry-run …`, `saw: applied …`, `saw: undone …`, `saw: dry-run undo …`, and `saw: no content changed; nothing written`.

## Input Sources and Stdin

- **Explicit file paths:** Relative paths resolve against the current working directory. Symbolic links resolve to their final target. Ignore rules never skip an explicit file path.
- **Explicit directory paths:** SAW walks directories in parallel, one job for each folder. Walks skip symbolic links.
- **Walk filters:** Directory walks skip `.git` always. Walks skip ignored paths unless you pass `--no-ignore`. Walks skip dot-prefixed entries unless you pass `--hidden`.
- **Sort order:** SAW sorts collected paths in ascending byte-lexicographic UTF-8 order.
- **Piped stdin:** When you pass no path and stdin is not a TTY, verbs read stdin as `<stdin>`. This applies to `find`, `lines` (read mode), `fields`, and `replace` (dry-run). Passing `--apply` with stdin exits `2`. Empty stdin exits `2` with `saw: missing path argument`.
- **No path on a TTY:** `find` walks `.`. `replace`, `lines`, and `fields` exit `2`. `patch` requires `--file <path>`.
- **Unreadable entries:** `find` reports each missing or unreadable path, searches the rest, and exits `2`. Other verbs fail on the first one, before any output or write.

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
- **Line endings:** Each untouched line keeps its own ending. A replaced line keeps the ending of the line it replaces. Other inserted lines take the ending of the file's first line (`\r\n` or `\n`). A file whose first line has no ending uses `\n`. SAW preserves trailing newline presence unless a deletion removes the final line.
- **Metadata and permissions:** SAW keeps each file's mode, access control list, and extended attributes through the swap.
- **Hard links:** SAW refuses a file with more than one hard link before any write. A dry run is refused too. Exit `2`.
- **Read-only files:** SAW refuses a file without write access before any write. Exit `2`.
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

SAW stores up to 32 undo transactions under `<app-data>/saw/`. On macOS this is `~/Library/Application Support/saw/`. On Windows this is `%APPDATA%\saw\`.

Each transaction has an id of the form `<YYYYMMDDTHHMMSS>-<6-hex-id>`. The record is `undo/<tx-id>.json`. The backup of file `i` is `blobs/<tx-id>-<i>.blob`. A transaction that is not complete has its record in `pending/`.

Every `--apply` runs a two-phase commit protocol:

1. **Plan:** Read the target files. Verify `--expect N`, hashes, bounds, and non-binary content. Any error aborts with zero disk writes.
2. **Stage:** Create the record in `pending/` and write it and every blob, in parallel.
3. **Commit:** Exchange each target with its blob in one step. Verify the bytes of both files. Where the system has no exchange, replace the target by one rename. If file `k` fails, SAW restores and verifies files `0..k-1`, deletes the transaction, and exits `2`. If a restore fails, SAW keeps the transaction. Then move the record to `undo/`.
4. **Prune:** Delete the oldest transactions in `undo/` beyond 32.

No lock exists. One exclusive creation gives each transaction its owner, so parallel commands never wait for each other.

`saw undo` requires `<tx-id>` unless you pass `--list`. The cache is shared by every folder, so SAW has no default transaction.

## Global Error Reference

| Condition | Exact Stderr String | Exit |
| :--- | :--- | :--- |
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
| Write or verify failure | `saw: write failed for <path>: <reason>; rolled back` | `2` |
| Rollback failure | `saw: write failed for <path>: <reason>; rollback failed for <paths>; backup kept in <folder>` | `2` |
