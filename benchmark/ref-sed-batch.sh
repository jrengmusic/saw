#!/usr/bin/env bash

lineCount=50
repetitionCount=10
exitCode=0

for ((repetition = 0; repetition < repetitionCount; repetition++))
do
    work="$(mktemp -d)"
    cp /tmp/saw-bench-small.txt "$work"
    target="$work/saw-bench-small.txt"

    for ((lineNumber = 1; lineNumber <= lineCount; lineNumber++))
    do
        LC_ALL=C sed "${lineNumber}s/.*/edited ${lineNumber}/" "$target" > "$target.tmp" && mv "$target.tmp" "$target"
        exitCode=$?
        if [ "$exitCode" -ne 0 ]; then break; fi
    done

    rm -rf "$work"
    if [ "$exitCode" -ne 0 ]; then break; fi
done

: > "$(dirname "$1")/ref-sed-batch-samples.md"
exit $exitCode
