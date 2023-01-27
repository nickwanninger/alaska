#!/bin/bash

# Fetch the inputs
if test $# -lt 1 ; then
  echo "USAGE: `basename $0` OUTPUT_DIR" ;
  exit 1 ;
fi
filesDir="$1" ;

# Create the output directory
outDir="${filesDir}/output" ;
mkdir -p ${outDir} ;

# Create the bitcode directories
bitcodeDir="${filesDir}/parallelized_bitcode" ;
mkdir -p ${bitcodeDir} ;
seqBitcodeDir="${filesDir}/baseline_bitcode" ;
mkdir -p ${seqBitcodeDir} ;
binaryDir="${filesDir}/binary" ;
mkdir -p ${binaryDir} ;

# Copy the text outputs ;
cp noelle*.txt ${filesDir}/ ;

# Copy the makefile
cp makefiles/Makefile ${filesDir}/ ;

# Copy the times 
mkdir -p ${filesDir}/data ;
cp times*.txt ${filesDir}/data/ ;

# Copy the files ;
for i in `find ./benchmarks -name noelle_output.txt` ; do
  echo "Copy $i" ;

  # Fetch the names
  benchDir=`dirname $i` ;
  benchName=`basename $benchDir` ;
  fileName=`basename $i` ;

  # Create the benchmark directory in the output directory
  mkdir -p ${outDir}/$benchDir ;

  # Copy the output
  cp $i ${outDir}/${benchDir}/$fileName ;

  # Copy the baseline bitcode
  seqBitcodeFile="${benchDir}/baseline_with_metadata.bc" ;
  if test -f $seqBitcodeFile ; then
    mkdir -p ${seqBitcodeDir}/${benchDir} ;
    cp ${seqBitcodeFile} ${seqBitcodeDir}/${benchDir}/ ;
  fi
  seqBitcodeFile="${benchDir}/baseline.bc" ;
  if test -f $seqBitcodeFile ; then
    mkdir -p ${seqBitcodeDir}/${benchDir} ;
    cp ${seqBitcodeFile} ${seqBitcodeDir}/${benchDir}/ ;
  fi
  seqBitcodeFile="${benchDir}/NOELLE_input.bc" ;
  if test -f $seqBitcodeFile ; then
    mkdir -p ${seqBitcodeDir}/${benchDir} ;
    cp ${seqBitcodeFile} ${seqBitcodeDir}/${benchDir}/ ;
  fi

  # Copy the parallelized bitcode
  bitcodeFile="${benchDir}/${benchName}.bc" ;
  if test -f $bitcodeFile ; then
    mkdir -p ${bitcodeDir}/${benchDir} ;
    cp ${bitcodeFile} ${bitcodeDir}/${benchDir}/ ;
  fi

  # Copy the binary
  binaryFile="${benchDir}/${benchName}" ;
  if test -f $binaryFile ; then
    mkdir -p ${binaryDir}/${benchDir} ;
    cp ${binaryFile} ${binaryDir}/${benchDir}/ ;
  fi

done
