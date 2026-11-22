#!/usr/bin/env bash

targetFile="/tmp/saw-bench-corpus/juce_javascript/choc/javascript/choc_javascript_QuickJS.h"

LC_ALL=C awk -F';' '{print $1 ";" $NF}' "$targetFile" > /dev/null
exitCode=$?

: > "$(dirname "$1")/ref-awk-fields-samples.md"
exit $exitCode
