#!/bin/bash


ALASKA="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )/../"


# make sure we enable alaska
source $ALASKA/enable


pushd $ALASKA/test/SPEC2017

make TAR=$1
make compile_rate
make bitcode

popd
