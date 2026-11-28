# saw find

Search files or stdin for literal strings or regex patterns.

## Invocation

```
saw find <pattern> [path...] [options]
```

## Options

| Option | Effect |
| :--- | :--- |
| `-r`, `--regex` | Interpret `<pattern>` as a `jam::RegExp` pattern instead of literal bytes. See Regex Grammar. |
| `-i`, `--ignore-case` | Match ASCII letters case-insensitively. |
| `-x`, `--exact` | Match only when `<pattern>` covers the entire line without its line ending. |
| `-v`, `--invert` | Emit lines that do not match `<pattern>`. |
| `-c`, `--count` | Print `<path>:<count>` (or `<count>` for one input) instead of matching lines. A walked file with count `0` prints no row. |
| `-l`, `--files-with-matches` | Print only the path of each input with a match. |
| `-o`, `--only-matching` | Print only each matched text, one for each output line. |
| `-w`, `--word` | Match only where no word byte (`A`–`Z`, `a`–`z`, `0`–`9`, `_`) is directly before or after the match. |
| `-e <pattern>`, `--pattern <pattern>` | Add a pattern. Repeatable. With `-e`, every positional is a path. A line matches when one pattern matches. |
| `--files` | Print the files that the walk reads, sorted. Takes paths only. With `-q`, print nothing. Exit `1` when the list is empty. |
| `--max-depth <N>`, `-d <N>` | With `--files`: list entries up to depth `N`. The children of the walk root are depth 1. |
| `--type <t>`, `-t <t>` | With `--files`: list `file`, `dir`, or `symlink` entries (`f`, `d`, `l`). Repeatable. Default `file`. |
| `--size <spec>`, `-S <spec>` | With `--files`: `+N<unit>` at least, `-N<unit>` at most, `N<unit>` exact. Units `b`, `k`, `m`, `g`, `t`, `ki`, `mi`, `gi`, `ti`. A unit is required. Repeatable. |
| `--changed-within <time>` | With `--files`: keep entries modified after a duration (`2h30m`), a date (`YYYY-MM-DD`), or `@<epoch seconds>`. |
| `--changed-before <time>` | With `--files`: keep entries modified before `<time>`. Same forms. |
| `--sort <name\|mtime\|size>` | With `--files`: order of the entries. Default `name`. |
| `--reverse` | With `--files`: reverse the order. |
| `--json` | With `--files`: one JSON object per entry with `path`, `type`, `size`, `mtime`, `mode`, `target`. A link whose target is missing reports the Unix epoch (time 0) as `mtime`, in the local time zone, and size 0. |
| `-q`, `--quiet` | Emit nothing to stdout; exit `0` on first match or `1` on no match. |
| `-n`, `--line-number` | Prefix output with 1-based line numbers (default: on). |
| `--no-line-number` | Omit line numbers from output. |
| `--hash` | Prefix each matched line with `<line>:<hhhh>\|` instead of `<line>:`. |
| `-C <N>`, `--context <N>` | Show `N` lines of context before and after each match (`N >= 0`). |
| `-A <N>`, `--after-context <N>` | Show `N` lines of context after each match. Overrides `-C` for that side. |
| `-B <N>`, `--before-context <N>` | Show `N` lines of context before each match. Overrides `-C` for that side. |
| `--after <N>` | Inspect only lines with 1-based line number greater than `N` (`line > N`). |
| `--lines <A:B>` | Restrict search to 1-based inclusive line range `A` through `B`. |
| `--first` | Stop after the first match per file (or first match for one input). |
| `--include <glob>` | Include only files whose name, path relative to the walk root, or printed path matches `<glob>`. Repeatable. |
| `--exclude <glob>` | Exclude files whose name, path relative to the walk root, or printed path matches `<glob>`. Repeatable. |
| `--hidden` | Include dot-prefixed files and directories in directory walks. |
| `--no-ignore` | Do not apply `.gitignore` filters in directory walks. |

## Behavior and Edge Cases

- **Output format:** Single input prints `<line>:<text>`. Multiple files or a directory walk print `<path>:<line>:<text>`. With `--hash`, SAW prints `<line>:<hhhh>|<text>` (or `<path>:<line>:<hhhh>|<text>`).
- **Default input:** When you omit `[path...]`, piped stdin reads `<stdin>`. A TTY walks `.`. Empty stdin exits `2` with `saw: missing path argument`.
- **Multiple matches on one line:** SAW emits that line once. `-c` counts matching lines.
- **No match found:** SAW writes nothing to stdout (or `0` with `-c`) and exits `1`.
- **`--after N` beyond EOF:** SAW emits nothing and exits `1`.
- **No trailing newline at EOF:** SAW matches normally and ends the stdout line with `\n`.
- **Directory walks:** SAW skips `.git`, ignored paths, dot-prefixed entries, and binary files silently. Naming a binary file directly exits `2`.
- **Pipes:** A path that names a pipe fails as an unreadable file. Use stdin.
- **Missing or unreadable paths:** SAW prints one error for each, prints the matches of the readable files, and exits `2`.
- **Combined output flags:** The first flag in the order `-q`, `-l`, `-c`, `-o` decides. `-o` prints no context lines. `-v` with `-o` prints nothing.
- **Quiet with errors:** With `-q` and a match, SAW prints no error and exits `0`. Without a match, SAW prints the errors and exits `2`.
- **Newline in pattern:** A pattern that holds a newline never matches. SAW exits `2`.

## Regex Grammar

With `-r`, SAW reads `<pattern>` as ECMAScript regex syntax.

- **Supported:** Character classes (`[a-z]`, `[^0-9]`), POSIX classes (`[[:digit:]]`, `[[:^alpha:]]`), `\d`, `\w`, `\s`, `\b`, `\A`, groups, named groups (`(?<name>...)`), lazy quantifiers (`*?`, `+?`), lookaround, backreferences (`\1`), Unicode properties (`\p{...}`), and inline flags (`(?i)`, `(?m)`, `(?s)`).
- **Inline flags:** A flag such as `(?i)` applies to the end of its group. It also applies to the later alternatives of that group. `(?m)` makes `^` and `$` match at each line break. `(?s)` makes `.` match a line terminator. A flag is removed with `-`, as in `(?i-s)`.
- **Text start:** `\A` matches at the start of each line that SAW searches.
- **Rejected:** `(?x)`. SAW exits `2` with `saw: invalid regex: <detail>`.
- **Case:** Use `-i` in place of `(?i)`. SAW compiles `-i` as `(?i:<pattern>)`.
- **Linear time:** A pattern runs in linear time, with three exceptions: a backreference, a lookaround that holds more than one character, and a pattern that compiles to more than 2048 instructions.
- **Step limit:** The exceptions run under a step limit and a depth limit. The match spans of a repeated group that can be empty (`(a*)*`) do too. Over a limit, SAW exits `2` with `saw: regex exceeded its step limit: <pattern>`.
- **Nesting:** A pattern with more than 250 nested groups is invalid.

## Exit Codes and Errors

| Code | Meaning |
| :--- | :--- |
| `0` | At least one match found. |
| `1` | No match found. |
| `2` | Usage error, empty pattern, newline in pattern, invalid regex, regex over its step limit, missing or unreadable path, or binary file. |

| Condition | Stderr Message | Exit |
| :--- | :--- | :--- |
| Missing `<pattern>` | `saw: find requires <pattern>` | `2` |
| Empty `<pattern>` | `saw: pattern cannot be empty` | `2` |
| Newline in `<pattern>` | `saw: pattern cannot contain a newline` | `2` |
| Invalid regex | `saw: invalid regex: <detail>` | `2` |
| Regex over its step limit | `saw: regex exceeded its step limit: <pattern>` | `2` |
| Invalid `-A`, `-B`, `-C`, or `--after` integer | `saw: invalid integer for <flag>: <value>` | `2` |
| Named path does not exist | `saw: not found: <path>` | `2` |
| Unreadable file or folder | `saw: cannot read <path>` | `2` |
| Named file is binary | `saw: binary file: <path>` | `2` |

## Examples

```
saw find "DEBT-" DEBT.md
saw find -x --after 12 --first -- "---" DEBT.md
saw find -q -x "entry" .gitignore
saw find -v -x "entry" .gitignore
saw find -r "return [a-z]+;" Source/ --include "*.cpp"
```
