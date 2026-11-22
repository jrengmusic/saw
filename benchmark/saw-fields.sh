#!/usr/bin/env bash

targetFile="/tmp/saw-bench-corpus/juce_javascript/choc/javascript/choc_javascript_QuickJS.h"

"$(command -v saw)" fields 1,-1 -d ";" "$targetFile" > /dev/null
exitCode=$?

: > "$(dirname "$1")/saw-fields-samples.md"
exit $exitCode
