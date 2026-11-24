#!/usr/bin/env bash

LC_ALL=C rg -n --no-ignore -C 2 JUCE_API /tmp/saw-bench-corpus > /dev/null
exitCode=$?

: > "$(dirname "$1")/ref-rg-find-context-samples.md"
exit $exitCode
