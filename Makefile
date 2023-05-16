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

alaska: .config local/bin/clang $(BUILD_REQ)
	@cd $(BUILD) && cmake --build . --target install --config Debug
	@cp build/compile_commands.json .

local/bin/clang:
	tools/build_deps.sh

deps: local/bin/clang

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


FORCE: # anything you want to force, depend on this
