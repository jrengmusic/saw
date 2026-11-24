# saw

## Identity

- **Name:** saw (SAW: Safe Atomic Writer)
- **Root:** `/Users/jreng/Documents/Poems/dev/saw`

## Purpose

SAW is a command-line tool, written in C++17 with JUCE and JAM. It replaces `sed`, `grep`, and file-oriented `awk`. It runs on macOS and Windows.

Every mutating command previews its changes by default. A write needs `--apply --expect N`. `saw undo <tx-id>` reverses an applied transaction.

`SPEC.md` holds the behavior. `Source/` holds the program.

## Contents

- `SPEC.md`: the specification of saw v0.0.1.
- `RFC-live-to-premise.md`, `PLAN-live-to-premise.md`: the defects against the premise, and the plan that fixes them.
- `Source/`: the program, its help documents, and the generated headers (`Source/generated/`, written by cast).
- `server/`: the `saw_ToolServer` target, an MCP server on stdio that runs saw command lines (SPEC.md §5.8).
- `help/HelpFrames.cpp`: the `saw_HelpFrames` target, which renders the help frames at build time.
- `cast/`, `project-info.md`: the vocabulary, texts, and build tables that cast reads.
- `test/`: the runner `run.lua`, the case table `cases.md`, fixtures, and baselines.
- `benchmark/`: the scout manifests, scripts, and reports.
- `carol/`: the CAROL contract files and the sprint log.
- `.claude/`: agents, commands, and settings for Claude Code.

## Key documents

| Document | Purpose |
|---|---|
| `SPEC.md` | Behavior, commands, exit codes, and undo model of saw. |
| `PLAN-live-to-premise.md` | Execution plan of the premise sprint, with its framework inventory. |
| `AGENTS.md` | CAROL project instructions. |
| `carol/SPRINT-LOG.md` | Sprint log. |
| `carol/config.yml` | CAROL configuration. |
| `carol/CODING.md`, `carol/MANIFESTO.md`, `carol/NAMES.md` | Contract for C++ code. |
