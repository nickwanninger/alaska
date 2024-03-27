#!/bin/bash

ROOT="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )/../"


mkdir -p $ROOT/local
pushd $ROOT/local


LLVM_VERSION=16.0.4
PREFIX=$ROOT/local
mkdir -p ${PREFIX}/{bin,lib}

export PATH=$PREFIX/bin:$PATH
export LD_LIBRARY_PATH=$PREFIX/lib:$LD_LIBRARY_PATH


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
	rm -rf go.tar.gz
fi



if [ ! -f "${PREFIX}/bin/clang" ]; then


	LLVM_FILE=""

	if [ "$(uname)" == "Linux" ]; then
		case $(uname -m) in
			x86_64)
				# the RHEL version is more stable on other distributions, it seems
				LLVM_FILE=clang+llvm-16.0.4-x86_64-linux-gnu-ubuntu-22.04.tar.xz
				;;
			arm)
				LLVM_FILE=clang+llvm-16.0.4-aarch64-linux-gnu.tar.xz
				;;
			aarch64)
				LLVM_FILE=clang+llvm-16.0.4-aarch64-linux-gnu.tar.xz
				;;
		esac
	fi


	if [ "$(uname)" == "Darwin" ]; then
		case $(uname -m) in
			arm64)
		    LLVM_FILE=clang+llvm-16.0.4-arm64-apple-darwin22.0.tar.xz
				;;
		esac
	fi

	if [ "${LLVM_FILE}" != "" ]; then

		if [ ! -f llvm.tar.xz ]; then
			wget -O llvm.tar.xz https://github.com/llvm/llvm-project/releases/download/llvmorg-${LLVM_VERSION}/$LLVM_FILE
		fi
		tar xvf llvm.tar.xz --strip-components=1 -C ${PREFIX}

	else
		echo "We have to compile LLVM from source on your platform..."
		sleep 4
		if [ ! -d llvm-project-${LLVM_VERSION}.src ]; then
			wget -O llvm.tar.xz https://github.com/llvm/llvm-project/releases/download/llvmorg-${LLVM_VERSION}/llvm-project-${LLVM_VERSION}.src.tar.xz
			tar xvf llvm.tar.xz
		fi
		pushd llvm-project-${LLVM_VERSION}.src
			mkdir -p build
			pushd build
				cmake ../llvm -G Ninja                                    \
					-DCMAKE_INSTALL_PREFIX=${PREFIX}                        \
					-DCMAKE_BUILD_TYPE=Release                              \
					-DLLVM_ENABLE_PROJECTS="clang;clang-tools-extra;openmp;compiler-rt" \
					-DLLVM_TARGETS_TO_BUILD="X86;AArch64;RISCV"
					# -DLLVM_ENABLE_LLD=True

				ninja install
			popd
		popd
	fi

	# rm -rf llvm.tar.xz

	

fi
