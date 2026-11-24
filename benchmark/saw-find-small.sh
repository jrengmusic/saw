#!/usr/bin/env bash

for ((iteration = 0; iteration < 50; iteration++))
do
    "$(command -v saw)" find JUCE /tmp/saw-bench-small.txt > /dev/null
    exitCode=$?
    if [ "$exitCode" -ne 0 ]; then break; fi
done

: > "$(dirname "$1")/saw-find-small-samples.md"
exit $exitCode
