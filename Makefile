#.ONESHELL:
.DEFAULT_GOAL := alaska

MAKEFLAGS += --no-print-directory


ROOT=$(shell pwd)
export PATH:=$(ROOT)/local/bin:$(PATH)
export LD_LIBRARY_PATH:=$(ROOT)/local/lib:$(LD_LIBRARY_PATH)

-include .config

BUILD=build


BUILD_REQ=$(BUILD)/Makefile

$(BUILD)/Makefile:
	mkdir -p $(BUILD)
	cd $(BUILD) && cmake ../ -DCMAKE_INSTALL_PREFIX:PATH=$(ROOT)/local

alaska: .config deps $(BUILD_REQ)
	@#cd $(BUILD) && cmake --build . --target install --config Debug
	@cd $(BUILD) && $(MAKE) install
	@cp build/compile_commands.json .

sanity: alaska
	@local/bin/alaska -O3 test/sanity.c -o build/sanity
	@build/sanity

test: alaska
	@python3 tools/unittest.py -s

.PHONY: alaska all bench bench/nas libc


# Create a virtual environment in ./venv/ capable of
# running our benchmarks with waterline. For Artifact evaluation
venv: venv/touchfile
venv/touchfile: requirements.txt
	test -d venv || virtualenv venv
	. venv/bin/activate; pip install -Ur requirements.txt
	touch venv/touchfile

bench: alaska venv FORCE
	. venv/bin/activate; python3 test/bench.py

# Run compilation unit tests to validate that the compiler can
# handle all the funky control flow in the GCC test suite
unit: alaska FORCE
	tools/unittest.py

docs:
	@doxygen Doxyfile

clean:
	rm -rf build .*.o*

# Chose the default configuration
defconfig:
	@rm -f .config
	@echo "Using default configuration"
	@echo "q" | env TERM=xterm-256color python3 tools/menuconfig.py >/dev/null

# Open the TUI menuconfig environment to chose your own configuration
cfg: menuconfig
menuconfig:
	@python3 tools/menuconfig.py

redis: alaska
	$(MAKE) -C test/redis

build/lua: alaska
	local/bin/alaska -O2 -b -k test/lua/onelua.c -o $@

docker:
	docker build -t alaska .
	docker run -it --rm alaska bash





# The following portion of this makefile exists to build the dependencies of ALASKA into local/
# The main part of this is to produce two binaries, 
#  - local/bin/ar    (From binutils)
#  - local/bin/clang (From LLVM)
# The reason we must build `ar' from binutils is so that we can use LLVM's LTO infrastructure,
# and unfortunately this requires that we build both binutils and LLVM from source...
deps: local/bin/ar local/bin/clang


# bin/ar is produced by binutils.
# /deps/ contains the source code of the dependencies
local/bin/ar: deps/binutils
	$(MAKE) -C deps/binutils
	$(MAKE) -C deps/binutils install


# Clone binutils
deps/binutils:
	mkdir -p deps
	git clone --depth 1 git://sourceware.org/git/binutils-gdb.git deps/binutils
	# Configure binutils with `./configure'
	cd deps/binutils && ./configure \
		--enable-gold \
		--enable-plugins \
		--disable-werror \
		--prefix=$(ROOT)/local/


# Compile clang into local/bin/clang. This *requires* local/bin/ar
local/bin/clang: deps/llvm-build/Makefile local/bin/ar
	$(MAKE) -C deps/llvm-build
	$(MAKE) -C deps/llvm-build install

deps/llvm-build/Makefile: deps/llvm
	mkdir -p deps/llvm-build
	cd deps/llvm-build && cmake ../llvm/llvm         \
		-DCMAKE_BUILD_TYPE=Release                     \
		-DCMAKE_INSTALL_PREFIX=$(ROOT)/local           \
		-DLLVM_BINUTILS_INCDIR="$(ROOT)/local/include" \
		-DLLVM_ENABLE_PROJECTS="clang;lld"

LLVM_VERSION=15.0.2
deps/llvm:
	wget -O deps/llvm.tar.xz https://github.com/llvm/llvm-project/releases/download/llvmorg-$(LLVM_VERSION)/llvm-project-$(LLVM_VERSION).src.tar.xz
	tar xvf deps/llvm.tar.xz -C deps/
	mv deps/llvm-project-$(LLVM_VERSION).src deps/llvm

FORCE: # anything you want to force, depend on this
