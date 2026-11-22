#!/usr/bin/env bash

work="$(mktemp -d)"
cp -R /tmp/saw-bench-corpus/. "$work"
exitCode=$?

if [ "$exitCode" -eq 0 ]; then
    expectCount="$(cat /tmp/saw-bench-corpus.expect)"
    "$(command -v saw)" replace "JUCE_API" "SAW_API" "$work" --apply --expect "$expectCount" > /dev/null
    exitCode=$?
fi

rm -rf "$work"

: > "$(dirname "$1")/saw-replace-samples.md"
exit $exitCode
