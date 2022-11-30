#.ONESHELL:
.DEFAULT_GOAL := alaska

ROOT=$(shell pwd)
export PATH:=$(ROOT)/local/bin:$(PATH)
export PATH:=$(ROOT)/install/bin:$(PATH)
export LD_LIBRARY_PATH:=$(ROOT)/local/lib:$(LD_LIBRARY_PATH)
export LD_LIBRARY_PATH:=$(ROOT)/install/lib:$(LD_LIBRARY_PATH)

#include.config

BUILD=build

$(BUILD)/Makefile:
	mkdir -p $(BUILD)
	cd $(BUILD) && cmake ../ -DCMAKE_INSTALL_PREFIX:PATH=$(ROOT)/local


$(BUILD)/build.ninja:
	mkdir -p $(BUILD)
	cd $(BUILD) && cmake ../ -G Ninja -DCMAKE_INSTALL_PREFIX:PATH=$(ROOT)/local

alaska: .config local/bin/clang $(BUILD)/build.ninja
	@#$(MAKE) --no-print-directory install -C build
	@ninja install -C build
	@cp build/compile_commands.json .



local/bin/clang:
	tools/build_deps.sh

deps: local/bin/clang

sanity: alaska
	@local/bin/alaska test/sanity.c -o build/sanity
	@build/sanity

test: alaska
	@make -C build verbose_test --no-print-directory





.PHONY: alaska all bench bench/nas

	
# targets to build benchmarks
NAS_BENCHMARKS := bench/nas/ft bench/nas/mg bench/nas/sp bench/nas/lu bench/nas/bt bench/nas/is bench/nas/ep bench/nas/cg
GAP_BENCHMARKS := bench/gap/bfs bench/gap/bc bench/gap/cc bench/gap/cc_sv bench/gap/pr bench/gap/pr_spmv bench/gap/sssp


NAS_CLASS=B
bench/nas/%: alaska
	@mkdir -p bench/nas
	@echo "  CC  " $@
	@$(MAKE) --no-print-directory -C test/npb $* CLASS=$(NAS_CLASS) >/dev/null
	@mv bench/$*.$(NAS_CLASS) bench/nas/$*.base
	@get-bc bench/nas/$*.base >/dev/null
	@local/bin/alaska -O3 bench/nas/$*.base.bc -k -lm -o $@ >/dev/null
	@rm bench/nas/$*.base.bc


bench/gap/%: alaska
	@mkdir -p bench/gap
	@echo "  CC  " $@
	@local/bin/alaska++ -std=c++11 -k -b -O3 -Wall test/gapbs/src/$*.cc -o $@

bench: alaska $(NAS_BENCHMARKS) $(GAP_BENCHMARKS)

clean:
	rm -rf build .*.o*

cfg: menuconfig
menuconfig:
	@python3 tools/menuconfig.py


nicktest: alaska
	@tools/acc test/nick.c -O3 --keep-ir -o build/nick

notebook:
	jupyter notebook .

