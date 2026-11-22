# saw

## Identity

- **Name:** saw (SAW: Safe Atomic Writer)
- **Root:** `/Users/jreng/Documents/Poems/dev/saw`

## Purpose

SAW is a planned command-line tool, written in C++17 with JUCE and JAM. It replaces `sed`, `grep`, and file-oriented `awk`.

Every mutating command previews its changes by default. A write needs `--apply --expect N`. `saw undo` reverses an applied transaction.

`SPEC.md` holds the behavior. This folder holds no source code yet. It holds the specification, the implementation plans, and the CAROL configuration.

## Contents

- `SPEC.md`: the specification of saw v0.0.1.
- `PLAN-saw-0-tracker.md` to `PLAN-saw-9-benchmark.md`: the tracker and the nine implementation parts. Every part shows status `pending`.
- `carol/`: the CAROL contract files and the sprint log.
- `.claude/`: agents, commands, and settings for Claude Code.

## Key documents

| Document | Purpose |
|---|---|
| `SPEC.md` | Behavior, commands, exit codes, and undo model of saw. |
| `PLAN-saw-0-tracker.md` | Nine-part tracker, source layout, and framework inventory. |
| `PLAN-saw-1-regexp.md` to `PLAN-saw-9-benchmark.md` | Execution plan for each part. |
| `AGENTS.md` | CAROL project instructions. |
| `carol/SPRINT-LOG.md` | Sprint log. It holds no sprint entry yet. |
| `carol/config.yml` | CAROL configuration. |
| `carol/CODING.md`, `carol/MANIFESTO.md`, `carol/NAMES.md` | Contract for C++ code. |
