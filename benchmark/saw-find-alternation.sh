#!/usr/bin/env bash

"$(command -v saw)" find --no-ignore -r "TODO|FIXME" /tmp/saw-bench-corpus > /dev/null
exitCode=$?

: > "$(dirname "$1")/saw-find-alternation-samples.md"
exit $exitCode
