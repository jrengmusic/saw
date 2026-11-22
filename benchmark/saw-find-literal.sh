#!/usr/bin/env bash

"$(command -v saw)" find --no-ignore "JUCE_API" /tmp/saw-bench-corpus > /dev/null
exitCode=$?

: > "$(dirname "$1")/saw-find-literal-samples.md"
exit $exitCode
