# PLAN: saw — Part 9: scout Benchmark Suite

**RFC:** none — objective from ARCHITECT prompt
**Date:** 2026-11-22
**BLESSED Compliance:** verified
**Language Constraints:** `scout` markdown manifests and bash scripts (`SPEC.md` §2, §10.3)

## Overview

This part implements the `scout` benchmark manifests and workload scripts in `~/Documents/Poems/dev/saw/benchmark/` covering all five workload pairs in `SPEC.md` §10.3 (`saw` vs `grep`, `rg`, `sed`, and `awk` over a copy of `~/Documents/Poems/JUCE/modules`).

## Language / Framework Constraints

- Every manifest uses the `scout` `## run` table (`epochs = 11`, `command`, `samples`, `output`, `baseline`) and `## toolchain` table (`command`, `flag`) (`SPEC.md` §10.3, `A27`, `P13`, `scout/benchmark/mcp.md:1-21`).
- Every workload script writes an empty samples file: `: > "$(dirname "$1")/<name>-samples.md"` (`scout/benchmark/mcp.sh:8`).
- Corpus is a copy of `~/Documents/Poems/JUCE/modules` copied to `/tmp/saw-bench-corpus` (`SPEC.md` §1.2, §10.3).
- Each mutating epoch (`replace`) copies `/tmp/saw-bench-corpus` to a fresh temporary directory before running the edit, and reads the expected replacement count from `/tmp/saw-bench-corpus.expect` written by `prepare-corpus.sh` (zero hardcoded magic counts).
- Benchmark runs use `carol_scout` MCP delegated via `@Pathfinder` (`CAROL.md` Profiling, `profile` skill).

## Framework Inventory

Full tables live in `PLAN-saw-0-tracker.md` §Framework Inventory. This part uses:

- **API:** A27 (`scout` manifest `## run` and `## toolchain`, `scout/benchmark/mcp.md:1-21`).
- **Pattern:** P13 (`scout/benchmark/mcp.md:1-21`, `scout/benchmark/mcp.sh:1-9`).
- **Names:** N6 (`run`, `prepare`).
- **Synthesis:** S9.
- **Absence:** none.

## Validation Gate

Each step is validated by COUNSELOR before proceeding to the next — against `MANIFESTO.md` (BLESSED), `NAMES.md`, `CODING.md`, and the locked PLAN decisions. @Auditor runs ONCE, after the final step — never per step.

## Steps

### Step 1: Author corpus preparation script and read-only benchmark scripts in `benchmark/`
**Scope:** `~/Documents/Poems/dev/saw/benchmark/prepare-corpus.sh`, `~/Documents/Poems/dev/saw/benchmark/{ref-grep-find-literal,ref-rg-find-literal,saw-find-literal,ref-grep-find-regex,ref-rg-find-regex,saw-find-regex,ref-sed-lines,ref-awk-lines,saw-lines,ref-awk-fields,saw-fields}.sh`
**Action:** Create executable (`chmod +x`) bash scripts in `~/Documents/Poems/dev/saw/benchmark/`:
1. `benchmark/prepare-corpus.sh`:
   - If `/tmp/saw-bench-corpus` and `/tmp/saw-bench-corpus.expect` already exist, exit `0`.
   - Otherwise copy `"$(cd "$(dirname "$0")/../../../JUCE/modules" && pwd)"` into `/tmp/saw-bench-corpus`, remove binary assets (`.png`, `.ico`), and compute the exact literal occurrence count of `"JUCE_API"` in `/tmp/saw-bench-corpus` via `"$(command -v saw)" replace "JUCE_API" "SAW_API" /tmp/saw-bench-corpus | tail -n 1` (extracting `<E>` from `Pass --apply --expect <E>`) into `/tmp/saw-bench-corpus.expect`.
2. Literal search (`"JUCE_API"` over `/tmp/saw-bench-corpus`): `ref-grep-find-literal.sh` (`grep -rn`), `ref-rg-find-literal.sh` (`rg -n --no-ignore -F`), `saw-find-literal.sh` (`saw find --no-ignore`).
3. Regex search (`"JUCE_API +class"` over `/tmp/saw-bench-corpus`): `ref-grep-find-regex.sh` (`grep -rnE`), `ref-rg-find-regex.sh` (`rg -n --no-ignore`), `saw-find-regex.sh` (`saw find --no-ignore -r`).
4. Line-range (`10000:50000` of `choc_javascript_QuickJS.h`): `ref-sed-lines.sh`, `ref-awk-lines.sh`, `saw-lines.sh`.
5. Fields (`1,-1 -d ";"` on `choc_javascript_QuickJS.h`): `ref-awk-fields.sh`, `saw-fields.sh`.
Each script writes `: > "$(dirname "$1")/<name>-samples.md"` and exits with the workload's exit code.
**Inventory:** A27, P13.
**Validation:** COUNSELOR reads each script and checks: no contradictory copy logic, empty `*-samples.md` written, exact tool exit code preserved.

### Step 2: Author mutating `replace` benchmark scripts in `benchmark/`
**Scope:** `~/Documents/Poems/dev/saw/benchmark/ref-sed-replace.sh`, `~/Documents/Poems/dev/saw/benchmark/saw-replace.sh`
**Action:** Create executable (`chmod +x`) bash scripts (`SPEC.md` §10.3 row 3):
1. `benchmark/ref-sed-replace.sh`: copies `/tmp/saw-bench-corpus` into `mktemp -d`, runs portable `LC_ALL=C sed 's/JUCE_API/SAW_API/g' "$file" > "$file.tmp" && mv "$file.tmp" "$file"` across all files, removes temp dir, writes `ref-sed-replace-samples.md`, exits with `exitCode`.
2. `benchmark/saw-replace.sh`: copies `/tmp/saw-bench-corpus` into `mktemp -d`, reads `expectCount="$(cat /tmp/saw-bench-corpus.expect)"`, runs `"$(command -v saw)" replace "JUCE_API" "SAW_API" "$work" --apply --expect "$expectCount" > /dev/null` (no unlisted `--no-ignore`), removes temp dir, writes `saw-replace-samples.md`, exits with `exitCode`.
**Inventory:** A27, P13.
**Validation:** COUNSELOR checks both scripts perform identical per-epoch copy, replacement, and cleanup with zero hardcoded magic counts.

### Step 3: Author `scout` benchmark manifests and run via `scout`
**Scope:** `~/Documents/Poems/dev/saw/benchmark/{ref-grep-find-literal,ref-rg-find-literal,saw-find-literal-vs-grep,saw-find-literal-vs-rg,ref-grep-find-regex,ref-rg-find-regex,saw-find-regex-vs-grep,saw-find-regex-vs-rg,ref-sed-replace,saw-replace-vs-sed,ref-sed-lines,ref-awk-lines,saw-lines-vs-sed,saw-lines-vs-awk,ref-awk-fields,saw-fields-vs-awk}.md`
**Action:**
1. Write each `ref-*.md` manifest (`epochs = 11`, `command`, `samples`, `output`, `## toolchain` row `bash | prepare-corpus.sh`).
2. Write each `saw-*.md` manifest adding `baseline | string | <ref-name>-report.md`.
3. Delegate to `@Pathfinder` (`Model: "flash"`) to run reference manifests first and `saw` manifests second through `carol_scout` MCP.
**Inventory:** A27, P13.
**Validation:** COUNSELOR checks every `*-report.md` exists and each `saw-*-report.md` contains `change` and `probability` columns (`SPEC.md` §9, §10.3).

## BLESSED Alignment

- **B — Bound:** Per-epoch temp directories are created with `mktemp -d` and removed before exit.
- **L — Lean:** One declarative `scout` manifest and short script per benchmark.
- **E — Explicit:** Zero hardcoded magic counts; exact reference `baseline` declared per manifest.
- **S — SSOT:** `scout` is the single benchmark engine; `/tmp/saw-bench-corpus.expect` is the single expected count.
- **S — Stateless:** Each epoch runs as an independent child process over an identical corpus.
- **E — Encapsulation:** Corpus setup lives in `prepare-corpus.sh` via `## toolchain`.
- **D — Deterministic:** `LC_ALL=C`, 11 epochs, identical corpus files.

## Risks / Open Questions

None.
