#!/usr/bin/env bash

LC_ALL=C grep -rnE "class +JUCE_API" /tmp/saw-bench-corpus > /dev/null
exitCode=$?

: > "$(dirname "$1")/ref-grep-find-regex-samples.md"
exit $exitCode
