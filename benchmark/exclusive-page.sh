#!/usr/bin/env bash

"$(command -v saw)" page /tmp/saw-bench-large.txt > /dev/null
exitCode=$?

: > "$(dirname "$1")/exclusive-page-samples.md"
exit $exitCode
