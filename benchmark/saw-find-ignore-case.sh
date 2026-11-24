#!/usr/bin/env bash

"$(command -v saw)" find --no-ignore -i "juce_api" /tmp/saw-bench-corpus > /dev/null
exitCode=$?

: > "$(dirname "$1")/saw-find-ignore-case-samples.md"
exit $exitCode
