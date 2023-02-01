#!/bin/bash

# Fetch the input
if test $# -lt 1 ; then
  echo "USAGE: `basename $0` BENCHMARK" ;
  exit 1;
fi
b="$1" ;
echo "Optimizing $b" ;

# Clean
rm -f benchmarks/${b}/default.profraw ;
rm -f benchmarks/${b}/baseline* ;
rm -f benchmarks/${b}/noelle_output.txt ;
rm -f benchmarks/${b}/*train_output.txt ;

# Parallelize the benchmark
make optimization BENCHMARK=$b ;
if test $? -ne 0 ; then
  echo "$b Error during parallelization" >> noelle_optimize.txt ;
  exit 1;
fi
