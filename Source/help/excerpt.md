# saw excerpt

Return exact line blocks from one or more files. Each block carries a header with the path, the line range, the line count of the file, and a hash of the block bytes.

## Invocation

```
saw excerpt [target...] [--context <N>]
```

## Targets

Targets come from the arguments. When there are none, SAW reads one target for each line of stdin.

| Target | Gives |
| :--- | :--- |
| `<path>` | The whole file. |
| `<path>:<A>` or `<path>:<A>-<B>` | Line `A`, or lines `A` through `B`. `B` beyond the last line clamps to it. |
| `<path> ~ <literal>` | Each line that holds the literal, with `--context` lines on each side. Overlapping ranges merge. |

The range suffix is the last `:` followed only by digits, or by digits, `-`, and digits. A Windows drive letter is part of the path.

## Options

| Option | Effect |
| :--- | :--- |
| `--context <N>` | Show `N` lines on each side of each literal match. The default is `0`. |

## Output

SAW writes the receipt line first. Then it writes one block for each target, in target order.

```
# <n> targets, <f> found, <m> missing
## <i> <path>:<A>-<B> of <lines> sha256:<16 hex>
<line number><TAB><line bytes>
## <i> <path> NOT FOUND
```

- `<n>` is the count of targets. `<i>` counts the targets from `1`.
- A literal target prints one block for each merged range. Every block of one target has the same `<i>`.
- The hash is the first 16 hex digits of the SHA-256 of the block bytes. The line endings are included.
- Each line prints its bytes without its line ending, then a line feed.
- A line that is not valid UTF-8 prints each invalid byte as `\xNN`. The header of that block ends with ` escaped`.

## States

In place of a block, a target prints one of these states:

| State | Meaning |
| :--- | :--- |
| `NOT FOUND` | The file does not exist. |
| `UNREADABLE` | The path is a folder, or the file cannot be read. |
| `BINARY` | The file holds a NUL byte. |
| `EMPTY` | The file has 0 bytes. |
| `NO MATCH` | A literal target matches no line. |
| `OUT OF RANGE` | The range starts at `0`, ends before it starts, does not fit an integer, or starts beyond the last line. |

## Exit Codes and Errors

| Code | Meaning |
| :--- | :--- |
| `0` | Every target gives a block of lines. |
| `1` | One target gives a state instead of a block. |
| `2` | Usage error. |

| Condition | Stderr Message | Exit |
| :--- | :--- | :--- |
| Invalid `--context` integer | `saw: invalid integer for --context: <value>` | `2` |
| No target and no stdin lines | `saw: excerpt requires <target>` | `2` |

## Examples

```
saw excerpt DEBT.md:12-18
saw excerpt "Source/Main.cpp ~ Stdio" --context 2
printf 'DEBT.md:1-5\nREADME.md\n' | saw excerpt
```
