#!/usr/bin/env bash

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"


echo "$SCRIPT_DIR"

for name in "ftr.h" "ftr.c"; do
    curl https://raw.githubusercontent.com/nickwanninger/ftr/refs/heads/main/src/${name} > "$SCRIPT_DIR/${name}"
done
