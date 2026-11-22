# saw lines

Read, delete, replace, or insert exact 1-based line ranges in one or more files.

## Invocation

```
saw lines <range> [path...] [options]
```

## Range Grammar

| Form | Meaning |
| :--- | :--- |
| `A` | Single 1-based line `A`. |
| `A:B` | Inclusive 1-based lines `A` through `B` (`A <= B`). |
| `A:` | From line `A` through the final line of the file. |
| `:B` | From line `1` through line `B`. |

## Options

| Option | Mode | Effect |
| :--- | :--- | :--- |
| `-n`, `--line-number` | Read | Prefix each line with `<line>\t`. |
| `--hash` | Read | Prefix each line with `<line>:<hhhh>\|`. |
| `--delete` | Mutate | Delete all lines in `<range>`. |
| `--replace <text>` | Mutate | Replace lines in `<range>` with `<text>`. |
| `--insert-before <text>` | Mutate | Insert `<text>` before line `A` (requires single line `A`). |
| `--insert-after <text>` | Mutate | Insert `<text>` after line `A` (single line `A`, or `0` to prepend at top). |
| `--stdin` | Mutate | Read replacement or inserted text verbatim from stdin (omit inline `<text>`). |
| `--verify-hash <hhhh>` | Mutate | Require line `A` to have content hash `<hhhh>` before mutating. |
| `--apply` | Mutate | Write planned edits to disk. Requires `--expect N`. |
| `--expect <N>` | Mutate | Exact count of line operations applied. |

Specify at most one mutating flag: `--delete`, `--replace`, `--insert-before`, or `--insert-after`.

## Behavior and Edge Cases

- **Read mode:** SAW emits the selected lines from named files or piped stdin and exits `0`.
- **Range out of bounds on read:** When `A > lineCount`, SAW emits nothing and exits `0`. When `B > lineCount`, SAW clamps `B` to `lineCount`.
- **Range out of bounds on mutate:** SAW fails before any write with exit `2`.
- **Mutate dry-run (default):** SAW prints unified diff hunks and `saw: dry-run — <E> edit(s) in <F> file(s); nothing written. Pass --apply --expect <E> to write.`.
- **Apply (`--apply --expect N`):** SAW saves an undo snapshot, swaps files atomically, and prints `saw: applied <E> edit(s) in <F> file(s) (tx: <tx-id>)`.
- **`--insert-after 0` on empty file:** On a 0-byte file, SAW inserts text as line 1 and counts `1` edit.
- **Insert range check:** `--insert-before` and `--insert-after` reject ranges such as `A:B` with exit `2`.
- **Pure insertion count:** `--insert-before` or `--insert-after` counts as `1` edit.
- **`--verify-hash` mismatch:** SAW fails before any write with exit `2`.

## Exit Codes and Errors

| Code | Meaning |
| :--- | :--- |
| `0` | Read succeeded, dry-run preview completed, or `--apply` succeeded. |
| `1` | `--expect N` mismatch. |
| `2` | Invalid range, out-of-bounds mutate, hash mismatch, missing `--expect`, conflicting flags, or I/O error. |

| Condition | Stderr Message | Exit |
| :--- | :--- | :--- |
| Invalid range syntax or `A > B` or `A < 1` (except `0` for `--insert-after`) | `saw: invalid line range: <range>` | `2` |
| Insert given a range `A:B` | `saw: insert requires a single line number, got <range>` | `2` |
| Multiple mutating flags | `saw: specify at most one of --delete, --replace, --insert-before, --insert-after` | `2` |
| Out-of-bounds range on mutate | `saw: line range <range> out of bounds for <path> (<lineCount> lines)` | `2` |
| Hash precondition failed | `saw: hash mismatch at <path>:<line>: expected <expected>, got <actual>` | `2` |
| `--apply` without `--expect` | `saw: --apply requires --expect <N>` | `2` |
| `--expect` mismatch | `saw: expected <N> edit(s), found <actual>; nothing written` | `1` |

## Examples

```
saw lines 1:10 DEBT.md
saw lines 10:25 --hash Source/Main.cpp
saw lines 15:22 --delete DEBT.md
saw lines 15:22 --delete DEBT.md --apply --expect 1
saw lines 4 --insert-after --stdin DEBT.md --apply --expect 1
```
