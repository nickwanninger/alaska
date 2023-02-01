#!/bin/bash

# Compilers
CC=alaska
CPP=alaska++
OPT=opt
FLAGS="-O3"


function genBinary {
	# Libraries
	
	LIBS="-lm -lstdc++ -lpthread "
	if [ ! -f "${BENCHMARKS_DIR}/${1}/${1}.bc" ]; then
		echo "Warning: Bitcode not found for ${1}, skipping"
		return
	fi
	if [ -f "${BENCHMARKS_DIR}/${1}/${1}_newbin" ]; then
		rm ${BENCHMARKS_DIR}/${1}/${1}_newbin
	fi
	echo "Generating binary '${1}_newbin' for ${1} from ${1}.bc" ;
	cd ${BENCHMARKS_DIR}/${1}

	alaska++ -pg -b ${FLAGS} ${1}.bc ${LIBS} -o ${1}_newbin;
  exitOutput=$? ;
	chmod +x ${1}_newbin;
}


BUILD_DIR=`pwd`

# Check the inputs
if [ ! "${1}" ];  then
  echo "Error: Please provide benchmark name or 'all' for generating binaries from bitcode. For Example: ./scripts/binary.sh lbm_s"
  exit
fi

# Check the state
if [ ! -d "${BUILD_DIR}/SPEC2017" ]; then
	echo "Error: Please run ./scripts/install.sh first to install SPEC2017 and build benchmarks. Then run make bitcode; to extract bitcodes before running this script."
	exit
fi
if [ ! -d "${BUILD_DIR}/benchmarks" ]; then
	echo "Error: Please run ./scripts/bitcode.sh first to build benchmarks and extract bitcodes."
	exit
fi

# Generate Binaries from Bitcode
BENCHMARKS_DIR=${BUILD_DIR}/benchmarks



if [ "${1}" == "all" ]; then

	for benchmark in `ls ${BENCHMARKS_DIR}`; do
		#benchmark="$( echo $benchmark_string | awk -F'.' '{print $2}')"
		genBinary ${benchmark} ;
	done

else
	genBinary ${1} ;
fi


echo "-----------------------------------------------------------"

echo "DONE" 
exit $exitOutput ;
