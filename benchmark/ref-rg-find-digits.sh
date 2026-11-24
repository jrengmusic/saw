#!/usr/bin/env bash

LC_ALL=C rg -n --no-ignore "[0-9]{3,}" /tmp/saw-bench-corpus > /dev/null
exitCode=$?

: > "$(dirname "$1")/ref-rg-find-digits-samples.md"
exit $exitCode
