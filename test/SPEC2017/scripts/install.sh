#!/bin/bash -e

PWD_PATH="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )/.." ;

if [ -d "${PWD_PATH}/benchmarks/" ]; then
  rm -rf ${PWD_PATH}/benchmarks ;
fi

cd SPEC2017 ;
printf 'yes' | ./install.sh ;
cp ${PWD_PATH}/patches/gclang.cfg ${PWD_PATH}/SPEC2017/config/ ;
cp ${PWD_PATH}/patches/pure_c_cpp_speed.bset ${PWD_PATH}/SPEC2017/benchspec/CPU/ ;
cp ${PWD_PATH}/patches/pure_c_cpp_rate.bset ${PWD_PATH}/SPEC2017/benchspec/CPU/ ;

cd ${PWD_PATH}/SPEC2017 ;
tar xf ${PWD_PATH}/patches/parest_r_patch.tar.xz ;

cp -r ${PWD_PATH}/patches/SPEC2017 ${PWD_PATH} ;

