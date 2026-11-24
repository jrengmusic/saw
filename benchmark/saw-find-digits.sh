#!/usr/bin/env bash

"$(command -v saw)" find --no-ignore -r "[0-9]{3,}" /tmp/saw-bench-corpus > /dev/null
exitCode=$?

: > "$(dirname "$1")/saw-find-digits-samples.md"
exit $exitCode
