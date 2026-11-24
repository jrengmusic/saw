#!/usr/bin/env bash

"$(command -v saw)" find --no-ignore -C 2 JUCE_API /tmp/saw-bench-corpus > /dev/null
exitCode=$?

: > "$(dirname "$1")/saw-find-context-samples.md"
exit $exitCode
