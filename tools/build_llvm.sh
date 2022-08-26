#!/bin/bash

ALASKA="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )/../"


if [ ! -f "${ALASKA}/.config" ]; then
	echo "ERROR: run 'make menuconfig' first";
	exit;
fi

source ${ALASKA}/.config


LLVM_VERSION=9.0.1
LLVM_DIR=${ALASKA}/dep/llvm/${LLVM_VERSION}

if [ ! -f "${LLVM_DIR}/enable" ]; then

	mkdir -p $LLVM_DIR/../

	pushd $LLVM_DIR/../

		git clone https://github.com/scampanoni/LLVM_installer.git ${LLVM_VERSION}
		pushd ${LLVM_VERSION}
			# Build
			make
		popd

	popd
fi
