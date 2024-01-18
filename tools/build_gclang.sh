#!/bin/bash

ROOT="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )/../"


mkdir -p $ROOT/local
pushd $ROOT/local

if [ ! -f "${ROOT}/.config" ]; then
	echo "ERROR: run 'make menuconfig' first";
	exit;
fi

LLVM_VERSION=15.0.2
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
