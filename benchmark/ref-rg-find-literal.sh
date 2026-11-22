#!/usr/bin/env bash

LC_ALL=C rg -n --no-ignore -F "JUCE_API" /tmp/saw-bench-corpus > /dev/null
exitCode=$?

: > "$(dirname "$1")/ref-rg-find-literal-samples.md"
exit $exitCode
