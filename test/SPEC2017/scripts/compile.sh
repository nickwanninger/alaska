#!/bin/bash

# Setup the local variables and environment
BUILD_DIR=`pwd`
pushd ./;
cd ../../install/bin ;
export PATH=`pwd`:$PATH ;
popd ;

if [ ! -d "${BUILD_DIR}/SPEC2017" ]; then
	echo "Error: Please run \"./scripts/install.sh\"  first to install SPEC2017."
	exit
fi
cd SPEC2017
source shrc
runcpu -D --loose --size test --tune peak -a setup --config gclang $1
echo "DONE installing SPEC2017 at \"${BUILD_DIR}/SPEC2017\"" 
