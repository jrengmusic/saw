# saw replace

Replace literal strings or regex matches across files. SAW previews diffs by default and writes zero bytes without `--apply`.

## Invocation

```
saw replace <find> <with> [path...] [options]
```

## Options

| Option | Effect |
| :--- | :--- |
| `-r`, `--regex` | Interpret `<find>` as `jam::RegExp`. Expand `$0`..`$9`, `${n}`, and `$$` (literal `$`) in `<with>`. |
| `-i`, `--ignore-case` | Match ASCII letters case-insensitively. |
| `--lines <A:B>` | Restrict replacements to 1-based inclusive line range `A` through `B`. |
| `--with-stdin` | Read `<with>` verbatim from stdin. Requires explicit file paths; omit `<with>`. |
| `--with-file <path>` | Read `<with>` verbatim from `<path>`. Omit `<with>`. |
| `--include <glob>` | Include only matching files when you pass a directory path. Repeatable. |
| `--exclude <glob>` | Exclude matching files when you pass a directory path. Repeatable. |
| `--hidden` | Include dot-prefixed files and directories in directory walks. |
| `--no-ignore` | Do not apply `.gitignore` filters in directory walks. |
| `--apply` | Write planned edits to disk. Requires `--expect N`. |
| `--expect <N>` | Exact count of match replacements across all target files (`N >= 0`). |

## Behavior and Edge Cases

- **Dry-run (default):** SAW prints unified diff hunks and `saw: dry-run — <E> edit(s) in <F> file(s); nothing written. Pass --apply --expect <E> to write.`. Zero bytes change on disk.
- **Apply (`--apply --expect N`):** When `actual == N`, SAW saves an undo snapshot and swaps files atomically. SAW preserves line endings and file metadata, verifies SHA-256 hashes, and prints `saw: applied <E> edit(s) in <F> file(s) (tx: <tx-id>)`.
- **Piped stdin:** When you omit `[path...]` on piped stdin, dry-run processes `<stdin>`. Passing `--apply` exits `2`.
- **`--apply` without `--expect`:** SAW fails with exit `2` before touching disk.
- **`--expect N` mismatch:** SAW prints the dry-run diff to stdout and the mismatch error to stderr. Zero bytes change; exit `1`.
- **`--apply --expect 0` with 0 matches:** SAW writes zero files and creates no undo transaction. SAW prints `saw: applied 0 edit(s) in 0 file(s)` and exits `0`.
- **`<find>` equals `<with>`:** Dry-run counts matches normally. `--apply` skips disk writes for identical files and prints `saw: no content changed; nothing written`.
- **Overlapping matches:** SAW advances past each matched span left-to-right (`"aa"` in `"aaa"` yields 1 match).
- **Regex zero-length match:** For patterns such as `^` or `$`, SAW applies once and advances 1 byte.
- **Invalid capture reference:** SAW fails before any write with exit `2`.
- **External modification after the plan:** SAW does not write the changed file. SAW rolls back all written files from pre-edit snapshots and exits `2`.
- **One file named two times:** SAW edits and counts it one time.
- **Backslash:** A backslash in `<with>` is literal. Use `--with-file` or `--with-stdin` to insert a newline.
- **Hard links:** SAW refuses a file with more than one hard link before any write. A dry run is refused too. Exit `2`.
- **Read-only files:** SAW refuses a file without write access before any write. Exit `2`.

## Exit Codes and Errors

| Code | Meaning |
| :--- | :--- |
| `0` | Dry-run preview completed, or `--apply` succeeded. |
| `1` | `--expect N` mismatch. |
| `2` | Usage error, bad regex or capture, missing `--expect`, stdin apply, binary file, hard-linked or read-only target, or write failure. |

| Condition | Stderr Message | Exit |
| :--- | :--- | :--- |
| Empty `<find>` | `saw: pattern cannot be empty` | `2` |
| `--apply` without `--expect` | `saw: --apply requires --expect <N>` | `2` |
| `--expect` not a non-negative integer | `saw: invalid integer for --expect: <value>` | `2` |
| `--expect N` mismatch | `saw: expected <N> edit(s), found <actual>; nothing written` | `1` |
| `--apply` on stdin | `saw: cannot --apply on stdin; specify file paths` | `2` |
| Named file is binary | `saw: binary file: <path>` | `2` |
| Hard-linked file | `saw: hard-linked file: <path>` | `2` |
| Read-only file | `saw: read-only file: <path>` | `2` |
| Invalid regex | `saw: invalid regex: <detail>` | `2` |
| Invalid capture group | `saw: invalid capture group $<k> in replacement` | `2` |
| File not writable or swap failed | `saw: write failed for <path>: <reason>; rolled back` | `2` |

## Examples

```
saw replace "DBG" "debug::Log" src/Foo.cpp
saw replace "DBG" "debug::Log" src/Foo.cpp --apply --expect 3
saw replace "a/b" "c/d" file.txt --apply --expect 2
saw replace -r "^model: .*" "model: opus" agent.md --apply --expect 1
```
