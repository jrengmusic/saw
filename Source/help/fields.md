# saw fields

Extract and format delimited columns from lines. This command is read-only.

## Invocation

```
saw fields <spec> [path...] [options]
```

`<spec>` is a comma-separated list of 1-based field indices. Negative indices count from the end (`-1` is last field, `-2` is second-to-last). Example: `1,3,-1`.

## Options

| Option | Effect |
| :--- | :--- |
| `-d <delim>`, `--delimiter <delim>` | Split lines on literal `<delim>`. Without `-d`, split on runs of ASCII space or tab and trim line ends. |
| `-o <out>`, `--output-delimiter <out>` | Join fields with `<out>`. Defaults to space without `-d`, or `<delim>` with `-d`. |
| `--match <pattern>` | Process only lines matching `<pattern>` (literal default; `-r` for regex). |
| `-r`, `--regex` | Interpret `--match <pattern>` as `jam::RegExp`. |
| `--lines <A:B>` | Restrict processing to 1-based inclusive line range `A:B`. |
| `--after <N>` | Process only lines with 1-based line number greater than `N`. |
| `--first` | Stop after the first emitted output line per file. |
| `--strict` | Exit `2` when any processed line lacks a requested field index. Default omits missing fields as empty. |

## Behavior and Edge Cases

- **Input source:** SAW reads named files or piped stdin. Omitting `[path...]` on a TTY fails with exit `2`.
- **Empty line with default delimiter:** Produces 0 fields and emits an empty line, or fails under `--strict`.
- **Index `0` in `<spec>`:** Fails with exit `2`: `saw: invalid field spec: <spec> (1-based or negative)`.
- **Negative index past start:** Emits an empty string, or fails under `--strict` with `saw: field <idx> out of range at <path>:<line>`.
- **No lines matched `--match`:** SAW emits nothing and exits `1`.

## Exit Codes and Errors

| Code | Meaning |
| :--- | :--- |
| `0` | Success (output emitted, or no `--match` filter failed). |
| `1` | No lines matched `--match`. |
| `2` | Invalid `<spec>`, empty `-d`, missing field under `--strict`, missing path, or binary file. |

| Condition | Stderr Message | Exit |
| :--- | :--- | :--- |
| Invalid `<spec>` | `saw: invalid field spec: <spec>` | `2` |
| Empty `-d ""` | `saw: delimiter cannot be empty` | `2` |
| Missing field under `--strict` | `saw: field <idx> out of range at <path>:<line>` | `2` |
| Named file is binary | `saw: binary file: <path>` | `2` |

## Examples

```
saw fields 1,-1 -d ":" /path/to/file.txt
saw fields 2 -d '"' version.txt
saw fields 1,-1 data.txt
saw fields 2 --match "Version:" --first info.txt
```
