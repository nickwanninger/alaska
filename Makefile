.ONESHELL:
.DEFAULT_GOAL := texas

# include .config

BUILD=build

texas:
	@tools/build.sh


test: texas
	@make -C build verbose_test --no-print-directory

GAP_SUITE:=bfs bc
GAP_BINS:=$(foreach v,$(GAP_SUITE),build/bench/gap.$(v).texas)

.PHONY: texas all gap

.SECONDARY: $(GAP_BINS) $(foreach v,$(GAP_SUITE),build/bench/gap.$(v).base)
	

clean:
	rm -rf build

cfg: menuconfig
menuconfig:
	@python3 tools/menuconfig.py


nicktest: texas
	@tools/acc test/nick.c -O3 --keep-ir -o build/nick
