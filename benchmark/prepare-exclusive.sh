#!/usr/bin/env bash
set -euo pipefail

corpusDir="/tmp/saw-bench-corpus"
largeTextFile="/tmp/saw-bench-large.txt"
largeJsonFile="/tmp/saw-bench-large.json"
saw="$(command -v saw)"

bash "$(dirname "$0")/prepare-walk.sh"

if [ ! -s "$largeTextFile" ]; then
    "$saw" find --files --no-ignore "$corpusDir" --include '*.cpp' | tr '\n' '\0' | xargs -0 "$saw" page > "$largeTextFile.staging"
    mv "$largeTextFile.staging" "$largeTextFile"
fi

if [ ! -s "$largeJsonFile" ]; then
    "$saw" json -n -c '[range(0; 200000) | {id: ., name: "item \(.)", tags: ["alpha", "beta"], value: (. * 3)}]' > "$largeJsonFile.staging"
    mv "$largeJsonFile.staging" "$largeJsonFile"
fi
