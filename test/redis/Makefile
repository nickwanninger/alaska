.DEFAULT_GOAL := redis
.PHONY: redis

src:
	git clone git@github.com:redis/redis.git src --recursive

src/src/redis-server: src
	$(MAKE) -C src USE_JEMALLOC=no CC=gclang OPTIMIZATION=-O0 "CFLAGS=-gdwarf-4 -Wno-strict-prototypes"

src/src/redis-server.bc: src/src/redis-server

redis: src/src/redis-server
	@mkdir -p build
	@echo " EX redis-server"
	@get-bc -b -o build/redis-server.bc $<
	@echo " TX build/redis-server.bc" 
	@llvm-dis build/redis-server.bc -o build/redis-server-in.ll
	@alaska build/redis-server.bc -o build/redis-server -ldl

clean:
	$(MAKE) -C src clean

