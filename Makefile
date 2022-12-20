#.ONESHELL:
.DEFAULT_GOAL := alaska

ROOT=$(shell pwd)
export PATH:=$(ROOT)/local/bin:$(PATH)
export LD_LIBRARY_PATH:=$(ROOT)/local/lib:$(LD_LIBRARY_PATH)

-include .config

BUILD=build


BUILD_REQ=$(BUILD)/Makefile

ifdef ALASKA_USE_NINJA
BUILD_REQ=$(BUILD)/build.ninja
endif

$(BUILD)/Makefile:
	mkdir -p $(BUILD)
	cd $(BUILD) && cmake ../ -DCMAKE_INSTALL_PREFIX:PATH=$(ROOT)/local


$(BUILD)/build.ninja:
	mkdir -p $(BUILD)
	cd $(BUILD) && cmake ../ -G Ninja -DCMAKE_INSTALL_PREFIX:PATH=$(ROOT)/local

alaska: .config local/bin/clang $(BUILD_REQ)
	@cd $(BUILD) && cmake --build . --target install --config Debug
	@cp build/compile_commands.json .



local/bin/clang:
	tools/build_deps.sh

deps: local/bin/clang

sanity: alaska
	@local/bin/alaska test/unit/sanity.c -o build/sanity
	@build/sanity

test: alaska
	@python3 tools/unittest.py


.PHONY: alaska all bench bench/nas libc

	
# targets to build benchmarks
NAS_BENCHMARKS := bench/nas/ft bench/nas/mg bench/nas/sp bench/nas/lu bench/nas/bt bench/nas/is bench/nas/ep bench/nas/cg
GAP_BENCHMARKS := bench/gap/bfs bench/gap/bc bench/gap/cc bench/gap/cc_sv bench/gap/pr bench/gap/pr_spmv bench/gap/sssp


NAS_CLASS=A
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



redis: alaska
	$(MAKE) -C test/redis

lua: alaska
	local/bin/alaska -b -k test/lua/onelua.c -o build/lua
#
# libc/src:
# 	mkdir -p libc
# 	# git checkout azanella/clang
# 	git clone git@github.com:bminor/glibc.git --depth 1 $@
#
# libc/build/Makefile: libc/src
# 	mkdir -p libc/build
# 	cd libc/build && unset LD_LIBRARY_PATH && CC=clang CXX=clang++ ../src/configure \
# 		--with-lld \
# 		--verbose \
# 		--with-default-link \
# 		--disable-multi-arch \
# 		--disable-sanity-checks \
# 		--prefix=$(ROOT)/libc/local
#
# libc/build/libc.a: libc/build/Makefile
# 	$(MAKE) -C libc/build
#
# libc/build/libc.bc: libc/build/libc.a
# 	get-bc -b $<
# 	@cp $^ $@
# 	@alaska-transform $@
# 	@llvm-dis $@
#
# local/lib/libc.o: libc/build/lib/libc.a
#
# libc: libc/build/Makefile
#

musl:
	git clone git://git.musl-libc.org/musl --depth 1 musl

musl/lib/libc.a: musl
	cd musl && CC=gclang ./configure --prefix=$(PWD)/local --syslibdir=$(PWD)/local/lib
	CC=gclang $(MAKE) -C musl install

musl/lib/lib%.a.bc: musl/lib/libc.a # everyone relies on libc.a, as we build all of them at the same time
	get-bc -b musl/lib/lib$*.a 2>/dev/null # ignore the warnings for asm files

build/lib%.bc: musl/lib/lib%.a.bc
	@echo " TX lib$*"
	@cp musl/lib/lib$*.a.bc build/lib$*.bc
	@alaska-transform build/lib$*.bc
	@llvm-dis build/lib$*.bc

# code to build libc with alaska :)
local/lib/lib%.o: alaska build/lib%.bc
	@echo " CC lib$*"
	@clang -O3 -c -o build/lib$*.o build/lib$*.bc
	@cp build/lib$*.o local/lib/
	@cp musl/lib/lib$*.a local/lib/
libc: local/lib/libc.o
