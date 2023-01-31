#!/bin/bash

if [ ! "${1}" == "rate" ] && [ ! "${1}" == "speed" ]; then
  echo "Please provide version  [rate,speed] for setting up run directories. For Example: \"./scripts/bitcode.sh rate\" "
  exit
fi

# Setup the local variables and environment
BUILD_DIR=`pwd`
pushd ./;
cd ../../install/bin ;
export PATH=`pwd`:$PATH ;
popd ;

mkdir -p bitcodes/LLVM15.0/SPEC2017

cd bitcodes/LLVM15.0/SPEC2017
BITCODE_DIR=`pwd` 
cd ${BUILD_DIR}

#delete previously generated bitcodes
#rm -rf ${BITCODE_DIR}/* # ED: this script is invoked twice (for speed and rate benchmarks), if we remove the whole bitcode directory we'll be left with only one set of benchmarks.

if [ ! -d "${BUILD_DIR}/benchmarks/" ]; then
  mkdir ${BUILD_DIR}/benchmarks
fi

if [ ! -d "${BUILD_DIR}/SPEC2017" ]; then
	echo "Error: Please run \"./scripts/setup.sh\" and \"./scripts/compile.sh\" first to install SPEC2017 and build benchmarks."
	exit
fi


#Setup Run directories with runcpu
cd SPEC2017
source shrc
#runcpu --loose --size ${1} --tune peak -a setup --config gclang pure_c_cpp_$2

#Copy Run directories and extract bitcodes
BENCHMARKS_DIR=${BUILD_DIR}/benchmarks


if [ "${1}" == "rate" ]; then
	key="_r"
elif [ "${1}" == "speed" ]; then
	key="_s"
else
	key=""
fi

for benchmark_string in `sed 1d ${BUILD_DIR}/patches/pure_c_cpp_${1}.bset | grep ${key}`; do
	benchmark="$( echo $benchmark_string | awk -F'.' '{print $2}')"
	if [ ! -d "${BENCHMARKS_DIR}/${benchmark}" ]; then
		mkdir ${BENCHMARKS_DIR}/${benchmark}
	fi
	echo ${benchmark}
	go ${benchmark} run
	if [ "${benchmark}" == "xalancbmk_r" ]; then
		cp run_peak_test_gclang.0000/cpuxalan_r_peak.gclang ${BENCHMARKS_DIR}/${benchmark}/${benchmark}
	else
		cp run_peak_test_gclang.0000/${benchmark}_peak.gclang ${BENCHMARKS_DIR}/${benchmark}/${benchmark}
	fi
	cd ${BENCHMARKS_DIR}/${benchmark}
	
	get-bc ${benchmark}

done
echo "-----------------------------------------------------------"
echo "Run directories created at \"${BENCHMARKS_DIR}\" contain respective binaries and bitcodes. Run workload '${1}' with ./run.sh found at respective workload directories."  

# Copy bitcode into bitcode dir and removebenchmarks dir, it will be created by bitocde_copy.sh
cp -r ${BENCHMARKS_DIR}/* ${BITCODE_DIR}/ ;
rm -r ${BENCHMARKS_DIR} ;

cd ${BITCODE_DIR}
#tar -czf spec2017.tgz *
echo "DONE" 

exit

