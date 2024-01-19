#.ONESHELL:
.DEFAULT_GOAL := alaska

MAKEFLAGS += --no-print-directory



ROOT=$(shell pwd)
export PATH:=$(ROOT)/local/bin:$(PATH)
export LD_LIBRARY_PATH:=$(ROOT)/local/lib:$(LD_LIBRARY_PATH)

-include .config



LLVM_VERSION=$(shell echo ${ALASKA_LLVM_VERSION})
LLVM=llvm-${LLVM_VERSION}

BUILD=build


BUILD_REQ=$(BUILD)/Makefile

$(BUILD)/Makefile:
	mkdir -p $(BUILD)
	cd $(BUILD) && cmake ../ -DCMAKE_INSTALL_PREFIX:PATH=$(ROOT)/local

alaska: .config deps $(BUILD_REQ)
	@cd $(BUILD) && cmake --build . --target install --config Debug
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



deps: local/bin/gclang local/bin/clang


local/bin/gclang: .config
	tools/build_gclang.sh



local/bin/clang: .config | deps/${LLVM}-build/Makefile
	$(MAKE) -C deps/${LLVM}-build
	$(MAKE) -C deps/${LLVM}-build install

deps/${LLVM}-build/Makefile: | deps/${LLVM}
	mkdir -p deps/${LLVM}-build
	cd deps/${LLVM}-build && cmake ../${LLVM}/llvm                              \
		-DCMAKE_BUILD_TYPE=Release                                          \
		-DCMAKE_INSTALL_PREFIX=$(ROOT)/local                                \
		-DLLVM_ENABLE_PROJECTS="clang;clang-tools-extra;openmp;compiler-rt" \
		-DLLVM_TARGETS_TO_BUILD="X86;AArch64;RISCV"
		# -DLLVM_BINUTILS_INCDIR="$(ROOT)/local/include"

deps/${LLVM}:
	mkdir -p deps
	wget -O deps/llvm.tar.xz https://github.com/llvm/llvm-project/releases/download/llvmorg-$(LLVM_VERSION)/llvm-project-$(LLVM_VERSION).src.tar.xz
	tar xvf deps/llvm.tar.xz -C deps/
	mv deps/llvm-project-$(LLVM_VERSION).src deps/${LLVM}






FORCE: # anything you want to force, depend on this
