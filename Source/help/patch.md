# saw patch

Apply a structured, multi-file batch of column-span, line-range, delete, or insert edits in one atomic transaction.

## Invocation

```
saw patch [options]
```

## Options

| Option | Effect |
| :--- | :--- |
| `--file <path>` | Read JSON edit batch from `<path>` instead of stdin. |
| `--apply` | Write planned edits to disk. Requires `--expect N`. |
| `--expect <N>` | Exact number of edit entries in the JSON array (`N == array.length`). |

## Batch JSON Schema

Input is a JSON array of edit objects. Every object requires `"file"` (string path) and one of four mutually exclusive shapes:

1. **Column-span edit** (1-based line, 0-based byte columns `[startCol, endCol)`, optional `"hash"`):
   ```json
   { "file": "src/Foo.cpp", "line": 42, "startCol": 8, "endCol": 10, "text": "not ", "hash": "a7c1" }
   ```
2. **Line delete** (1-based single `"line"` or inclusive `"startLine"`/`"endLine"`, optional `"hash"`):
   ```json
   { "file": "src/Foo.cpp", "line": 12, "delete": true }
   ```
   or `{ "file": "src/Foo.cpp", "startLine": 12, "endLine": 15, "delete": true }`
3. **Line-range replace** (1-based inclusive `"startLine"`/`"endLine"`, optional `"hash"`):
   ```json
   { "file": "src/Foo.cpp", "startLine": 10, "endLine": 12, "text": "new line 1\nnew line 2", "hash": "a7c1" }
   ```
4. **Line insert** (1-based `"insertAfter"` or `"insertBefore"`; `0` allowed for `"insertAfter"`, optional `"hash"`):
   ```json
   { "file": "src/Foo.cpp", "insertAfter": 10, "text": "// banner", "hash": "a7c1" }
   ```

## Behavior and Edge Cases

- **Input source:** SAW reads JSON from stdin or `--file <path>`. When stdin is a TTY and you omit `--file`, SAW exits `2`.
- **Pre-validation:** SAW parses the full JSON array and checks `--expect N` before reading or writing files.
- **Canonical grouping:** SAW resolves each symbolic link to its final target and groups edits by canonical path in sorted order.
- **Bounds and hash checks:** SAW verifies all line and column bounds (`0 <= startCol <= endCol <= lineByteLength`) and optional `"hash"` preconditions.
- **Conflict rejection:** SAW rejects overlapping column spans on one line (`prev.endCol > curr.startCol` or `prev.startCol == curr.startCol`). SAW rejects column spans mixed with delete or replace on the same line, and overlapping line ranges. SAW rejects an insert whose line is in a deleted or replaced range, and two inserts with the same line.
- **Column positions:** Each column span counts bytes of the original line, whatever the order of the entries.
- **Multi-line `"text"` in column-span edit:** SAW allows `\n` in column-span `"text"` and normalizes it to the file's line ending.
- **Dry-run (default):** SAW prints unified diff hunks and `saw: dry-run — <E> edit(s) in <F> file(s); nothing written. Pass --apply --expect <E> to write.`.
- **Apply (`--apply --expect N`):** SAW records one undo transaction, swaps all files atomically, and prints `saw: applied <E> edit(s) in <F> file(s) (tx: <tx-id>)`. Any file failure rolls back all files.
- **Empty JSON array `[]`:** Dry-run prints `saw: dry-run — 0 edits; nothing written.` and exits `0`. `--apply --expect 0` writes nothing and exits `0`.

## Exit Codes and Errors

| Code | Meaning |
| :--- | :--- |
| `0` | Dry-run preview completed, or `--apply` succeeded. |
| `1` | `--expect N` mismatch. |
| `2` | Invalid JSON, invalid entry, conflict, out-of-bounds, hash mismatch, missing `--expect`, or I/O error. |

| Condition | Stderr Message | Exit |
| :--- | :--- | :--- |
| Invalid JSON or root not an array | `saw: invalid patch JSON: <detail>` | `2` |
| Missing/unknown/conflicting fields on entry `i` | `saw: invalid patch entry <i>: <reason>` | `2` |
| Overlapping edits on same line or range | `saw: conflicting edits in <path> at line <line>` | `2` |
| Column or line out of bounds | `saw: out of bounds in <path> at entry <i>` | `2` |
| Hash precondition failed | `saw: hash mismatch at <path>:<line>: expected <expected>, got <actual>` | `2` |
| `--apply` without `--expect` | `saw: --apply requires --expect <N>` | `2` |
| `--expect N` mismatch | `saw: expected <N> edit(s), found <actual>; nothing written` | `1` |
| Named file missing or binary | `saw: not found: <path>` / `saw: binary file: <path>` | `2` |

## Examples

```
saw patch --file edits.json
saw patch --file edits.json --apply --expect 4
saw patch --apply --expect 4 < edits.json
```
