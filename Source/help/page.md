# saw page

Print the bytes of files, or of standard input, to standard output. With the libraries of ling, render them as a page.

## Invocation

```
saw page [<path>...] [options]
```

## Options

| Option | Effect |
| :--- | :--- |
| `-A`, `--show-all` | Show each non-printable byte in the notation of `--nonprintable-notation`. |
| `--nonprintable-notation <name>` | Notation of the non-printable bytes: `unicode` (default) or `caret`. |
| `--binary=as-text` | Print a file that holds a NUL byte to a terminal. Without it, the content is not printed. |
| `-p`, `--plain` | Plain style, with no components. Given twice, no decoration at all. |
| `-f`, `--force-colorization` | Enable decorations and colour, even when piped. `-pp -f` keeps the colour and drops the decorations. |
| `--color <when>` | Colour output: `auto` (default, a terminal), `never`, or `always`. |
| `--theme <name>` | Style sheet of the colours: `<name>.css` in the theme folder, or in the folder of the page library (`gfx`, `ansi`). `dark` and `light` use `--theme-dark` and `--theme-light`. Without a theme file, SAW writes a warning to standard error and uses the default colours. |
| `--theme-dark <name>` | Theme used when `--theme=dark`. |
| `--theme-light <name>` | Theme used when `--theme=light`. |
| `--list-themes` | Print the names of the style sheets, and exit. |
| `-l`, `--language <name>` | Syntax language, by name or by extension. Without it, the language follows the file name. There is no first-line detection. |
| `-m`, `--map-syntax <glob:syntax>` | Map a glob pattern to a syntax name. The option may repeat. |
| `--ignored-suffix <suffix>` | Ignore a file name suffix for syntax detection. The option may repeat. |
| `-L`, `--list-languages` | Print the languages as `name:globs` lines, and exit. It prints nothing without the token library of ling. |
| `--italic-text <when>` | Parsed: `always` or `never` (default). The page command sets no italic text. |
| `-H`, `--highlight-line <range>` | Highlight the lines of a range with the line highlight colour. The option may repeat. |
| `-d`, `--diff` | Show only the lines that differ from the Git index, with the snip line between them. |
| `--diff-context <n>` | Context lines around each changed line with `--diff`. `0` by default. |
| `-r <range>`, `--line-range <range>` | Print only the lines of a range. The option may repeat. The result is the union of the ranges, in file order. |
| `--decorations <when>` | When to draw the components of `--style`: `auto` (default: a terminal), `never`, or `always`. |
| `--style <components>` | Comma-separated components: `changes`, `grid`, `rule`, `header`, `header-filename`, `header-filesize`, `numbers`, `snip`. Sets: `default`, `full`, `auto`, `plain`. `+` adds a component and `-` removes it. Without `--style`, the set is `default`. |
| `-n`, `--number` | Line numbers as the only component, when `--style` is absent. |
| `--tabs <n>` | Tab width in columns, `4` by default. `0` passes tabs through. Tabs expand only in the rendered lane. |
| `--wrap <mode>` | `auto` and `character` wrap a line at the width, by character. `never` does not wrap. `word` wraps at the last space before the width. |
| `-S`, `--chop-long-lines` | Never wrap a line. The same as `--wrap=never`. |
| `--terminal-width <n>` | Output width in columns. Without it, the width of the terminal, or `80`. `+n` and `-n` are offsets of the width. |
| `--file-name <name>` | File name shown in the header instead of the path. |
| `-s`, `--squeeze-blank` | Squeeze consecutive empty lines into one. The kept line keeps its line number. |
| `--squeeze-limit <n>` | Most consecutive empty lines kept when `-s` is given. `1` by default. |
| `--strip-ansi <when>` | Remove ANSI escape sequences: `always`, or `never` (default). `auto` does not strip. |
| `--sanitize <when>` | Replace terminal-active bytes and bidi or zero-width characters with U+FFFD: `auto`, `always`, or `never` (default). Escape sequences stay. |
| `-V`, `--version` | Print the version line and exit. |
| `--config-file` | Print the path of the configuration file, and exit. |
| `--config-dir` | Print the path of the configuration folder, and exit. |
| `--paging <when>` | `auto` (default): the page scrolls inside the terminal when it is higher than the terminal. `always`: the page always scrolls inside the terminal. `never`: the page does not scroll. A pipe never pages. |
| `-P` | No paging. The same as `--paging=never`. |
| `--pager <command>` | Parsed. No pager command starts. |
| `--generate-config-file` | Parsed. The page command writes no configuration file. |
| `--completion <shell>` | Parsed. The page command prints no completion. |
| `-u`, `--unbuffered` | Parsed. The page command reads its input in one pass. |
| `--set-terminal-title` | Parsed. The page command sets no terminal title. |

## Configuration

Options are read in this order, and a later source wins: the configuration file, then the environment, then the command line.

- The configuration file is `$BAT_CONFIG_PATH` when it is set, otherwise `config` in the configuration folder. Each line is one option. A line that starts with `#` is a comment. Quotes are removed, and a quoted value keeps its spaces.
- The configuration folder is `$BAT_CONFIG_DIR` when it is set, otherwise `$XDG_CONFIG_HOME/bat` when `XDG_CONFIG_HOME` is set, otherwise `~/.config/bat`.
- The theme folder is `themes` in the configuration folder. A style sheet is `<name>.css` in the theme folder or in the folder of the page library.
- `BAT_THEME`, `BAT_THEME_LIGHT`, `BAT_THEME_DARK`, `BAT_STYLE`, `BAT_PAGER`, `BAT_PAGING`, and `BAT_WIDTH` set `--theme`, `--theme-light`, `--theme-dark`, `--style`, `--pager`, `--paging`, and `--terminal-width`. `BAT_OPTS` holds options, separated by spaces, with quotes.

## Line Ranges

| Range | Lines |
| :--- | :--- |
| `N` | Line N. |
| `N:M` | Lines N to M, inclusive. |
| `N:` | Line N to the last line. |
| `:M` | First line to line M. |
| `-K:` | Last K lines. |
| `N:-K` | Line N-K through line N. |
| `:-K` | First line through line count minus K. |
| `N:+K` | Line N through line N+K. |
| `N::C` | Line N with C lines of context before and after it. |
| `N:M:C` | Lines N to M with C lines of context before and after them. |

Line 0 selects no line in a single-line range. As the start of a range, 0 starts at line 1. A range with its end before its start selects no line.

## Behavior and Edge Cases

- **Input source:** With no path, SAW reads standard input when standard input holds data. Without data, SAW reads the current folder, as `saw find` does.
- **Standard input:** `-` names standard input. It is read one time, at its place in the path list.
- **Bytes:** SAW prints the bytes of each file as they are. It adds no line ending. Without a library of ling, the output is the plain bytes, with exit `0` and no message. The MCP lane never loads a library. The rendered lane runs on the command line only, when a decoration, colour, or paging applies. It renders each file as one page: the header, the rows with their line numbers, the snip line between ranges that are not adjacent, and the footer. Tabs expand in the rendered lane only. Long lines wrap at the content width.
- **UTF-16:** Input with a UTF-16 byte order mark is decoded to UTF-8 when a decoration or a transform is on. Without them, the bytes pass as they are.
- **Colour:** Colours are 24-bit. Colour follows the style sheet named by `--theme`. A missing style sheet writes a warning to standard error and uses the default colours.
- **Pager:** The keys are those of the reader mode of the editor. `q`, Escape, and Ctrl+C end the page. No pager command starts.
- **Missing file:** SAW prints the other files. It writes the failure to standard error and exits `1`.
- **Folder:** SAW writes the failure `'<path>' is a directory.` and exits `1`.
- **Binary file on a terminal:** SAW writes one warning line to standard error and prints no content of that file. The exit code stays `0`. In a pipe, the content passes.
- **Show all:** `-A` changes the control bytes and the DEL byte to their notation. A line feed keeps its line end. A space shows as `·`. A tab fills to the next column that is a multiple of 4. A valid UTF-8 codepoint shows as `\u{hex}`. A byte that is not part of a valid UTF-8 sequence shows as `\xHH`.

## Exit Codes and Errors

| Code | Meaning |
| :--- | :--- |
| `0` | The content was printed, or a binary warning was written. |
| `1` | A file was not found, was a folder, or could not be read. A line range is invalid. |
| `2` | A value of `--binary`, `--nonprintable-notation`, `--decorations`, `--style`, `--wrap`, `--tabs`, or `--terminal-width` is invalid. |

| Condition | Stderr Message | Exit |
| :--- | :--- | :--- |
| Invalid line range | `saw: invalid line range: <range>` | `1` |
| Folder path | `saw: '<path>' is a directory.` | `1` |
| Invalid `--nonprintable-notation` value | `saw: invalid value for --nonprintable-notation: <value>` | `2` |
| Invalid `--binary` value | `saw: invalid value for --binary: <value>` | `2` |

## Examples

```
saw page src/Main.cpp
saw page -A --nonprintable-notation=caret data.bin
saw page -r 10:12 -r 40: src/Main.cpp
saw page --binary=as-text data.bin
```
