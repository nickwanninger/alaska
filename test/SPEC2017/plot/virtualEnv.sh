#!/bin/bash

export REPO_PATH=`pwd` ;
export LLVM_COMPILER=clang ;
virtualEnvDir="`pwd`/virtualEnv" ;

# Setup python virtual environment
if ! test -d ${virtualEnvDir} ; then
  mkdir ${virtualEnvDir} ;
  virtualenv -p python3 ${virtualEnvDir} ;
  source ${virtualEnvDir}/bin/activate ;
  pip install --upgrade pip ;
  pip install matplotlib ;

else
  source ${virtualEnvDir}/bin/activate ;
fi
