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


newtest: alaska
	./alaska-clang -O3 test/worst.c -c -o build/worst.o
	./alaska-clang -O3 build/worst.o -o build/worst

	
# targets to build benchmarks
NAS_BENCHMARKS := bench/nas/ft bench/nas/mg bench/nas/sp bench/nas/lu bench/nas/bt bench/nas/is bench/nas/ep bench/nas/cg
# NAS_BENCHMARKS := bench/nas/ft bench/nas/mg bench/nas/sp bench/nas/lu bench/nas/is bench/nas/ep bench/nas/cg
GAP_BENCHMARKS := bench/gap/bfs bench/gap/bc bench/gap/cc bench/gap/cc_sv bench/gap/pr bench/gap/pr_spmv bench/gap/sssp



# BENCH_FLAGS=-fopenmp
BENCH_FLAGS=

NAS_CLASS=B
bench/nas/%: alaska
	@mkdir -p bench/nas
	@echo "  CC  " $@
	@$(MAKE) --no-print-directory -C test/npb $* CLASS=$(NAS_CLASS) >/dev/null
	@mv bench/$*.$(NAS_CLASS) bench/nas/$*.base
	@get-bc bench/nas/$*.base >/dev/null
	@local/bin/alaska $(BENCH_FLAGS) -b -O3 bench/nas/$*.base.bc -k -lm -lomp -o $@ >/dev/null
	@rm bench/nas/$*.base.bc


bench/gap/%: alaska
	@mkdir -p bench/gap
	@echo "  CC  " $@
	@local/bin/alaska++ $(BENCH_FLAGS) -std=c++11 -k -b -O3 -Wall test/gapbs/src/$*.cc -o $@



venv: venv/touchfile

venv/touchfile: requirements.txt
	test -d venv || virtualenv venv
	. venv/bin/activate; pip install -Ur requirements.txt
	touch venv/touchfile


bench: alaska venv FORCE
	. venv/bin/activate; python3 test/bench.py

docs:
	@doxygen Doxyfile

spec: alaska
	@bash tools/build_spec.sh $(SPECTAR)

clean:
	rm -rf build .*.o*


defconfig:
	@rm -f .config
	@echo "Using default configuration"
	@echo "q" | env TERM=xterm-256color python3 tools/menuconfig.py >/dev/null

cfg: menuconfig
menuconfig:
	@python3 tools/menuconfig.py

nicktest: alaska
	@tools/acc test/nick.c -O3 --keep-ir -o build/nick

notebook:
	jupyter notebook .


redis: alaska
	$(MAKE) -C test/redis

build/lua: alaska
	local/bin/alaska -O2 -b -k test/lua/onelua.c -o $@

docker:
	docker build -t alaska .
	docker run -it --rm alaska bash


FORCE: # anything you want to force, depend on this
