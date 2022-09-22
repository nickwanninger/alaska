#!/bin/bash

ALASKA="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )/../"


if [ ! -f "${ALASKA}/.config" ]; then
	echo "ERROR: run 'make menuconfig' first";
	exit;
fi

source ${ALASKA}/enable
source ${ALASKA}/.config

# cd into the root directory of the ALASKA project
cd ${ALASKA}

BUILD=${ALASKA}/build

# Make sure the dependencies are built
${ALASKA}/tools/build_deps.sh


# If there isn't a makefile in the target directory, Configure cmake
if [ ! -f ${BUILD}/Makefile ]; then
	echo "Configuring ALASKA..."
	# make sure the directory exists
	mkdir -p ${BUILD}
	cd ${BUILD}
	# configure cmake
	cmake ${ALASKA}
	echo "Done configuring."
fi




pushd ${BUILD} >/dev/null
	make --no-print-directory -j $(nproc)
	make install --no-print-directory

	cp ${BUILD}/compile_commands.json ${BUILD}/../
popd >/dev/null


# pushd ${ALASKA}/rt >/dev/null
# 	cargo build --release --target-dir=${BUILD} # --out-dir=${BUILD}
# popd >/dev/null
