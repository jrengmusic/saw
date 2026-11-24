#!/usr/bin/env bash
set -euo pipefail

corpusDir="/tmp/saw-bench-corpus"
expectFile="/tmp/saw-bench-corpus.expect"
smallFile="/tmp/saw-bench-small.txt"
smallSourceFile="$corpusDir/juce_core/juce_core.h"
smallLineCount=200

if [ -d "$corpusDir" ] && [ -s "$expectFile" ] && [ -s "$smallFile" ]; then
    exit 0
fi

rm -rf "$corpusDir" "$expectFile" "$smallFile"

sourceDir="$(cd "$(dirname "$0")/../../../JUCE/modules" && pwd)"
cp -R "$sourceDir" "$corpusDir"

find "$corpusDir" -type f \( \
    -iname '*.png' -o -iname '*.ico' -o -iname '*.icns' -o \
    -iname '*.ttf' -o -iname '*.otf' -o -iname '*.wav' -o \
    -iname '*.aif' -o -iname '*.mp3' -o -iname '*.ogg' -o \
    -iname '*.flac' -o -iname '*.jpg' -o -iname '*.jpeg' -o \
    -iname '*.gif' -o -iname '*.pdf' -o -iname '*.zip' -o \
    -iname '*.bin' -o -iname '*.dat' -o -iname '*.exe' -o \
    -iname '*.dll' -o -iname '*.so' -o -iname '*.dylib' -o \
    -iname '*.a' -o -iname '*.o' \
\) -delete

summaryLine="$("$(command -v saw)" replace "JUCE_API" "SAW_API" "$corpusDir" | tail -n 1)"
expectCount="$(printf '%s\n' "$summaryLine" | sed -n 's/.*--expect \([0-9][0-9]*\).*/\1/p')"

case "$expectCount" in
    ''|*[!0-9]*)
        printf 'prepare-corpus: failed to extract expect count from: %s\n' "$summaryLine" >&2
        rm -rf "$corpusDir" "$expectFile"
        exit 1
        ;;
esac

printf '%s\n' "$expectCount" > "$expectFile"

head -n "$smallLineCount" "$smallSourceFile" > "$smallFile"
