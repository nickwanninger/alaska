#!/bin/bash

ALASKA="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )/../"


if [ ! -f "${ALASKA}/.config" ]; then
	echo "ERROR: run 'make menuconfig' first";
	exit;
fi

# We depend on LLVM for artifact reasons
${ALASKA}/tools/build_llvm.sh

source ${ALASKA}/enable
source ${ALASKA}/.config

NOELLE_VERSION=9.7.0

# Compile noelle if we need to
if [ ! -f ${ALASKA}/dep/noelle/enable ]; then
	mkdir -p ${ALASKA}/dep
	pushd ${ALASKA}/dep
		# Download the noelle release
		wget -O noelle.zip https://github.com/arcana-lab/noelle/archive/refs/tags/v${NOELLE_VERSION}.zip

		unzip noelle.zip
		mv "noelle-${NOELLE_VERSION}" noelle

		pushd noelle
			# noelle depends on executing in a git repo,
			# and we downloaded it from a release zip file.
			git init

			make

		popd
	popd
fi
