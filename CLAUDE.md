# saw

## Identity

- **Name:** saw (SAW: Safe Atomic Writer)
- **Root:** `/Users/jreng/Documents/Poems/dev/saw`

## Purpose

SAW is a command-line tool, written in C++17 with JUCE and JAM. It replaces `sed`, `grep`, and file-oriented `awk`. It runs on macOS and Windows.

Every mutating command previews its changes by default. A write needs `--apply --expect N`. `saw undo <tx-id>` previews the reverse of an applied transaction, and `--apply` restores it.

`SPEC.md` holds the behavior. `Source/` holds the program.

## Contents

- `SPEC.md`: the specification of saw v0.0.1.
- `RFC-live-to-premise.md`, `PLAN-native-mcp.md`: the defects against the premise, and the plan that fixes them.
- `Source/`: the program, its help documents, and the generated headers (`Source/generated/`, written by cast).
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
| `PLAN-native-mcp.md` | Execution plan of the native MCP and the clean-up. |
| `AGENTS.md` | CAROL project instructions. |
| `carol/SPRINT-LOG.md` | Sprint log. |
| `carol/config.yml` | CAROL configuration. |
| `carol/CODING.md`, `carol/MANIFESTO.md`, `carol/NAMES.md` | Contract for C++ code. |
