.DEFAULT_GOAL := redis
.PHONY: redis compilation

# redis caches OPT in src/src/.make-settings, so pass OPT explicitly to override it.
# -disable-llvm-passes: unoptimized IR without -O0's optnone, keeping TBAA.
OPT := -O2 -Xclang -disable-llvm-passes


src:
	git clone git@github.com:redis/redis.git src
	git -C src checkout 131d95f203351b19f307072e6582fda91e149580 

compilation: src FORCE
	$(MAKE) -C src USE_JEMALLOC=no CC=gclang "OPTIMIZATION=$(OPT)" "OPT=$(OPT)" "CFLAGS=-gdwarf-4 -fPIE -fpic -Wno-strict-prototypes $(shell alaska-config --cflags)" "LDFLAGS=$(shell alaska-config --ldflags)"

bin/redis-server.bc: src compilation
	@mkdir -p bin
	@echo " EX redis-server"
	@get-bc -b -o bin/redis-server.bc src/src/redis-server
	opt -O3 bin/redis-server.bc -o bin/redis-server.bc
	opt -strip-debug bin/redis-server.bc -o bin/redis-server.bc

	@opt -passes=gvn,instnamer bin/redis-server.bc -o bin/redis-server.bc
	@llvm-dis bin/redis-server.bc




bin/redis-server-alaska: bin/redis-server.bc
	cp bin/redis-server.bc bin/alaska.bc
	alaska-transform bin/alaska.bc -o bin/alaska-transformed.bc
	llc -O3 bin/alaska-transformed.bc --relocation-model=pic --filetype=obj -o bin/alaska.o
	# llc bin/alaska-transformed.bc --relocation-model=pic --filetype=obj -o bin/alaska.o
	clang bin/alaska.o $(shell alaska-config --ldflags --cflags) -lm -ldl -o bin/redis-server-alaska


bin/redis-server-baseline: bin/redis-server.bc
	cp bin/redis-server.bc bin/baseline.bc
	alaska-transform --baseline bin/baseline.bc -o bin/baseline-transformed.bc
	llc -O3 bin/baseline-transformed.bc --relocation-model=pic --filetype=obj -o bin/baseline.o
	clang bin/baseline.o $(shell alaska-config --cflags) -lm -ldl -o bin/redis-server-baseline


redis: bin/redis-server-baseline bin/redis-server-alaska

clean:
	rm -rf bin
	$(MAKE) -C src clean


FORCE:
