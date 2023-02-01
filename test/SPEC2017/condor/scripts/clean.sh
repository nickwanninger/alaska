#!/bin/bash

# Restore the original IR file
pushd ./ ;
cd benchmarks ;
for i in `ls` ; do
  if ! test -d $i ; then
    continue ;
  fi
  if ! test -e ${i}/NOELLE_input.bc ; then
    continue ;
  fi

  cp ${i}/NOELLE_input.bc ${i}/${i}.bc ;
done
popd ;

# Clean
rm -f benchmarks/*/default.profraw ;
rm -f benchmarks/*/baseline* ;
rm -f benchmarks/*/noelle_output.txt ;
rm -f benchmarks/*/*train_output.txt ;
rm -f benchmarks/*/NOELLE_input.bc ;
rm -f benchmarks/*/afterLoopMetadata.bc ;
rm -f benchmarks/*/NOELLE_APIs.bc ;
rm -f benchmarks/*/parallelized*.bc ;
rm -f benchmarks/*/code*.bc ;
rm -f benchmarks/*/*newbin;
rm -f benchmarks/*/*.dot ;
rm -f benchmarks/*/Parallelizer_utils.bc ;
rm -f benchmarks/*/*output.txt ;
rm -f *.txt ;

pushd ./ ;
cd benchmarks ;
for i in `ls` ; do
  if ! test -d $i ; then
    continue ;
  fi
  rm -f ${i}/${i} ;
  rm -f ${i}/runme* ${i}/fetchRuntime.sh ${i}/download.sh ;
  rm -rf ${i}/include ;
  rm -f ${i}/input* ;
  rm -f ${i}/*.pgm ;
  rm -f ${i}/*.jpg ;
done
popd ;

pushd ./ ;
cd makefiles ; 
for i in `ls` ; do
  rm -f ../benchmarks/*/$i ;
done
popd ;

pushd ./ ;
cd condor ; 
make clean ; 
popd ;
