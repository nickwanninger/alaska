#!/bin/bash


if [[ "$#" -ne 1 ]] ; then
    echo "Usage: $0 <run dir>" ;
		exit
fi

DIR=$1
shift


# echo "bench,baseline,transformed"
printf "%12s, %13s, %13s, raw calls\n" bench baseline transformed


# go through the GAP benchmark suite and print out slowdown
for bench in gap.bc gap.bfs gap.cc gap.cc_sv gap.pr gap.pr_spmv gap.sssp gap.tc
do
	if [ -d $DIR/$bench.base ]; then
		BASELINE=$(grep "Average Time:" $DIR/$bench.base/out/stdout | awk '{print $3}')
		TRANSLATED=$(grep "Average Time:" $DIR/$bench.texas/out/stdout | awk '{print $3}')

		# echo "$bench, $BASELINE, $TRANSLATED"
		printf "%12s, %12.5f1, %12.5f1\n" "$bench" "$BASELINE" "$TRANSLATED"
	fi

done


# go through the GAP benchmark suite and print out slowdown
for bench in nas.ft nas.mg nas.sp nas.lu nas.bt nas.is nas.ep nas.cg
do
	if [ -d $DIR/$bench.base ]; then

		BASELINE=$(grep "Time in seconds" $DIR/$bench.base/out/stdout | awk '{print $5}')
		TRANSLATED=$(grep "Time in seconds" $DIR/$bench.texas/out/stdout | awk '{print $5}')
		CALLS_RAW=$(grep "dynamic calls:" $DIR/$bench.texas/out/stdout | sed "s/.*dynamic calls: //g" )

		printf "%12s, %12.5f1, %12.5f1, $CALLS_RAW\n" "$bench" "$BASELINE" "$TRANSLATED"
	fi
done

