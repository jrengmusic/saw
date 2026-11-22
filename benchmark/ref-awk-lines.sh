#!/usr/bin/env bash

targetFile="/tmp/saw-bench-corpus/juce_javascript/choc/javascript/choc_javascript_QuickJS.h"

LC_ALL=C awk 'NR>=10000 && NR<=50000' "$targetFile" > /dev/null
exitCode=$?

: > "$(dirname "$1")/ref-awk-lines-samples.md"
exit $exitCode
