m.ONESHELL:
.DEFAULT_GOAL := alaska

MAKEFLAGS += --no-print-directory



ROOT=$(shell pwd)
export PATH:=$(ROOT)/local/bin:$(PATH)
export LD_LIBRARY_PATH:=$(ROOT)/local/lib:$(LD_LIBRARY_PATH)

CC=clang
CXX=clang++
export CC
export CXX

BUILD=build


BUILD_REQ=$(BUILD)/Makefile

$(BUILD)/Makefile:
	@mkdir -p $(BUILD)
	@cd $(BUILD) && cmake ../ -DCMAKE_INSTALL_PREFIX:PATH=$(ROOT)/local

alaska: $(BUILD_REQ)
	@cd $(BUILD) && cmake --build . --target install --config Release
	@cp build/compile_commands.json .

sanity: alaska
	@local/bin/alaska -O3 test/sanity.c -o build/sanity
	@build/sanity

test: alaska
	@build/runtime/alaska_test

.PHONY: alaska all 

# Run compilation unit tests to validate that the compiler can
# handle all the funky control flow in the GCC test suite
unit: alaska FORCE
	tools/unittest.py

docs:
	@doxygen Doxyfile

clean:
	rm -rf build .*.o*

docker:
	docker build -t alaska .
	docker run -it --rm alaska bash

deps: local/bin/gclang local/bin/clang

local/bin/gclang:
	tools/build_gclang.sh

local/bin/clang:
	tools/get_llvm.sh


redis: FORCE
	nix develop --command bash -c "source enable && make -C test/redis"

FORCE: # anything you want to force, depend on this
