#!/usr/bin/env bash

set -e

if [ ! -f bench/speedup.csv ]; then
	echo "benchmark,suite,baseline,alaska,speedup" > bench/speedup.csv
fi



function run_spec() {
	number=$1
	shift
	bench=$1
	shift
	if [ -f test/SPEC2017/benchmarks/$bench/${bench}_newbin ]; then
		pushd test/SPEC2017/benchmarks/$bench/test 2>/dev/null
			pwd
			echo "RUN spec/$bench (baseline)"
			/usr/bin/time -f'%e' -o time ../${bench}_newbin.base $@ >/dev/null
			BASELINE=$(cat time)
			rm time


			echo "RUN spec/$bench (alaska)"
			/usr/bin/time -f'%e' -o time ../${bench}_newbin $@ >/dev/null
			ALASKA=$(cat time)
			rm time
		popd 2>/dev/null
		SPEEDUP=$(echo "scale=2; $BASELINE/$ALASKA" | bc)
		echo "$number.$bench,SPEC2017,$BASELINE,$ALASKA,$SPEEDUP" >> bench/speedup.csv
	else
		echo "SKIP $bench"
	fi
}

for trial in $(seq 1 10)
do

	# run_spec 605 mcf_s inp.in
	# run_spec 625 x264_s --dumpyuv 50 --frames 156 -o BuckBunny_New.264 BuckBunny.yuv 1280x720
	# run_spec 657 xz_s cpu2006docs.tar.xz 1 055ce243071129412e9dd0b3b69a21654033a9b723d874b2015c774fac1553d9713be561ca86f74e4f16f22e664fc17a79f30caa5ad2c04fbc447549c2810fae 629064 -1 4e


	# todo:
	#  useful out of float:
	#   blender_s
	#   imagick_s
	#   lbm_s
	#   cactuBSSN_s
	# 

	# run nas
	# for bench in ft mg sp lu bt ep cg
	for bench in ft mg lu bt ep cg
	do
		out=bench/results/nas/$bench
		mkdir -p $out
		echo "RUN nas/$bench (baseline)"
		bench/nas/$bench.base >$out/stdout.base
		echo "RUN nas/$bench (alaska)"
		bench/nas/$bench >$out/stdout.alaska


		# extract the time in seconds from the output
		BASELINE=$(grep 'Time in seconds' $out/stdout.base | sed 's/.*=\W*//')
		ALASKA=$(grep 'Time in seconds' $out/stdout.alaska | sed 's/.*=\W*//')

		SPEEDUP=$(echo "scale=2; $BASELINE/$ALASKA" | bc)
		echo "$bench,NAS,$BASELINE,$ALASKA,$SPEEDUP" >> bench/speedup.csv
	done


	# run gap
	for bench in bfs bc cc cc_sv pr pr_spmv sssp # tc
	do
	 	out=bench/results/gap/$bench
	 	SIZE=16
	 	mkdir -p $out
	 	echo "RUN gap/$bench (baseline)"
	 	bench/gap/$bench.base -g $SIZE >$out/stdout.base
	 	echo "RUN gap/$bench (alaska)"
	 	bench/gap/$bench -g $SIZE >$out/stdout.alaska
	
	
	 	# # extract the time in seconds from the output
	 	BASELINE=$(grep 'Average Time:' $out/stdout.base | sed 's/.*:\W*//')
	 	ALASKA=$(grep 'Average Time:' $out/stdout.alaska | sed 's/.*:\W*//')
	 	SPEEDUP=$(echo "scale=2; $BASELINE/$ALASKA" | bc)
	 	echo "$bench,GAPBS,$BASELINE,$ALASKA,$SPEEDUP" >> bench/speedup.csv
	done
done
# cat bench/speedup.csv
