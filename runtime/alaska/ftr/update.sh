#!/usr/bin/env bash

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

rm *.zip
rm -rf ftr-main *.zip

wget https://github.com/nickwanninger/ftr/archive/refs/heads/main.zip

unzip main.zip

cp ftr-main/src/ftr.* .


rm -rf *.zip ftr-main
