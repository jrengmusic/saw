# saw count

Count the lines, words, and bytes of files, or of standard input.

## Invocation

```
saw count [path...] [options]
```

## Options

| Option | Effect |
| :--- | :--- |
| `-l`, `--lines` | Print the line count. |
| `-w`, `--words` | Print the word count. |
| `-c`, `--bytes` | Print the byte count. |

## Behavior and Edge Cases

- **Input source:** With no path, SAW reads standard input. Standard input must not be a terminal.
- **Lines:** A line is a newline byte. A last line without a newline counts as a line.
- **Words:** A word is a run of bytes that are not space, tab, newline, vertical tab, form feed, or carriage return.
- **Bytes:** The count is the number of bytes of the input.
- **Selection:** With no count option, SAW prints all three counts. Each option selects one count. The selected counts print in the order lines, words, bytes.
- **Width:** A single count of a single input prints without padding. Otherwise each number is right-aligned to the digit count of the total size of the named files. Standard input gives at least seven columns.
- **Name:** A named file ends its line with its path. Standard input has no name.
- **Total:** With more than one path, SAW prints a `total` line with the sums of the counts.
- **Counting:** SAW counts the bytes of each input in place. No input is copied.

## Exit Codes and Errors

| Code | Meaning |
| :--- | :--- |
| `0` | The counts are printed. |
| `2` | Missing or unreadable path, terminal standard input, or unknown flag. |

| Condition | Stderr Message | Exit |
| :--- | :--- | :--- |
| Standard input is a terminal | `saw: missing path argument` | `2` |
| Path does not exist | `saw: not found: <path>` | `2` |
| Unknown flag | `saw: unknown flag: <flag>` | `2` |

## Examples

```
saw count a.txt
saw count -l src/main.cpp
saw count -w a.txt b.txt
saw count < draft.txt
```
