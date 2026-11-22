# saw undo

Inspect and safely reverse applied SAW transactions from the 32-entry rolling undo cache.

## Invocation

```
saw undo [tx-id] [options]
```

## Options

| Option | Effect |
| :--- | :--- |
| `--list` | Print stored transactions (newest first) as `<tx-id>\t<timestamp>\t<files> file(s)\t<command>` and exit `0`. |
| `--apply` | Execute the restore on disk. Without `--apply`, SAW prints the reverse unified diff only. |
| `--force` | Skip post-edit hash verification on `--apply` to overwrite newer disk edits. |

## Behavior and Edge Cases

- **Default target:** When you omit `<tx-id>`, SAW selects the latest active transaction.
- **Empty cache with `--list`:** SAW prints nothing and exits `0`.
- **Empty cache without `--list`:** SAW prints `saw: no undo transactions available` to stderr and exits `2`.
- **Dry-run (default):** SAW verifies each file's SHA-256 matches `postHash`. SAW prints the reverse unified diff (`current -> original`) and `saw: dry-run undo (<tx-id>) — <F> file(s); nothing written. Pass --apply to restore.`.
- **Apply (`--apply`):** SAW verifies each file's SHA-256 matches `postHash` (unless you pass `--force`). SAW restores original bytes and POSIX execute bits via `juce::TemporaryFile`, removes the transaction directory, and prints `saw: undone <tx-id> — restored <F> file(s)`.
- **Partial write failure:** If restoring file `k + 1` fails, SAW reverts files `0..k` to their pre-undo bytes and exits `2`.
- **File modified after `--apply` (hash drift):** SAW refuses restoration before writing any file and exits `2`.
- **Target file deleted after `--apply`:** Default check fails with hash drift (`<path> missing since apply`). With `--force`, SAW recreates the file with original bytes via `juce::TemporaryFile`.

## Exit Codes and Errors

| Code | Meaning |
| :--- | :--- |
| `0` | `--list`, dry-run preview, or `--apply` restore succeeded. |
| `2` | Empty cache (without `--list`), unknown `<tx-id>`, corrupt transaction, hash drift, or write failure. |

| Condition | Stderr Message | Exit |
| :--- | :--- | :--- |
| Undo cache empty | `saw: no undo transactions available` | `2` |
| Unknown `<tx-id>` | `saw: unknown transaction: <tx-id>` | `2` |
| Corrupt `meta.json` or missing blob | `saw: corrupt transaction <tx-id>: <detail>` | `2` |
| File drifted since `--apply` | `saw: cannot undo <tx-id>: <path> changed since apply (pass --force to overwrite)` | `2` |
| Restore write failure | `saw: undo failed for <path>: <reason>; rolled back` | `2` |

## Examples

```
saw undo --list
saw undo
saw undo --apply
saw undo 20261122T103000-a3f9c1 --apply
saw undo --apply --force
```
