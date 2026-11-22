#!/usr/bin/env bash

targetFile="/tmp/saw-bench-corpus/juce_javascript/choc/javascript/choc_javascript_QuickJS.h"

LC_ALL=C sed -n '10000,50000p' "$targetFile" > /dev/null
exitCode=$?

: > "$(dirname "$1")/ref-sed-lines-samples.md"
exit $exitCode
