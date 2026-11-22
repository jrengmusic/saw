#!/usr/bin/env bash

LC_ALL=C grep -rn -F "JUCE_API" /tmp/saw-bench-corpus > /dev/null
exitCode=$?

: > "$(dirname "$1")/ref-grep-find-literal-samples.md"
exit $exitCode
