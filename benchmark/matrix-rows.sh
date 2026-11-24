#!/usr/bin/env bash

setName="$1"
benchmark="$(cd "$(dirname "$0")" && pwd)"
scratch="/tmp/saw-bench-matrix"
rows="$scratch/rows"
corpus="/tmp/saw-bench-corpus"
saw="$(command -v saw)"
server="$(command -v saw_ToolServer)"
table="$benchmark/matrix-$setName-table.md"
failures=0

cd "$scratch" || exit 1

getCount()
{
    case "$1" in
        1K|10K|50K|100K) printf '40' ;;
        1M) printf '20' ;;
        10M) printf '8' ;;
        100M) printf '4' ;;
        tree) printf '6' ;;
        *) printf '20' ;;
    esac
}

getServerCount()
{
    case "$1-$2" in
        findList-*|findQuiet-*|findFirst-*|linesHead-*|fieldsHead-*) printf '1000' ;;
        *-1M) printf '300' ;;
        *-10M) printf '60' ;;
        *-100M|*-tree) printf '20' ;;
        *) printf '1000' ;;
    esac
}

fail()
{
    printf 'FAIL %s\n' "$*"
    failures=$((failures + 1))
}

runRow()
{
    local caseName="$1" size="$2" tool="$3" count="$4" name="$1-$2-$3"
    local border="+----------+--------+--------------------------------------------------+"

    chmod +x "$rows/$name.sh"
    {
        printf '## run\n\n%s\n| %-8s | %-6s | %-48s |\n%s\n' "$border" key type value "${border//-/=}"
        printf "| %-8s | %-6s | %-48s |\n$border\n" epochs int 5 command string "$name.sh" samples string "$name-samples.md" output string "$name-report.md"
    } > "$rows/$name.md"

    if scout "$rows/$name.md"; then
        LC_ALL=C awk -v caseName="$caseName" -v size="$size" -v tool="$tool" -v count="$count" -F'|' \
            '$2 ~ /^ *epoch *$/ { printf "| %s | %s | %s | %.1f | %.2f |\n", caseName, size, tool, $4 / count / 1000, $5 }' \
            "$rows/$name-report.md" | tee -a "$table"
    else
        fail "$name scout"
    fi
}

writeLoop()
{
    local name="$1" count="$2" expected="$3"

    cat > "$rows/$name.sh" << SCRIPT
#!/usr/bin/env bash
cd $scratch
exitCode=0
for ((iteration = 0; iteration < $count; iteration++))
do
$4
    exitCode=\$?
    if [ "\$exitCode" -ne $expected ]; then break; fi
done
: > "$rows/$name-samples.md"
[ "\$exitCode" -eq $expected ]
SCRIPT
}

runCommand()
{
    local caseName="$1" size="$2" tool="$3" expected="$4"
    shift 4
    local count
    count="$(getCount "$size")"

    writeLoop "$caseName-$size-$tool" "$count" "$expected" "    $(printf '%q ' "$@") < /dev/null > /dev/null"
    runRow "$caseName" "$size" "$tool" "$count"
}

getRequest()
{
    local arguments="" argument

    for argument in "$@"; do
        argument="${argument//\\/\\\\}"
        argument="${argument//\"/\\\"}"
        arguments="$arguments${arguments:+,}\"$argument\""
    done

    printf '{"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":"run","arguments":{"directory":"%s","arguments":[%s]},"_meta":{"io.modelcontextprotocol/protocolVersion":"2026-07-28","io.modelcontextprotocol/clientCapabilities":{}}}}' "$scratch" "$arguments"
}

runServer()
{
    local caseName="$1" size="$2" expected="$3"
    shift 3
    local count name="$caseName-$size-server" request
    count="$(getServerCount "$caseName" "$size")"
    request="$(getRequest "$@")"

    awk -v count="$count" '{ for (id = 1; id <= count; id++) { line = $0; sub(/"id":1,/, "\"id\":" id ",", line); print line } }' <<< "$request" > "$rows/$name.jsonl"

    cat > "$rows/$name.sh" << SCRIPT
#!/usr/bin/env bash
work="\$(mktemp -d)"
mkfifo "\$work/hold"
exec 3<> "\$work/hold"
{ cat "$rows/$name.jsonl"; read -r line <&3; } | "$server" | { head -n $count > /dev/null; echo >&3; }
exitCode=\${PIPESTATUS[1]}
rm -rf "\$work"
: > "$rows/$name-samples.md"
exit \$exitCode
SCRIPT

    head -n 1 "$rows/$name.jsonl" > "$rows/$name-one.jsonl"
    local hold
    hold="$(mktemp -d)"
    mkfifo "$hold/hold"
    exec 3<> "$hold/hold"
    if ! { cat "$rows/$name-one.jsonl"; read -r line <&3; } | "$server" | { head -n 1 > "$rows/$name-reply.json"; echo >&3; } ||
       ! LC_ALL=C grep -q "\"exitCode\":$expected," "$rows/$name-reply.json"; then
        fail "$name reply"
    fi
    exec 3>&-
    rm -rf "$hold" "$rows/$name-reply.json"

    runRow "$caseName" "$size" server "$count"
}

checkEqual()
{
    local label="$1" expected="$2"
    shift 2

    "$@" < /dev/null > "$rows/reference-output" 2> /dev/null
    local code=$?

    if [ "$code" -ne "$expected" ] || ! cmp -s "$rows/saw-output" "$rows/reference-output"; then
        fail "$label differs"
    fi
}

checkEqualLines()
{
    local label="$1"
    shift

    "$@" < /dev/null 2> /dev/null | LC_ALL=C tr -d '\r' | LC_ALL=C sort > "$rows/reference-output"
    LC_ALL=C tr -d '\r' < "$rows/saw-output" | LC_ALL=C sort | cmp -s - "$rows/reference-output" || fail "$label differs"
}

searchNames="findLiteral findNoMatch findIgnoreCase findWord findRegex findDigits findBoundary findAlternation findContext findCount findList findQuiet findFirst"

setSearch()
{
    case "$1" in
        findLiteral)     sawArguments=(find juce_wchar);                 grepArguments=(-n juce_wchar);              rgArguments=(-n juce_wchar) ;;
        findNoMatch)     sawArguments=(find zzqqxxyy);                   grepArguments=(-n zzqqxxyy);                rgArguments=(-n zzqqxxyy) ;;
        findIgnoreCase)  sawArguments=(find -i juce_api);                grepArguments=(-n -i juce_api);             rgArguments=(-n -i juce_api) ;;
        findWord)        sawArguments=(find -w Component);               grepArguments=(-n -w Component);            rgArguments=(-n -w Component) ;;
        findRegex)       sawArguments=(find -r 'jassert \([a-z]+');      grepArguments=(-nE 'jassert \([a-z]+');     rgArguments=(-n 'jassert \([a-z]+') ;;
        findDigits)      sawArguments=(find -r '[0-9]{3,}');             grepArguments=(-nE '[0-9]{3,}');            rgArguments=(-n '[0-9]{3,}') ;;
        findBoundary)    sawArguments=(find -r '\bset[A-Z]\w+');         grepArguments=();                           rgArguments=(-n '\bset[A-Z]\w+') ;;
        findAlternation) sawArguments=(find -r 'TODO|FIXME|XXX');        grepArguments=(-nE 'TODO|FIXME|XXX');       rgArguments=(-n 'TODO|FIXME|XXX') ;;
        findContext)     sawArguments=(find -C 2 JUCE_API);              grepArguments=(-n -C 2 JUCE_API);           rgArguments=(-n -C 2 JUCE_API) ;;
        findCount)       sawArguments=(find -c juce);                    grepArguments=(-c juce);                    rgArguments=(-c --include-zero juce) ;;
        findList)        sawArguments=(find -l juce);                    grepArguments=(-l juce);                    rgArguments=(-l juce) ;;
        findQuiet)       sawArguments=(find -q juce);                    grepArguments=(-q juce);                    rgArguments=(-q juce) ;;
        findFirst)       sawArguments=(find --first juce);               grepArguments=(-n -m 1 juce);               rgArguments=(-n -m 1 juce) ;;
    esac
}

runSearches()
{
    local name size expected

    for name in $searchNames; do
        setSearch "$name"
        for size in "$@"; do
            "$saw" "${sawArguments[@]}" "$size.txt" < /dev/null > "$rows/saw-output" 2> /dev/null
            expected=$?

            if [ "${#grepArguments[@]}" -gt 0 ]; then
                checkEqual "$name $size grep" "$expected" grep "${grepArguments[@]}" "$size.txt"
                runCommand "$name" "$size" grep "$expected" grep "${grepArguments[@]}" "$size.txt"
            fi

            checkEqual "$name $size rg" "$expected" rg --no-config "${rgArguments[@]}" "$size.txt"
            runCommand "$name" "$size" rg "$expected" rg --no-config "${rgArguments[@]}" "$size.txt"
            runCommand "$name" "$size" saw "$expected" "$saw" "${sawArguments[@]}" "$size.txt"
            runServer "$name" "$size" "$expected" "${sawArguments[@]}" "$size.txt"
        done
    done
}

runLines()
{
    local size total first last

    for size in "$@"; do
        total="$(wc -l < "$size.txt")"
        first=$((total * 9 / 10 - 100))
        if [ "$first" -lt 1 ]; then first=1; fi
        last=$((first + 100))

        "$saw" lines "$first:$last" "$size.txt" > "$rows/saw-output"
        checkEqual "lines $size sed" 0 sed -n "$first,${last}p" "$size.txt"
        checkEqual "lines $size awk" 0 awk "NR>=$first && NR<=$last" "$size.txt"
        runCommand lines "$size" sed 0 sed -n "$first,${last}p" "$size.txt"
        runCommand lines "$size" awk 0 awk "NR>=$first && NR<=$last" "$size.txt"
        runCommand lines "$size" saw 0 "$saw" lines "$first:$last" "$size.txt"
        runServer lines "$size" 0 lines "$first:$last" "$size.txt"

        "$saw" lines 1:20 "$size.txt" > "$rows/saw-output"
        checkEqual "linesHead $size sed" 0 sed 20q "$size.txt"
        checkEqual "linesHead $size awk" 0 awk '{print} NR==20{exit}' "$size.txt"
        runCommand linesHead "$size" sed 0 sed 20q "$size.txt"
        runCommand linesHead "$size" awk 0 awk '{print} NR==20{exit}' "$size.txt"
        runCommand linesHead "$size" saw 0 "$saw" lines 1:20 "$size.txt"
        runServer linesHead "$size" 0 lines 1:20 "$size.txt"

        "$saw" fields 1,-1 -d , "$size.csv" > "$rows/saw-output"
        checkEqual "fields $size awk" 0 awk -F, '{print $1 "," $NF}' "$size.csv"
        runCommand fields "$size" awk 0 awk -F, '{print $1 "," $NF}' "$size.csv"
        runCommand fields "$size" saw 0 "$saw" fields 1,-1 -d , "$size.csv"
        runServer fields "$size" 0 fields 1,-1 -d , "$size.csv"

        "$saw" fields 1,-1 -d , --lines 1:20 "$size.csv" > "$rows/saw-output"
        checkEqual "fieldsHead $size awk" 0 awk -F, '{print $1 "," $NF} NR==20{exit}' "$size.csv"
        runCommand fieldsHead "$size" awk 0 awk -F, '{print $1 "," $NF} NR==20{exit}' "$size.csv"
        runCommand fieldsHead "$size" saw 0 "$saw" fields 1,-1 -d , --lines 1:20 "$size.csv"
        runServer fieldsHead "$size" 0 fields 1,-1 -d , --lines 1:20 "$size.csv"

        runCommand replacePreview "$size" sed 0 sed s/JUCE/JUCX/g "$size.txt"
        runCommand replacePreview "$size" saw 0 "$saw" replace JUCE JUCX "$size.txt"
        runServer replacePreview "$size" 0 replace JUCE JUCX "$size.txt"
    done
}

runInput()
{
    local caseName="$1" size="$2" tool="$3" input="$4"
    shift 4
    local count
    count="$(getCount "$size")"

    writeLoop "$caseName-$size-$tool" "$count" 0 "    $(printf '%q ' "$@") < $input > /dev/null"
    runRow "$caseName" "$size" "$tool" "$count"
}

runPipe()
{
    local caseName="$1" size="$2" tool="$3" input="$4"
    shift 4
    local count
    count="$(getCount "$size")"

    writeLoop "$caseName-$size-$tool" "$count" 0 "    cat $input | $(printf '%q ' "$@") > /dev/null"
    runRow "$caseName" "$size" "$tool" "$count"
}

runInputs()
{
    local size run

    for size in "$@"; do
        for run in Input Pipe; do
            "run$run" "find$run" "$size" grep "$size.txt" grep -n JUCE
            "run$run" "find$run" "$size" rg "$size.txt" rg --no-config -n JUCE
            "run$run" "find$run" "$size" saw "$size.txt" "$saw" find JUCE
            "run$run" "findQuiet$run" "$size" grep "$size.txt" grep -q JUCE
            "run$run" "findQuiet$run" "$size" rg "$size.txt" rg --no-config -q JUCE
            "run$run" "findQuiet$run" "$size" saw "$size.txt" "$saw" find -q JUCE
            "run$run" "linesHead$run" "$size" sed "$size.txt" sed 20q
            "run$run" "linesHead$run" "$size" saw "$size.txt" "$saw" lines 1:20
            "run$run" "lines$run" "$size" sed "$size.txt" sed -n 5,20p
            "run$run" "lines$run" "$size" saw "$size.txt" "$saw" lines 5:20
            "run$run" "fields$run" "$size" awk "$size.csv" awk -F, '{print $1 "," $NF}'
            "run$run" "fields$run" "$size" saw "$size.csv" "$saw" fields 1,-1 -d ,
            "run$run" "replace$run" "$size" sed "$size.txt" sed s/JUCE/JUCX/g
            "run$run" "replace$run" "$size" saw "$size.txt" "$saw" replace JUCE JUCX
        done
    done
}

if sed --version 2> /dev/null | grep -q GNU; then sedInPlace="sed -i"; else sedInPlace="sed -i ''"; fi

runApply()
{
    local size count matches

    for size in "$@"; do
        count="$(getCount "$size")"
        matches="$(LC_ALL=C grep -o JUCE "$size.txt" | wc -l | tr -d ' ')"
        cp "$size.txt" apply-saw.txt
        cp "$size.txt" apply-sed.txt

        writeLoop "replaceApply-$size-sed" "$count" 0 "    if (( iteration % 2 == 0 )); then from=JUCE; to=JUCX; else from=JUCX; to=JUCE; fi
    $sedInPlace \"s/\$from/\$to/g\" apply-sed.txt"
        runRow replaceApply "$size" sed "$count"
        writeLoop "replaceApply-$size-saw" "$count" 0 "    if (( iteration % 2 == 0 )); then from=JUCE; to=JUCX; else from=JUCX; to=JUCE; fi
    \"$saw\" replace \"\$from\" \"\$to\" apply-saw.txt --apply --expect $matches > /dev/null"
        runRow replaceApply "$size" saw "$count"

        cmp -s apply-saw.txt "$size.txt" || fail "replaceApply $size saw file"
        cmp -s apply-sed.txt "$size.txt" || fail "replaceApply $size sed file"
    done

    local batchCount=4
    writeLoop patchBatch-50edits-sed "$batchCount" 0 "    cp batch.txt batch-sed.txt
    for ((line = 1; line <= 50; line++)); do $sedInPlace \"\${line}s/.*/edited \$line/\" batch-sed.txt; done"
    runRow patchBatch 50edits sed "$batchCount"
    writeLoop patchBatch-50edits-saw "$batchCount" 0 "    cp batch.txt batch-saw.txt
    \"$saw\" patch --file batch.json --apply --expect 50 > /dev/null"
    runRow patchBatch 50edits saw "$batchCount"
    cmp -s batch-saw.txt batch-sed.txt || fail "patchBatch files differ"
}

runExcerpt()
{
    local total count index first sedCommands targets
    total="$(wc -l < 1M.txt)"

    for count in 1 2 10 100; do
        sedCommands=""
        targets=()
        for ((index = 1; index <= count; index++)); do
            first=$((total * index / (count + 1)))
            targets+=("1M.txt:$first-$((first + 20))")
            sedCommands="$sedCommands    sed -n '$first,$((first + 20))p' 1M.txt > /dev/null
"
        done

        writeLoop "excerpt-${count}targets-sed" "$(getCount excerpt)" 0 "$sedCommands"
        runRow excerpt "${count}targets" sed "$(getCount excerpt)"
        runCommand excerpt "${count}targets" saw 0 "$saw" excerpt "${targets[@]}"
        runServer excerpt "${count}targets" 0 excerpt "${targets[@]}"
    done
}

runTree()
{
    local name expected

    for name in $searchNames; do
        setSearch "$name"
        "$saw" "${sawArguments[@]}" "$corpus" < /dev/null > "$rows/saw-output" 2> /dev/null
        expected=$?

        if [ "$name" = findCount ]; then rgArguments=(-c juce); fi
        if [ "$name" != findQuiet ] && [ "$name" != findContext ]; then
            checkEqualLines "$name tree rg" rg --no-config "${rgArguments[@]}" "$corpus"
        fi

        if [ "${#grepArguments[@]}" -gt 0 ]; then
            runCommand "$name" tree grep "$expected" grep -rI "${grepArguments[@]}" "$corpus"
        fi

        runCommand "$name" tree rg "$expected" rg --no-config "${rgArguments[@]}" "$corpus"
        runCommand "$name" tree saw "$expected" "$saw" "${sawArguments[@]}" "$corpus"
    done

    runCommand findFiles tree rg 0 rg --no-config --files "$corpus"
    runCommand findFiles tree saw 0 "$saw" find --files "$corpus"
    runCommand findSorted tree rg 0 rg --no-config -n --sort path juce_wchar "$corpus"
    runCommand findSorted tree saw 0 "$saw" find juce_wchar "$corpus"
}

cat > "$table" << 'HEADER'
| case | size | tool | microseconds | errorPercent |
| :--- | :--- | :--- | ---: | ---: |
HEADER

case "$setName" in
    small) runSearches 1K 10K 50K 100K 1M; runLines 1K 10K 50K 100K 1M ;;
    large) runSearches 10M 100M ;;
    lines) runLines 10M 100M ;;
    tree)  runTree ;;
    input) runInputs 1K 10K 50K 100K 1M 10M 100M ;;
    apply) . "$benchmark/undo-cache.sh"; runApply 1K 10K 50K 100K 1M 10M; runExcerpt ;;
esac

if [ "$failures" -gt 0 ]; then exit 1; fi
