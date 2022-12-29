#!/usr/bin/env bash

set -e

echo "benchmark,baseline,alaska,speedup" > bench/speedup.csv

# run nas
for bench in ft mg sp lu bt ep cg
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
	echo "nas/$bench,$BASELINE,$ALASKA,$SPEEDUP" >> bench/speedup.csv
done

exit


# run gap
for bench in bfs bc cc cc_sv pr pr_spmv sssp # tc
do
	out=bench/results/gap/$bench
	SIZE=15
	mkdir -p $out
	echo "RUN gap/$bench (baseline)"
	bench/gap/$bench.base -g $SIZE >$out/stdout.base
	echo "RUN gap/$bench (alaska)"
	bench/gap/$bench -g $SIZE >$out/stdout.alaska


	# # extract the time in seconds from the output
	BASELINE=$(grep 'Average Time:' $out/stdout.base | sed 's/.*:\W*//')
	ALASKA=$(grep 'Average Time:' $out/stdout.alaska | sed 's/.*:\W*//')
	SPEEDUP=$(echo "scale=2; $BASELINE/$ALASKA" | bc)
	echo "gap/$bench,$BASELINE,$ALASKA,$SPEEDUP" >> bench/speedup.csv
done

# cat bench/speedup.csv
