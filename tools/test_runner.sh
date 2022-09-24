#!/bin/bash

set -e

ALASKA="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )/../"

name=$1
shift
input=$1
shift
TESTDIR=$ALASKA/artifacts
mkdir -p $TESTDIR/$name

OUTDIR=$(realpath $TESTDIR/$name)


LOGFILE=$OUTDIR/log
rm -f $LOGFILE

echo "=========== Compiling ===========" > $LOGFILE

$ALASKA/tools/acc $input -g -O3 --keep-ir \
    --baseline $OUTDIR/baseline \
    -o $OUTDIR/bin \
    -I$ALASKA/src/rt/include \
    $@ 2>&1 | tee -a $LOGFILE


echo "=========== Running ===========" >> $LOGFILE
$OUTDIR/bin 2>&1 | tee -a $LOGFILE
