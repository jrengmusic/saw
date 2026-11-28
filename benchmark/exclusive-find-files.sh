#!/usr/bin/env bash

"$(command -v saw)" find --files --no-ignore /tmp/saw-bench-walk > /dev/null
exitCode=$?

: > "$(dirname "$1")/exclusive-find-files-samples.md"
exit $exitCode
