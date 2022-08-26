#!/bin/bash

set -e

TEXAS="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )/../"

name=$1
shift
input=$1
shift
TESTDIR=$TEXAS/artifacts
mkdir -p $TESTDIR/$name

OUTDIR=$(realpath $TESTDIR/$name)


LOGFILE=$OUTDIR/log
rm -f $LOGFILE

echo "=========== Compiling ===========" > $LOGFILE

$TEXAS/tools/acc $input -g -O3 --keep-ir \
    --baseline $OUTDIR/baseline \
    -o $OUTDIR/bin \
    -I$TEXAS/src/newrt/include \
    $@ 2>&1 | tee -a $LOGFILE


echo "=========== Running ===========" >> $LOGFILE
$OUTDIR/bin 2>&1 | tee -a $LOGFILE
