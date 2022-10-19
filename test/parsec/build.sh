#!/usr/bin/env bash


HERE="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )/"
ALASKA="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )/../../"

source $ALASKA/enable

if [ ! -d parsec-3.0 ]; then
	if [ ! -d parsec.tar.gz ]; then
		wget -O parsec.tar.gz http://parsec.cs.princeton.edu/download/3.0/parsec-3.0.tar.gz
	fi
	tar xvf parsec.tar.gz
fi


A="$HERE/parsec-3.0/pkgs/apps"
K="$HERE/parsec-3.0/pkgs/kernels"
BLD=build

CFLAGS="-lm -pthread -O3 -Wno-format "

mkdir -p $BLD


cp gclang.bldconf parsec-3.0/config/
ls parsec-3.0/config
# cp gclang.runconf parsec-3.0/config/

cd parsec-3.0
CONFIG=gcc-serial

./bin/parsecmgmt -a fulluninstall -c $CONFIG
./bin/parsecmgmt -a fullclean -c $CONFIG

benchmarksList=("blackscholes" "bodytrack" "fluidanimate" "freqmine" "swaptions" "x264" "canneal" "streamcluster") ;


for elem in ${benchmarksList[@]} ; do
  ./bin/parsecmgmt -a build -c $CONFIG -p ${elem}
done

exit

