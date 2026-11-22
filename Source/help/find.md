# saw find

Search files or stdin for literal strings or regex patterns.

## Invocation

```
saw find <pattern> [path...] [options]
```

## Options

| Option | Effect |
| :--- | :--- |
| `-r`, `--regex` | Interpret `<pattern>` as a `jam::RegExp` pattern instead of literal bytes. |
| `-i`, `--ignore-case` | Match ASCII letters case-insensitively. |
| `-x`, `--exact` | Match only when `<pattern>` covers the entire line without its line ending. |
| `-v`, `--invert` | Emit lines that do not match `<pattern>`. |
| `-c`, `--count` | Print `<path>:<count>` (or `<count>` for one input) instead of matching lines. |
| `-q`, `--quiet` | Emit nothing to stdout; exit `0` on first match or `1` on no match. |
| `-n`, `--line-number` | Prefix output with 1-based line numbers (default: on). |
| `--no-line-number` | Omit line numbers from output. |
| `--hash` | Prefix each matched line with `<line>:<hhhh>\|` instead of `<line>:`. |
| `-C <N>`, `--context <N>` | Show `N` lines of context before and after each match (`N >= 0`). |
| `--after <N>` | Inspect only lines with 1-based line number greater than `N` (`line > N`). |
| `--lines <A:B>` | Restrict search to 1-based inclusive line range `A` through `B`. |
| `--first` | Stop after the first match per file (or first match for one input). |
| `--include <glob>` | Include only files whose name or relative path matches `<glob>`. Repeatable. |
| `--exclude <glob>` | Exclude files whose name or relative path matches `<glob>`. Repeatable. |
| `--hidden` | Include dot-prefixed files and directories in directory walks. |
| `--no-ignore` | Do not apply `.gitignore` filters in directory walks. |

## Behavior and Edge Cases

- **Output format:** Single input prints `<line>:<text>`. Multiple files or a directory walk print `<path>:<line>:<text>`. With `--hash`, SAW prints `<line>:<hhhh>|<text>` (or `<path>:<line>:<hhhh>|<text>`).
- **Default input:** When you omit `[path...]`, piped stdin reads `<stdin>`. A TTY walks `.`.
- **Multiple matches on one line:** SAW emits that line once. `-c` counts matching lines.
- **No match found:** SAW writes nothing to stdout (or `0` with `-c`) and exits `1`.
- **`--after N` beyond EOF:** SAW emits nothing and exits `1`.
- **No trailing newline at EOF:** SAW matches normally and ends the stdout line with `\n`.
- **Directory walks:** SAW skips `.git`, ignored paths, dot-prefixed entries, and binary files silently. Naming a binary file directly exits `2`.

## Exit Codes and Errors

| Code | Meaning |
| :--- | :--- |
| `0` | At least one match found. |
| `1` | No match found. |
| `2` | Usage error, empty pattern, invalid regex, missing path, or binary file. |

| Condition | Stderr Message | Exit |
| :--- | :--- | :--- |
| Missing `<pattern>` | `saw: find requires <pattern>` | `2` |
| Empty `<pattern>` | `saw: pattern cannot be empty` | `2` |
| Invalid regex | `saw: invalid regex: <detail>` | `2` |
| Invalid `-C` or `--after` integer | `saw: invalid integer for <flag>: <value>` | `2` |
| Named path does not exist | `saw: not found: <path>` | `2` |
| Named file is binary | `saw: binary file: <path>` | `2` |

## Examples

```
saw find "DEBT-" DEBT.md
saw find -x "---" --after 12 --first DEBT.md
saw find -q -x "entry" .gitignore
saw find -v -x "entry" .gitignore
saw find -r "return [a-z]+;" Source/ --include "*.cpp"
```
