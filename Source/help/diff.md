# saw diff

Compare two files, or a file and standard input, and print the unified diff of their lines.

## Invocation

```
saw diff [options] <file> <file|-> 
```

## Options

| Option | Effect |
| :--- | :--- |
| `-q`, `--quiet` | Compare the raw bytes and print nothing. Exit `0` when the bytes are equal, `1` when they differ. |

## Behavior and Edge Cases

- **Paths:** Exactly two paths are required. The path `-` names standard input, and it may be given once.
- **Header:** The header names the first path with `---` and the second path with `+++`. No `(planned)` suffix is printed.
- **Hunks:** The hunks follow `diff -u`, with three lines of context around each change.
- **Identical files:** A default comparison of identical text prints nothing and exits `0`.
- **Quiet mode:** Two files compare with the byte content of each file. A file and standard input compare the byte content of both inputs.
- **Text:** The default mode compares lines of text. A binary file, or binary standard input, is refused, as for every text verb.
- **Standard input:** Standard input must not be a terminal.

## Exit Codes and Errors

| Code | Meaning |
| :--- | :--- |
| `0` | The inputs are the same. |
| `1` | The inputs differ. |
| `2` | Missing or unreadable path, terminal standard input, binary file, or a bad argument. |

| Condition | Stderr Message | Exit |
| :--- | :--- | :--- |
| Fewer than two paths | `saw: missing path argument` | `2` |
| More than two paths | `saw: unexpected argument: <arg>` | `2` |
| Standard input is a terminal | `saw: missing path argument` | `2` |
| Path does not exist | `saw: not found: <path>` | `2` |
| Unknown flag | `saw: unknown flag: <flag>` | `2` |

## Examples

```
saw diff a.txt b.txt
saw diff -q a.txt b.txt
saw diff a.txt - < b.txt
```
