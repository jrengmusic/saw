#!/usr/bin/env bash

LC_ALL=C rg -n --no-ignore "TODO|FIXME" /tmp/saw-bench-corpus > /dev/null
exitCode=$?

: > "$(dirname "$1")/ref-rg-find-alternation-samples.md"
exit $exitCode
