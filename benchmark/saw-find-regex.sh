#!/usr/bin/env bash

"$(command -v saw)" find --no-ignore -r "class +JUCE_API" /tmp/saw-bench-corpus > /dev/null
exitCode=$?

: > "$(dirname "$1")/saw-find-regex-samples.md"
exit $exitCode
