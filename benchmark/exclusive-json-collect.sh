#!/usr/bin/env bash

"$(command -v saw)" json -n -c '[range(0; 32000)]' > /dev/null
exitCode=$?

: > "$(dirname "$1")/exclusive-json-collect-samples.md"
exit $exitCode
