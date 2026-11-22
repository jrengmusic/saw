#!/usr/bin/env bash

targetFile="/tmp/saw-bench-corpus/juce_javascript/choc/javascript/choc_javascript_QuickJS.h"

"$(command -v saw)" lines 10000:50000 "$targetFile" > /dev/null
exitCode=$?

: > "$(dirname "$1")/saw-lines-samples.md"
exit $exitCode
