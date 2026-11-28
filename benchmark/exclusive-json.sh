#!/usr/bin/env bash

"$(command -v saw)" json . /tmp/saw-bench-large.json > /dev/null
exitCode=$?

: > "$(dirname "$1")/exclusive-json-samples.md"
exit $exitCode
