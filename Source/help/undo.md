# saw undo

Inspect and safely reverse applied SAW transactions from the 32-entry rolling undo cache.

## Invocation

```
saw undo <tx-id> [options]
saw undo --list
```

## Options

| Option | Effect |
| :--- | :--- |
| `--list` | Print stored transactions (newest first) as `<tx-id>\t<timestamp>\t<files> file(s)\t<command>` and exit `0`. |
| `--apply` | Execute the restore on disk. Without `--apply`, SAW prints the reverse unified diff only. |
| `--force` | Skip post-edit hash verification on `--apply` to overwrite newer disk edits. |

## Behavior and Edge Cases

- **Missing `<tx-id>`:** Without `--list`, SAW prints `saw: undo requires <tx-id>` to stderr and exits `2`. The cache is shared by every folder, so SAW has no default transaction.
- **No transactions stored:** With `--list`, SAW prints nothing and exits `0`.
- **Dry-run (default):** SAW verifies each file's SHA-256 matches `postHash`. SAW prints the reverse unified diff (`current -> original`) and `saw: dry-run undo (<tx-id>) — <F> file(s); nothing written. Pass --apply to restore.`.
- **Apply (`--apply`):** SAW verifies each file's SHA-256 matches `postHash` (unless you pass `--force`). SAW restores the original bytes, keeps the current mode of each file, deletes the record and the blobs, and prints `saw: undone <tx-id> — restored <F> file(s)`.
- **Partial write failure:** If restoring file `k + 1` fails, SAW reverts files `0..k` to their pre-undo bytes and exits `2`.
- **Rollback failure:** If the rollback of a failed restore also fails, SAW keeps the backup, prints `saw: undo failed for <path>: <reason>; rollback failed for <paths>; backup kept in <folder>`, and exits `2`.
- **File modified after `--apply` (hash drift):** SAW refuses restoration before writing any file and exits `2`.
- **Target file deleted after `--apply`:** Default check fails with hash drift (`<path> missing since apply`). With `--force`, SAW creates the file again with its original bytes and recorded POSIX execute bit.
- **Corrupt transaction with `--list`:** SAW prints its error, prints the other transactions, and exits `2`.
- **Parallel commands:** No lock exists. One `undo --apply` run owns a transaction. A second run of the same id exits `2`.
- **Pending transaction:** With `--apply`, a transaction that another process has not completed exits `2` with `saw: transaction <tx-id> is pending (pass --force to restore it)`. `--force` restores it. A dry-run reads it.

## Exit Codes and Errors

| Code | Meaning |
| :--- | :--- |
| `0` | `--list`, dry-run preview, or `--apply` restore succeeded. |
| `2` | Missing `<tx-id>` (without `--list`), unknown `<tx-id>`, corrupt transaction, hash drift, rollback failure, or write failure. |

| Condition | Stderr Message | Exit |
| :--- | :--- | :--- |
| Missing `<tx-id>` | `saw: undo requires <tx-id>` | `2` |
| Unknown `<tx-id>` | `saw: unknown transaction: <tx-id>` | `2` |
| Corrupt record or missing blob | `saw: corrupt transaction <tx-id>: <detail>` | `2` |
| `--apply` of a pending `<tx-id>` without `--force` | `saw: transaction <tx-id> is pending (pass --force to restore it)` | `2` |
| File drifted since `--apply` | `saw: cannot undo <tx-id>: <path> changed since apply (pass --force to overwrite)` | `2` |
| Restore write failure | `saw: undo failed for <path>: <reason>; rolled back` | `2` |
| Rollback failure | `saw: undo failed for <path>: <reason>; rollback failed for <paths>; backup kept in <folder>` | `2` |

## Examples

```
saw undo --list
saw undo 20261122T103000-a3f9c1
saw undo 20261122T103000-a3f9c1 --apply
saw undo 20261122T103000-a3f9c1 --apply --force
```
