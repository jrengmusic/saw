#!/usr/bin/env bash

work="$(mktemp -d)"
cp -R /tmp/saw-bench-corpus/. "$work"
exitCode=$?

if [ "$exitCode" -eq 0 ]; then
    find "$work" -type f -print0 | while IFS= read -r -d '' file; do
        LC_ALL=C sed 's/JUCE_API/SAW_API/g' "$file" > "$file.tmp" && mv "$file.tmp" "$file" || exit 1
    done
    exitCode=$?
fi

rm -rf "$work"

: > "$(dirname "$1")/ref-sed-replace-samples.md"
exit $exitCode
