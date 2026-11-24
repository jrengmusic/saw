#!/usr/bin/env bash

. "$(dirname "$0")/undo-cache.sh"

work="$(mktemp -d)"
cp /tmp/saw-bench-small.txt "$work"
exitCode=$?
target="$work/saw-bench-small.txt"

if [ "$exitCode" -eq 0 ]; then
    summaryLine="$("$(command -v saw)" replace JUCE JUCX "$target" | tail -n 1)"
    expectCount="$(printf '%s\n' "$summaryLine" | sed -n 's/.*--expect \([0-9][0-9]*\).*/\1/p')"

    case "$expectCount" in
        ''|*[!0-9]*)
            printf 'saw-replace-small: failed to extract expect count from: %s\n' "$summaryLine" >&2
            exitCode=1
            ;;
        *)
            for ((iteration = 0; iteration < 50; iteration++))
            do
                if (( iteration % 2 == 0 )); then fromTerm=JUCE; toTerm=JUCX; else fromTerm=JUCX; toTerm=JUCE; fi
                "$(command -v saw)" replace "$fromTerm" "$toTerm" "$target" --apply --expect "$expectCount" > /dev/null
                exitCode=$?
                if [ "$exitCode" -ne 0 ]; then break; fi
            done
            ;;
    esac
fi

rm -rf "$work"

: > "$(dirname "$1")/saw-replace-small-samples.md"
exit $exitCode
