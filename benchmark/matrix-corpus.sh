#!/usr/bin/env bash
set -euo pipefail

bash "$(dirname "$0")/prepare-corpus.sh"

corpus="/tmp/saw-bench-corpus"
scratch="/tmp/saw-bench-matrix"

if [ -f "$scratch/complete" ]; then
    exit 0
fi

rm -rf "$scratch"
mkdir -p "$scratch/rows"

whole="$scratch/whole"
once="$scratch/once"
largestSize=104857600

find "$corpus" -type f \( -name '*.h' -o -name '*.cpp' \) | LC_ALL=C sort | tr '\n' '\0' | xargs -0 cat | LC_ALL=C tr -d '\r\000' > "$once"

: > "$whole"
while [ "$(wc -c < "$whole")" -lt "$largestSize" ]; do
    cat "$once" >> "$whole"
done

while read -r size bytes; do
    head -c "$bytes" "$whole" | LC_ALL=C sed '$d' > "$scratch/$size.txt"
    awk -v limit="$bytes" 'BEGIN { while (length_ < limit) { number++; line = number ",alpha " number % 97 ",beta,gamma " number % 13; print line; length_ += length(line) + 1 } }' > "$scratch/$size.csv"
done << 'SIZES'
1K 1024
10K 10240
50K 51200
100K 102400
1M 1048576
10M 10485760
100M 104857600
SIZES

rm "$whole" "$once"

cp "$scratch/10K.txt" "$scratch/batch.txt"
awk 'BEGIN { printf "["; for (line = 1; line <= 50; line++) printf "%s{\"file\":\"batch-saw.txt\",\"startLine\":%d,\"endLine\":%d,\"text\":\"edited %d\"}", (line > 1 ? "," : ""), line, line, line; print "]" }' > "$scratch/batch.json"

: > "$scratch/complete"
