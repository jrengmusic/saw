# saw write

Write the complete text from standard input to one file, verbatim.

## Invocation

```
saw write <path> [--apply --expect 1]
```

## Options

| Option | Effect |
| :--- | :--- |
| `--apply` | Write the file. Requires `--expect 1`. |
| `--expect <N>` | Exact number of edits. One write is one edit, so `N` is `1`. |

## Behavior and Edge Cases

- **Input source:** SAW reads the complete text from standard input. Standard input must not be a terminal.
- **Verbatim:** SAW writes the bytes of standard input as they are. It adds no line ending.
- **Dry run (default):** SAW prints the unified diff and writes nothing.
- **Apply (`--apply --expect 1`):** SAW creates the file, or replaces it.
- **Missing folders:** SAW creates the missing parent folders at apply.
- **Empty input:** Empty standard input writes an empty file.
- **NUL bytes:** SAW refuses standard input with a NUL byte, and writes nothing.
- **Hard links and read-only files:** SAW refuses a hard-linked target and a read-only target.
- **Symbolic links:** SAW writes the target of a symbolic link.
- **Undo:** `saw undo <tx-id>` deletes the created file. It also deletes each created folder that is empty.

## Exit Codes and Errors

| Code | Meaning |
| :--- | :--- |
| `0` | Dry-run preview completed, or `--apply` succeeded. |
| `1` | `--expect N` mismatch. |
| `2` | Missing path, extra argument, terminal input, NUL byte, hard-linked or read-only target, or I/O error. |

| Condition | Stderr Message | Exit |
| :--- | :--- | :--- |
| Path argument omitted | `saw: missing path argument` | `2` |
| Second positional argument | `saw: unexpected argument: <arg>` | `2` |
| Standard input is a terminal | `saw: write requires stdin` | `2` |
| NUL byte in standard input | `saw: binary file: <path>` | `2` |
| `--apply` without `--expect` | `saw: --apply requires --expect <N>` | `2` |

## Examples

```
saw write src/New.cpp < draft.txt
saw write src/New.cpp --apply --expect 1 < draft.txt
saw undo <tx-id>
```
