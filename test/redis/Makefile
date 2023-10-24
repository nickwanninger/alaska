.DEFAULT_GOAL := redis
.PHONY: redis

src:
	git clone git@github.com:redis/redis.git src --recursive


src/src/redis-server: src
	$(MAKE) -C src USE_JEMALLOC=no CC=gclang OPTIMIZATION=-O2 "CFLAGS=-gdwarf-4 -Wno-strict-prototypes $(shell alaska-config --cflags)" "LDFLAGS=$(shell alaska-config --ldflags)"

build/redis-server.bc: src/src/redis-server
	@mkdir -p build
	@echo " EX redis-server"
	@get-bc -b -o build/redis-server.bc src/src/redis-server
	@llvm-dis build/redis-server.bc

redis: build/redis-server.bc FORCE
	cp build/redis-server.bc build/alaska.bc
	alaska-transform build/alaska.bc
	llvm-dis build/alaska.bc
	llc -O3 build/alaska.bc --relocation-model=pic --filetype=obj -o build/alaska.o
	clang build/alaska.o $(shell alaska-config --ldflags --cflags) -ldl -o build/redis-server

	cp build/redis-server.bc build/baseline.bc
	ALASKA_COMPILER_BASELINE=true alaska-transform build/baseline.bc
	llvm-dis build/baseline.bc
	llc -O3 build/baseline.bc --relocation-model=pic --filetype=obj -o build/baseline.o
	clang build/baseline.o $(shell alaska-config --ldflags --cflags) -ldl -o build/redis-server.base
	# @#ALASKA_NO_REPLACE_MALLOC=1 alaska -O1 build/redis-server.bc -o build/redis-server -ldl
	# @alaska -k -b -O2 build/redis-server.bc -o build/redis-server -ldl

clean:
	$(MAKE) -C clean


FORCE:
