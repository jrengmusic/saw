# PLAN: saw — Part 3: Deterministic Neovim Lua Test Runner

**RFC:** none — objective from ARCHITECT prompt
**Date:** 2026-11-22
**BLESSED Compliance:** verified
**Language Constraints:** Lua 5.1 on Neovim 0.10+ (`SPEC.md` §2, §10)

## Overview

This part implements the deterministic test harness specified in `SPEC.md` §10.1 and §10.2: `test/run.lua`, `test/cases.md`, undo cache isolation, baseline comparison mode, reference tool comparison mode, and the initial CLI error cases for Part 2.

## Language / Framework Constraints

- Invoked as `nvim --headless -l test/run.lua <saw-binary>` (`SPEC.md` §10.1).
- Process execution uses `vim.system` with `text = false` and `stdin` always set to a Lua string (`SPEC.md` §10.1, `A26`).
- Byte comparisons use Lua 8-bit clean string `==` (`SPEC.md` §10.1).
- Undo isolation moves `<app-data>/saw/undo` to `<app-data>/saw/undo-runner-<pid>` before running cases and restores it in all exit paths, including Lua errors (`SPEC.md` §1.2, §10.1).
- Reference processes run with `clear_env = true` and `env = { LC_ALL = "C", PATH = ..., HOME = ... }` (`SPEC.md` §10.2).

## Framework Inventory

Full tables live in `PLAN-saw-0-tracker.md` §Framework Inventory. This part uses:

- **API:** A26 (`vim.system`, `vim.uv.fs_mkdtemp`, `fs_realpath`, `fs_copyfile`, `fs_scandir`, `fs_scandir_next`, `fs_stat`, `fs_chmod`, `fs_readlink`, `fs_symlink`, `fs_rename`, `os_getpid`, `os_tmpdir`, `vim.fs.joinpath`, `vim.fn.delete`, `io.open`, `os.exit`).
- **Pattern:** P12 (`ling/test/mcp.lua:1-8,105-145`).
- **Names:** N6 (`run`, `get`, `is`, `read`, `copy`).
- **Synthesis:** S3, S9.
- **Absence:** X14, X15.

## Validation Gate

Each step is validated by COUNSELOR before proceeding to the next — against `MANIFESTO.md` (BLESSED), `NAMES.md`, `CODING.md`, and the locked PLAN decisions. @Auditor runs ONCE, after the final step — never per step.

## Steps

### Step 1: Implement `test/run.lua`
**Scope:** `~/Documents/Poems/dev/saw/test/run.lua`
**Action:** Create `test/run.lua` implementing `SPEC.md` §10.1 and §10.2 in full:

1. **Arguments and paths:**
   - Read `local sawBinary = arg[1]` and `assert (sawBinary ~= nil and #sawBinary > 0, "usage: nvim --headless -l test/run.lua <saw-binary>")`.
   - Resolve `sawBinary = vim.fn.fnamemodify (sawBinary, ":p")` and `local testDir = vim.fn.fnamemodify (arg[0], ":p:h")`.
   - Set `casesPath`, `fixtureRoot`, `stdinRoot`, `baselineRoot` via `vim.fs.joinpath`.

2. **File and tree helpers:**
   - `readBinaryFile (path)`: open `io.open (path, "rb")`; return `nil` if absent, otherwise `file:read ("*a")` and close.
   - `readFileOrEmpty (path)`: return `readBinaryFile (path) or ""`.
   - `copyTree (sourceDir, targetDir)`: recurse directories via `vim.uv.fs_scandir` / `fs_scandir_next`; copy files with `fs_copyfile` + `fs_chmod (dstPath, stat.mode)`; recreate symlinks with `fs_readlink` + `fs_symlink`.
   - `listRelativeEntries (rootDir)`: walk `rootDir` recursively; collect every regular file (`type = "file"`) and symlink (`type = "link"`) with its `/` relative path; sort ascending by path. When comparing two trees, verify identical sorted entry lists, identical entry types, identical `fs_readlink` targets for `"link"`, and identical `readBinaryFile` bytes for `"file"`.
   - `makeTempDirectory()`: `fs_mkdtemp (vim.fs.joinpath (vim.uv.os_tmpdir(), "saw-test-XXXXXX"))`, normalized with `vim.uv.fs_realpath`.

3. **Command tokenizer (`splitCommand (text)`):**
   - Split on ASCII space/tab; `"..."` keeps one argument with quotes removed; `""` produces empty string argument `""` (`SPEC.md` §10.2).

4. **Grid table parser (`parseCases (markdownText)`):**
   - Locate `## cases`, find `+` column offsets from the first `+---+` border, verify the 7 headers (`name`, `fixture`, `command`, `stdin`, `exit`, `reference`, `compare`), slice each `|` row by `+` offsets, trim whitespace, and group consecutive rows with the same `name` into one case.

5. **Output normalizer (`normalizeOutput (bytes, tempRoot)`):**
   - Escape Lua pattern metacharacters in `tempRoot` via `local escapedRoot = tempRoot:gsub ("([%(%)%.%%%+%-%*%?%[%]%^%$])", "%%%1")` before replacing `bytes = bytes:gsub (escapedRoot, "<root>")` (`SPEC.md` §10.2 baseline rule 4; hyphens in `saw-test-XXXXXX` must be escaped!).
   - Replace every transaction ID matching `%d%d%d%d%d%d%d%dT%d%d%d%d%d%d%-%x%x%x%x%x%x` with `<tx-id>`.

6. **Case executor (`runCase (caseEntry)`):**
   - Create `tempRoot` and copy initial fixture when non-empty.
   - For each step: copy step fixture overlay if different from step 1's fixture (supports external drift in multi-step undo cases); run `saw` via `vim.system (sawCmd, { stdin = stdinBytes, text = false, cwd = tempRoot }):wait()`; verify exit code.
   - Baseline mode (`step.reference == ""`): compare normalized stdout/stderr against `test/baseline/<name>/<step>/stdout` and `stderr`; after the last step, compare `tempRoot` against `test/baseline/<name>/files/` (or initial fixture when `files/` is absent).
   - Reference mode (`step.reference ~= ""`): copy fixture to a second `refRoot`, run reference command with `clear_env = true` and `env = { LC_ALL = "C", PATH = os.getenv ("PATH") or "", HOME = os.getenv ("HOME") or "" }`, compare `stdout` (and reference exit code) or `file:<path>` bytes.
   - Delete temporary directories in all branches.

7. **Undo isolation and top-level runner (`SPEC.md` §10.1):**
   - Compute `appDataSaw` (`%APPDATA%/saw` on Windows, `$HOME/Library/Application Support/saw` on macOS).
   - Move `undo` to `undo-runner-<pid>` when present; run all cases inside `xpcall`; always delete test `undo` and restore `undo-runner-<pid>` after `xpcall`; exit `0` when all pass or `1` otherwise.
**Inventory:** A26, P12, N6, S3, S9, X14, X15.
**Validation:** COUNSELOR reads `test/run.lua` and verifies: `escapedRoot` escapes Lua magic characters, `text = false`, `stdin` always string, symlink tree comparison works, undo isolation restores inside `xpcall` cleanup, and reference mode sets `clear_env = true` + `LC_ALL=C`.

### Step 2: Author `test/cases.md` and Part 2/3 baseline cases
**Scope:** `~/Documents/Poems/dev/saw/test/cases.md`, `~/Documents/Poems/dev/saw/test/baseline/{cli-unknown-command,cli-unknown-flag,cli-help-extra-flag,cli-version-extra-flag}/1/stderr`
**Action:**
1. Create `test/cases.md` with the `## cases` grid table and the four Part 2 CLI dispatch error cases (`cli-unknown-command`, `cli-unknown-flag`, `cli-help-extra-flag`, `cli-version-extra-flag`, all exit `2`).
2. Create the four expected stderr baseline files:
   - `cli-unknown-command/1/stderr`: `saw: unknown command: bogus\n`
   - `cli-unknown-flag/1/stderr`: `saw: unknown flag: --bogus\n`
   - `cli-help-extra-flag/1/stderr`: `saw: unknown flag: --bogus\n`
   - `cli-version-extra-flag/1/stderr`: `saw: unknown flag: --bogus\n`
**Inventory:** A26, P12.
**Validation:** ARCHITECT runs `nvim --headless -l test/run.lua ~/.local/bin/saw` and verifies all 4 cases print `PASS` and exit `0`.

## BLESSED Alignment

- **B — Bound:** Each case owns and deletes its temp directory; `xpcall` cleanup unconditionally restores the real undo directory.
- **L — Lean:** One Lua file and one markdown table drive all baseline and reference tests.
- **E — Explicit:** Byte-exact exit code, stdout, stderr, and tree checks.
- **S — SSOT:** `test/cases.md` is the single test manifest.
- **S — Stateless:** Every case runs in a fresh copy of its fixture.
- **E — Encapsulation:** Reference tools run isolated with `clear_env = true` and `LC_ALL=C`.
- **D — Deterministic:** `text = false`, escaped `<root>` and `<tx-id>` masking, and sorted tree checks.

## Risks / Open Questions

None.
