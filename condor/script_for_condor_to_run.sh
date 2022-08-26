#!/bin/bash



if [[ "$#" -ne 2 ]] ; then
    echo "Usage: $(basename $0) <run dir> <bench>" ;
		exit
fi


export PATH=


cd $1
shift

echo "running bench $1 on ${HOSTNAME} in ${PWD}"


OMP_NUM_THREADS=1 bin/$1 -g 20 >out/stdout 2>out/stderr
