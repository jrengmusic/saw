# saw json

Run a jq 1.8.2 filter over JSON input, with the arguments and the filter language of jq.

## Invocation

```
saw json [options] <filter> [file...]
saw json [options] --args <filter> [strings...]
saw json [options] --jsonargs <filter> [JSON_TEXTS...]
saw json --run-tests [file] [--skip n] [--take n]
```

## Options

| Option | Effect |
| :--- | :--- |
| `-n`, `--null-input` | Use `null` as the single input value. |
| `-R`, `--raw-input` | Read each line as a string. |
| `-s`, `--slurp` | Read all inputs into one array. With `-R`, one string. |
| `-c`, `--compact-output` | Compact output instead of pretty-printed output. |
| `-r`, `--raw-output` | Output strings without quotes and escapes. |
| `--raw-output0` | Raw output with a NUL byte after each output. |
| `-j`, `--join-output` | Raw output without a newline after each output. |
| `-a`, `--ascii-output` | Output only ASCII, with escape sequences. |
| `-S`, `--sort-keys` | Sort the keys of each object on output. |
| `-C`, `--color-output` | Colour the output. |
| `-M`, `--monochrome-output` | Disable colour. |
| `--tab` | Indent with tabs. |
| `--indent <n>` | Indent with `n` spaces, at most 7. |
| `--unbuffered` | Write each output as it is made. |
| `--stream` | Run the filter over the stream events of each input. |
| `--stream-errors` | Implies `--stream`. |
| `--seq` | Read and write `application/json-seq`. |
| `-f`, `--from-file` | Load the filter from a file. |
| `-L`, `--library-path <dir>` | Search modules in a directory. |
| `--arg <name> <value>` | Set `$name` to a string. |
| `--argjson <name> <json>` | Set `$name` to a JSON value. |
| `--slurpfile <name> <file>` | Set `$name` to the array of the values of a file. |
| `--rawfile <name> <file>` | Set `$name` to the text of a file. |
| `--args` | Remaining arguments are string positional arguments. |
| `--jsonargs` | Remaining arguments are JSON positional arguments. |
| `-e`, `--exit-status` | Set the exit code from the last output. |
| `-b`, `--binary` | Accepted for jq parity. No effect. |
| `-V`, `--version` | Show the version of jq that the verb matches. |
| `--build-configuration` | Show the build configuration. |
| `--run-tests [file]` | Run the tests of a jq test file. Standard input when no file is named. A failure case passes when the filter fails validation. |
| `--skip <n>` | Skip the first `n` tests. |
| `--take <n>` | Run `n` tests after the skipped ones. |
| `-h`, `--help` | Show this help. |

## Behavior and Edge Cases

- **Options:** Options go anywhere, and short flags combine, as in `-nr`. `--` ends the options.
- **Inputs:** Files and standard input are read in order. A file that cannot be opened is reported, and the other files are read. The exit code is 2.
- **Parse errors:** A parse error ends the input. The outputs of the values before it are kept. The exit code is 5.
- **Runtime errors:** A runtime error is reported on standard error, and the run goes on with the next value. The exit code is 5.
- **Exit codes:** 0 success. 1 with `-e` when the last output is false or null. 2 for a usage error or a missing file. 3 for a compile error. 4 with `-e` when there is no output. 5 for a parse error, a runtime error, or `halt_error`.
- **Variables:** `$ENV`, `$ARGS`, `$__loc__`, and the names of `--arg`, `--argjson`, `--slurpfile` and `--rawfile` are set.
- **Colour:** `-C` colours the output. Without it, colour is used only on a terminal, unless `NO_COLOR` is set or `-M` is given. `JQ_COLORS` sets the colours.
- **Standard error:** Each diagnostic keeps the `saw: ` prefix.
- **In place:** `saw json` never edits a file. Write the result with `saw write`.
- **Comments:** JSON with comments is an error.
- **Run tests:** Each test is a program line, an input line, and one expected result for each following line, until a blank line. A line that starts with `#` is ignored. A block that starts with `%%FAIL` expects a compile error with the lines that follow. The report is on standard output. The exit code is 1 when a test fails.
