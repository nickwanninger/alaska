#!/bin/bash

BUILD_DIR=`pwd`
cd ${BUILD_DIR}

if [ -d "${BUILD_DIR}/benchmarks/" ]; then
  rm -rf ${BUILD_DIR}/benchmarks
fi

if [ -e SPEC2017/uninstall.sh ] ; then
  cd SPEC2017
  printf 'yes' | ./uninstall.sh 
fi
cd ${BUILD_DIR}
rm -rf SPEC2017
rm -rf ./benchmarks
