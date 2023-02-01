#!/bin/bash

# Get benchmark suite dir
PWD_PATH="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )/.." ;
benchmarkSuiteName="SPEC2017" ;

# Get a list of all generated binaries, we'll use it to extract bitcode files
llvmVersion=`llvm-config --version` ;
bitcodesDir="${PWD_PATH}/bitcodes/LLVM15.0/${benchmarkSuiteName}" ;

# Check if bitcode dir exists
if ! test -d ${bitcodesDir} ; then
  echo "ERROR: ${bitcodesDir} not found. Run make bitcode." ;
  exit 1 ;
fi

# Get bitcode benchmark dir
benchmarksDir="${PWD_PATH}/benchmarks" ;
mkdir -p ${benchmarksDir} ;

# Copy bitcode in benchmark
cp -r ${bitcodesDir}/* ${benchmarksDir}/ ;
