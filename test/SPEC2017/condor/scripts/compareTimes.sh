#!/bin/bash

function computeMedian {
  local dataFile=$1 ;
  local tempFile=`mktemp` ;

  # Sort the data
  sort -g $dataFile > $tempFile ;

  # Fetch the number of lines of the file
  lines=`wc -l $tempFile | awk '{print $1}'` ;

  # Compute the median
  median=`echo "$lines / 2" | bc` ;

  # Fetch the median
  echo $median
  result=`awk -v median=$median '
    BEGIN {
      c = 0;
    }{
      if (c == median){
        print ;
      }
      c++;
    }' $tempFile` ;

  # Free
  rm $tempFile ;

  return ;
}

# Fetch the inputs
if test $# -lt 3 ; then
  echo "USAGE: `basename $0` BASELINE_DIRECTORY OPTIMIZATION_DIRECTORY SPEEDUP_FILE" ;
  exit 1;
fi
baselineDir="`realpath $1`" ;
optDir="`realpath $2`" ;
speedupFile="`realpath $3`" ;

# Variables
origDir="`pwd`" ;
result="0" ;

# Compute the speedups
rm -f $speedupFile ;
pushd ./ ;
cd $baselineDir ;
for i in `ls` ; do

  # Compute the name of the benchmark
  bench=`echo "$i" | sed s/times_//g | sed s/\.txt//g` ;
  echo "Benchmark: $i" ;

  # Fetch the files
  baselineFile="`realpath $i`" ;
  optFile="`realpath ${optDir}/$i`" ;
  echo "  Baseline = $baselineFile" ;
  echo "  Optimization = $optFile" ;
  if ! test -f $optFile ; then
    echo "  The optimization file doesn't exist" ;
    echo "$bench 0" >> $speedupFile ;
    continue ;
  fi

  # Fetch the medians
  computeMedian $baselineFile ;
  baselineMedian=$result ;
  echo "  Baseline = $baselineMedian" ;

  computeMedian $optFile ;
  optMedian=$result ;
  echo "  Baseline = $optMedian" ;

  # Compute the speedup
  speedup=`echo "scale=3; $baselineMedian / $optMedian" | bc` ;
  echo "  Speedup = $speedup" ;

  # Dump the speedup
  echo "$bench $speedup" >> $speedupFile ;
done
popd ;
