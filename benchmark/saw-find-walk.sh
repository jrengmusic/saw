#!/usr/bin/env bash

"$(command -v saw)" find --no-ignore "JUCE_API" /tmp/saw-bench-walk > /dev/null
exitCode=$?

: > "$(dirname "$1")/saw-find-walk-samples.md"
exit $exitCode
