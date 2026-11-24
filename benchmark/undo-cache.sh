#!/usr/bin/env bash

case "$(uname -s)" in
    MINGW*|MSYS*) sawCache="$APPDATA/saw" ;;
    *)            sawCache="$HOME/Library/Application Support/saw" ;;
esac

sawCacheFolders=(undo pending blobs)

restoreUndoCache()
{
    local folder

    for folder in "${sawCacheFolders[@]}"; do
        rm -rf "${sawCache:?}/$folder"
        if [ -d "$sawCache/$folder-benchmark-$$" ]; then
            mv "$sawCache/$folder-benchmark-$$" "$sawCache/$folder"
        fi
    done
}

for folder in "${sawCacheFolders[@]}"; do
    if [ -d "$sawCache/$folder" ]; then
        mv "$sawCache/$folder" "$sawCache/$folder-benchmark-$$" || exit 1
    fi
done

trap restoreUndoCache EXIT
trap 'exit 130' INT
trap 'exit 143' TERM HUP
