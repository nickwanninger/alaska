#!/bin/bash

# Get args
benchmarkToRun="${1}" ;
binaryName="${2}" ;
inputArg="$3" ;

cd train ; 
argsToUse="`tail -n 1 ../run_train.sh | awk -F${benchmarkToRun} '{print $$2}'`" ;
./../${binaryName} $argsToUse; 
mv default.profraw ../ ;
