.DEFAULT_GOAL := redis
.PHONY: redis

src:
	git clone git@github.com:redis/redis.git src --recursive


redis: #src/src/redis-server
	$(MAKE) -C src USE_JEMALLOC=no CC=gclang OPTIMIZATION=-O2 "CFLAGS=-gdwarf-4 -Wno-strict-prototypes $(shell alaska-config --cflags)" "LDFLAGS=$(shell alaska-config --ldflags)"

	@mkdir -p build
	@echo " EX redis-server"
	@get-bc -b -o build/redis-server.bc src/src/redis-server
	@echo " TX build/redis-server.bc" 
	@llvm-dis build/redis-server.bc -o build/redis-server-in.ll
	@ALASKA_NO_REPLACE_MALLOC=1 alaska -O1 build/redis-server.bc -o build/redis-server -ldl

clean:
	$(MAKE) -C src clean

