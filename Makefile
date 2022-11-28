# .ONESHELL:
.DEFAULT_GOAL := alaska

ROOT=$(shell pwd)
export PATH:=$(ROOT)/local/bin:$(PATH)
export PATH:=$(ROOT)/install/bin:$(PATH)
export LD_LIBRARY_PATH:=$(ROOT)/local/lib:$(LD_LIBRARY_PATH)
export LD_LIBRARY_PATH:=$(ROOT)/install/lib:$(LD_LIBRARY_PATH)

# include .config

BUILD=build

$(BUILD)/Makefile:
	mkdir -p $(BUILD)
	cd $(BUILD) && cmake ../ -DCMAKE_INSTALL_PREFIX:PATH=$(ROOT)/local

alaska: local/bin/clang $(BUILD)/Makefile
	@$(MAKE) --no-print-directory install -C build
	@cp build/compile_commands.json .



local/bin/clang:
	tools/build_deps.sh

deps: local/bin/clang

sanity: alaska
	@local/bin/alaska test/sanity.c -o build/sanity
	@build/sanity

test: alaska
	@make -C build verbose_test --no-print-directory

GAP_SUITE:=bfs bc
GAP_BINS:=$(foreach v,$(GAP_SUITE),build/bench/gap.$(v).alaska)

.PHONY: alaska all gap

.SECONDARY: $(GAP_BINS) $(foreach v,$(GAP_SUITE),build/bench/gap.$(v).base)

clean:
	rm -rf build .*.o*

cfg: menuconfig
menuconfig:
	@python3 tools/menuconfig.py


nicktest: alaska
	@tools/acc test/nick.c -O3 --keep-ir -o build/nick

notebook:
	jupyter notebook .


