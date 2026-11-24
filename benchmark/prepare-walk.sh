#!/usr/bin/env bash
set -euo pipefail

corpusDir="/tmp/saw-bench-corpus"
walkDir="/tmp/saw-bench-walk"
copyCount=40

bash "$(dirname "$0")/prepare-corpus.sh"

if [ -d "$walkDir" ]; then
    exit 0
fi

stagingDir="$walkDir.staging"
rm -rf "$stagingDir"

for folder in "$corpusDir"/*/; do
    name="$(basename "$folder")"
    mkdir -p "$stagingDir/$name"

    for copy in $(seq 1 "$copyCount"); do
        cp -R "$corpusDir/$name" "$stagingDir/$name/copy$copy"
    done
done

mv "$stagingDir" "$walkDir"
