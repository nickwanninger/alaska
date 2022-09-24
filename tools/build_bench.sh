#!/bin/bash

ALASKA="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )/../"


source ${ALASKA}/enable
DST=${ALASKA}/bench
mkdir -p $DST



# set -e

# compile NPB

make -C test/npb clean
for bench in ft mg sp lu bt is ep cg
do
	CLASS=W
	make -C test/npb $bench CLASS=$CLASS
	mv $DST/$bench.$CLASS $DST/nas.$bench.base
	get-bc $DST/nas.$bench.base >/dev/null
	${ALASKA}/tools/acc $DST/nas.$bench.base.bc -lm -o $DST/nas.$bench.transformed >/dev/null
	# rm -f $DST/nas.$bench.base.bc
done

# exit

# compile gap
for bench in bfs bc cc cc_sv pr pr_spmv sssp # tc
do
	echo "  CC  " $bench
	gclang++ -std=c++11 -O3 -Wall ${ALASKA}/test/gapbs/src/${bench}.cc -o $DST/gap.$bench.base
	echo "  TX  " $bench
	get-bc $DST/gap.$bench.base >/dev/null
	${ALASKA}/tools/acc $DST/gap.$bench.base.bc -o $DST/gap.$bench.transformed
	rm -f $DST/gap.$bench.base.bc
done


echo "Compiling SQLite for fun... Wait a bit"
gclang -Wall ${ALASKA}/test/sqlite/*.c -ldl -lm -pthread -o $DST/sqlite.base
get-bc $DST/sqlite.base >/dev/null
${ALASKA}/tools/acc $DST/sqlite.base.bc -ldl -lm -pthread -o $DST/sqlite.transformed >/dev/null
