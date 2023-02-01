#!/bin/bash


function runOptimizations {

	if [ ! -d "${BUILD_DIR}/benchmarks/${1}" ]; then
			echo "Warning: Benchmark directory ${1} not found, skipping."
			return
	fi
	
	echo "Running your optimizations for \"${1}\"" ;
	cd ${BENCHMARKS_DIR}/${1}
	cp ${BUILD_DIR}/makefiles/* .
	make BENCHMARK=${1} >> noelle_output.txt 2>&1 ;
  exitOutput=$? ;
  echo "  The compilation output can be found here: `pwd`/noelle_output.txt" ;
}


BUILD_DIR=`pwd`

# Check the inputs
if [ ! "${1}" ];  then
  echo "Error: Please provide benchmark name or 'all' for running your optimizations. For Example: ./scripts/optimization.sh lbm_s"
  exit
fi

# Check the state
if [ ! -d "${BUILD_DIR}/SPEC2017" ]; then
	echo "Error: Please run \"./scripts/install.sh\" first to install SPEC2017 and build benchmarks. Then run make bitcode; to extract bitcodes before running this script."
	exit
fi
if [ ! -d "${BUILD_DIR}/benchmarks" ]; then
	echo "Error: Please run \"make bitcode\" first to build benchmarks and extract bitcodes."
	exit
fi

# Generate Binaries from Bitcode
BENCHMARKS_DIR=${BUILD_DIR}/benchmarks



if [ "${1}" == "all" ]; then

	for benchmark in `ls ${BENCHMARKS_DIR}`; do
		#benchmark="$( echo $benchmark_string | awk -F'.' '{print $2}')"
		runOptimizations ${benchmark} ;
	done

else
	runOptimizations ${1} ;
fi


echo "-----------------------------------------------------------"

echo "DONE" 
exit $exitOutput 
