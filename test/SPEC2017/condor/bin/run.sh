#!/bin/bash

# Fetch the inputs
if test $# -lt 2 ; then
  echo "USAGE: `basename $0` REPO_DIR COMMAND " ;
  exit 1;
fi
repoDir="$1" ;
binaryToRun="$2" ;

# Setup the environment
source ~/.bash_profile ;

# Go to the working directory
cd $repoDir/ ;

# Run
./condor/scripts/${binaryToRun} ${@:3};
