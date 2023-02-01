#!/bin/bash


ALASKA="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )/../"
# make sure we enable alaska
source $ALASKA/enable


function compile_bench () {
	BENCH=$1

	if [ ! -f benchmarks/$BENCH/$BENCH.bc ]; then
		scripts/compile.sh pure_c_cpp_speed
		scripts/bitcode.sh speed
		scripts/bitcode_copy.sh
		scripts/setup.sh test speed
	fi

	scripts/binary.sh $BENCH
}


pushd $ALASKA/test/SPEC2017
	if [ ! -d SPEC2017 ]; then
		tar -xvf $1
		chmod +x -R SPEC2017/bin SPEC2017/tools SPEC2017/*.sh
		scripts/install.sh
	fi


	compile_bench mcf_s
	compile_bench x264_s
	compile_bench xz_s
	compile_bench namd_s
	# we only do rate
	# compile_bench x264_r
	# compile_bench mcf_r
	# compile_bench xz_r
	# compile_bench namd_r
	# compile_bench parest_r

popd
