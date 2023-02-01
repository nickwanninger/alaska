#!/bin/bash

# Fetch the inputs
if test $# -lt 3 ; then
  echo "USAGE: `basename $0` CONDOR_FILE FILE_PREFIX FILE_SUFFIX" ;
  exit 1;
fi
condorFile="$1" ;
prefixName="$2" ;
if test "$prefixName" == " " ; then
  prefixName="" ;
fi
suffixName="$3" ;
origDir=`pwd` ;

# Instantiate the condor script
python2 scripts/generateCondorScript.py scripts/${condorFile} ./${condorFile} "$condorFile" " " ;

# Add the benchmarks
pushd ./ ;
cd ../benchmarks ; 
for i in `ls` ; do

  # Check if it is a directory
  if ! test -d $i ; then
    continue ;
  fi

  # Check if there is the bitcode
  filesFound="`ls ${i}/${prefixName}*${suffixName} 2> /dev/null`"
  if test "${filesFound}" == "" ; then
    continue ;
  fi
  echo "  Benchmark ${i} is added" ;

  # Add the entry to the condor file
  echo "Benchmark = $i" >> ${origDir}/${condorFile} ;
  echo "Queue" >> ${origDir}/${condorFile} ;
done
popd ;
