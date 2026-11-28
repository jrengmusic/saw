# saw sort

Sort the lines of files, or of standard input, as one line set.

## Invocation

```
saw sort [path...] [options]
```

## Options

| Option | Effect |
| :--- | :--- |
| `-n`, `--numeric-sort` | Compare by numeric value. |
| `-r`, `--reverse` | Reverse the order. |
| `-u`, `--unique` | Print one line of each run of equal lines. |
| `-c`, `--count` | Prefix each distinct line with its count, as uniq -c. |

## Behavior and Edge Cases

- **Input source:** With no path, SAW reads standard input. Standard input must not be a terminal. Each path is a file. All inputs form one line set.
- **Lines:** A line ends at a newline. The newline is not part of the compared text. A last line without a newline is written with one.
- **Default order:** Lines compare by their bytes. A shorter line that is a prefix of a longer line sorts first.
- **Numeric order (`-n`):** Leading spaces and tabs are skipped. A leading `-` makes the number negative. A `+` is not a sign. Leading zeros are ignored. A `.` starts the fraction. A line with no number has the value zero. Ties fall back to the byte order of the whole line.
- **Reverse (`-r`):** Every comparison is reversed, including the fallback.
- **Unique (`-u`):** Lines that compare equal form a run, and SAW prints the first line of each run. Under `-n`, two lines with equal numbers are equal, even when their text differs.
- **Count (`-c`):** SAW groups adjacent lines whose bytes are equal and prints each group as its count in seven columns, one space, then the line.
- **Sorting:** The sort is stable, so equal lines keep their input order.

## Exit Codes and Errors

| Code | Meaning |
| :--- | :--- |
| `0` | The sorted lines are printed. |
| `2` | Missing or unreadable path, or terminal standard input. |

| Condition | Stderr Message | Exit |
| :--- | :--- | :--- |
| Standard input is a terminal | `saw: missing path argument` | `2` |
| Path does not exist | `saw: not found: <path>` | `2` |
| Unknown flag | `saw: unknown flag: <flag>` | `2` |

## Examples

```
saw sort words.txt
saw sort -n numbers.txt
saw sort -u -c words.txt
saw sort < draft.txt
```
