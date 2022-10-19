.ONESHELL:
.DEFAULT_GOAL := alaska

# include .config

BUILD=build

alaska:
	@tools/build.sh


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
