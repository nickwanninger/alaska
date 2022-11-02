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

# if [ ! -f ${PREFIX}/bin/cmake ]; then
# 	if [ ! -f cmake.tar.gz ]; then
# 		wget -O cmake.tar.gz https://github.com/Kitware/CMake/releases/download/v3.24.2/cmake-3.24.2.tar.gz
# 	fi

# 	tar xvf cmake.tar.gz
# 	pushd cmake-3.24.2

# 		./configure --prefix=${PREFIX} --parallel=$(nproc)
# 		make -j $(nproc) install
# 	popd

# fi


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

	rm -rf go
fi



if [ ! -f "${PREFIX}/bin/clang" ]; then


	if [ "$(uname)" == "Linux" ]; then
		case $(uname -m) in
			x86_64)
				# the RHEL version is more stable on other distributions, it seems
				LLVM_FILE=clang+llvm-15.0.2-x86_64-unknown-linux-gnu-rhel86.tar.xz
				;;
			arm)
				LLVM_FILE=clang+llvm-15.0.2-aarch64-linux-gnu.tar.xz
				;;
			aarch64)
				LLVM_FILE=clang+llvm-15.0.2-aarch64-linux-gnu.tar.xz
				;;
		esac
		if [ ! -f llvm.tar.xz ]; then
			wget -O llvm.tar.xz https://github.com/llvm/llvm-project/releases/download/llvmorg-15.0.2/$LLVM_FILE
		fi
		tar xvf llvm.tar.xz --strip-components=1 -C local/
	else
		echo "We have to compile LLVM from source on your platform..."
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


fi