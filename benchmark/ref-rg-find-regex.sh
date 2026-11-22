#!/usr/bin/env bash

LC_ALL=C rg -n --no-ignore "class +JUCE_API" /tmp/saw-bench-corpus > /dev/null
exitCode=$?

: > "$(dirname "$1")/ref-rg-find-regex-samples.md"
exit $exitCode
