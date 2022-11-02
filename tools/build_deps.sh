#!/bin/bash

ROOT="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )/../"


mkdir -p $ROOT/dep
pushd $ROOT/dep

if [ ! -f "${ROOT}/.config" ]; then
	echo "ERROR: run 'make menuconfig' first";
	exit;
fi

LLVM_VERSION=15.0.2
PREFIX=$ROOT/dep/local
mkdir -p ${PREFIX}/{bin,lib}

export PATH=$PREFIX/bin:$PATH
export LD_LIBRARY_PATH=$PREFIX/lib:$LD_LIBRARY_PATH

# cmake --version
# exit

if [ ! -f ${PREFIX}/bin/cmake ]; then
	if [ ! -f cmake.tar.gz ]; then
		wget -O cmake.tar.gz https://github.com/Kitware/CMake/releases/download/v3.24.2/cmake-3.24.2.tar.gz
	fi

	tar xvf cmake.tar.gz
	pushd cmake-3.24.2

		./configure --prefix=${PREFIX} --parallel=$(nproc)
		make -j $(nproc) install
	popd

fi


if [ ! -f ${PREFIX}/bin/gclang ]; then
	# install go
	KERNEL=$(uname | tr '[:upper:]' '[:lower:]')

	case $(uname -p) in
		x86_64)
			ARCH=amd64
			;;
		arm)
			ARCH=arm64
			;;
		aarch64)
			ARCH=arm64
			;;
	esac

	wget -O go.tar.gz https://golang.google.cn/dl/go1.19.1.${KERNEL}-${ARCH}.tar.gz
	tar xvf go.tar.gz

	GOPATH=${PREFIX} GO111MODULE=off go/bin/go get -v github.com/SRI-CSL/gllvm/cmd/...
fi



if [ ! -f "${PREFIX}/bin/clang" ]; then
	if [ ! -f llvm.tar.xz ]; then
		wget -O llvm.tar.xz https://github.com/llvm/llvm-project/releases/download/llvmorg-${LLVM_VERSION}/llvm-project-${LLVM_VERSION}.src.tar.xz
		tar xvf llvm.tar.xz
	fi

	pushd llvm-project-${LLVM_VERSION}.src
		mkdir build
		pushd build
			cmake ../llvm -G Ninja                                    \
				-DCMAKE_INSTALL_PREFIX=${PREFIX}                        \
				-DCMAKE_BUILD_TYPE=Release                              \
				-DLLVM_ENABLE_PROJECTS="clang;clang-tools-extra;openmp;compiler-rt" \
				-DLLVM_TARGETS_TO_BUILD="X86;AArch64;RISCV"                         \
				-DLLVM_ENABLE_LLD=True

			ninja install
		popd
	popd
fi








# # We depend on LLVM for artifact reasons
# ${ALASKA}/tools/build_llvm.sh
# 
# source ${ALASKA}/enable
# source ${ALASKA}/.config
# 
# NOELLE_VERSION=9.7.0
# 
# # Compile noelle if we need to
# if [ ! -f ${ALASKA}/dep/noelle/enable ]; then
# 	mkdir -p ${ALASKA}/dep
# 	pushd ${ALASKA}/dep
# 		# Download the noelle release
# 		wget -O noelle.zip https://github.com/arcana-lab/noelle/archive/refs/tags/v${NOELLE_VERSION}.zip
# 
# 		unzip noelle.zip
# 		mv "noelle-${NOELLE_VERSION}" noelle
# 
# 		pushd noelle
# 			# noelle depends on executing in a git repo,
# 			# and we downloaded it from a release zip file.
# 			git init
# 
# 			make
# 
# 		popd
# 	popd
# fi
