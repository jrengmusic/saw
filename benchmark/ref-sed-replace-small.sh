#!/usr/bin/env bash

work="$(mktemp -d)"
cp /tmp/saw-bench-small.txt "$work"
exitCode=$?
target="$work/saw-bench-small.txt"

if [ "$exitCode" -eq 0 ]; then
    for ((iteration = 0; iteration < 50; iteration++))
    do
        if (( iteration % 2 == 0 )); then fromTerm=JUCE; toTerm=JUCX; else fromTerm=JUCX; toTerm=JUCE; fi
        LC_ALL=C sed "s/${fromTerm}/${toTerm}/g" "$target" > "$target.tmp" && mv "$target.tmp" "$target"
        exitCode=$?
        if [ "$exitCode" -ne 0 ]; then break; fi
    done
fi

rm -rf "$work"

: > "$(dirname "$1")/ref-sed-replace-small-samples.md"
exit $exitCode
