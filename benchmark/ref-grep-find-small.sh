#!/usr/bin/env bash

for ((iteration = 0; iteration < 50; iteration++))
do
    LC_ALL=C grep -n JUCE /tmp/saw-bench-small.txt > /dev/null
    exitCode=$?
    if [ "$exitCode" -ne 0 ]; then break; fi
done

: > "$(dirname "$1")/ref-grep-find-small-samples.md"
exit $exitCode
