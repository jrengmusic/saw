#!/usr/bin/env bash

LC_ALL=C rg -n --no-ignore -i "juce_api" /tmp/saw-bench-corpus > /dev/null
exitCode=$?

: > "$(dirname "$1")/ref-rg-find-ignore-case-samples.md"
exit $exitCode
