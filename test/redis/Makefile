.DEFAULT_GOAL := redis
.PHONY: redis compilation


src:
	git clone git@github.com:redis/redis.git src --depth 1

compilation: src FORCE
	$(MAKE) -C src USE_JEMALLOC=no CC=gclang OPTIMIZATION=-O0 "CFLAGS=-gdwarf-4 -Wno-strict-prototypes $(shell alaska-config --cflags)" "LDFLAGS=$(shell alaska-config --ldflags)"

bin/redis-server.bc: src compilation
	@mkdir -p bin
	@echo " EX redis-server"
	@get-bc -b -o bin/redis-server.bc src/src/redis-server
	@llvm-dis bin/redis-server.bc



bin/redis-server-alaska: bin/redis-server.bc
	cp bin/redis-server.bc bin/alaska.bc
	opt -strip-debug bin/alaska.bc -o bin/alaska.bc
	opt -O3 bin/alaska.bc -o bin/alaska.bc
	ALASKA_NO_STRICT_ALIAS=YES alaska-transform bin/alaska.bc -o bin/alaska-transformed.bc
	llc -O3 bin/alaska-transformed.bc --relocation-model=pic --filetype=obj -o bin/alaska.o
	clang bin/alaska.o $(shell alaska-config --ldflags --cflags) -lm -ldl -o bin/redis-server-alaska


bin/redis-server-baseline: bin/redis-server.bc
	cp bin/redis-server.bc bin/baseline.bc
	opt -strip-debug bin/baseline.bc -o bin/baseline.bc
	opt -O3 bin/baseline.bc -o bin/baseline.bc
	alaska-transform --baseline bin/baseline.bc -o bin/baseline-transformed.bc
	llc -O3 bin/baseline-transformed.bc --relocation-model=pic --filetype=obj -o bin/baseline.o
	clang bin/baseline.o $(shell alaska-config --cflags) -lm -ldl -o bin/redis-server-baseline


redis: bin/redis-server-baseline bin/redis-server-alaska

clean:
	rm -rf bin
	$(MAKE) -C src clean


FORCE:
