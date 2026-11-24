#!/usr/bin/env bash

. "$(dirname "$0")/undo-cache.sh"

lineCount=50
repetitionCount=10
exitCode=0

for ((repetition = 0; repetition < repetitionCount; repetition++))
do
    work="$(mktemp -d)"
    cp /tmp/saw-bench-small.txt "$work"
    target="$work/saw-bench-small.txt"
    patchFile="$work/patch.json"

    {
        printf '[\n'
        for ((lineNumber = 1; lineNumber <= lineCount; lineNumber++))
        do
            separator=","
            if (( lineNumber == lineCount )); then separator=""; fi
            printf '    {"file": "%s", "startLine": %d, "endLine": %d, "text": "edited %d"}%s\n' "$target" "$lineNumber" "$lineNumber" "$lineNumber" "$separator"
        done
        printf ']\n'
    } > "$patchFile"

    "$(command -v saw)" patch --file "$patchFile" --apply --expect "$lineCount" > /dev/null
    exitCode=$?
    rm -rf "$work"
    if [ "$exitCode" -ne 0 ]; then break; fi
done

: > "$(dirname "$1")/saw-patch-batch-samples.md"
exit $exitCode
