
set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} -O3 -g3 -Wall -Wno-reorder -Wextra -Wno-format -Wno-ignored-attributes -Wno-unused-parameter -Wno-sign-compare ")
# set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -std=c++17 -g -march=native -mcmodel=large")



# essential files
set(SRC_FILES
	set.c
	rbtree.c
	translate.c
)


# set(SRC_FILES ${SRC_FILES}
# 	jemalloc/jemalloc.c
# 	jemalloc/atomic.c
# 	jemalloc/arena.c
# 	jemalloc/base.c
# 	jemalloc/bitmap.c
# 	jemalloc/chunk.c
# 	jemalloc/chunk_dss.c
# 	jemalloc/chunk_mmap.c
# 	jemalloc/ckh.c
# 	jemalloc/ctl.c
# 	jemalloc/extent.c
# 	jemalloc/hash.c
# 	jemalloc/huge.c
# 	jemalloc/mb.c
# 	jemalloc/mutex.c
# 	jemalloc/nstime.c
# 	jemalloc/pages.c
# 	jemalloc/prng.c
# 	jemalloc/prof.c
# 	jemalloc/quarantine.c
# 	jemalloc/rtree.c
# 	jemalloc/stats.c
# 	jemalloc/spin.c
# 	jemalloc/tcache.c
# 	jemalloc/ticker.c
# 	jemalloc/tsd.c
# 	jemalloc/util.c
# 	jemalloc/witness.c
# 	jemalloc/valgrind.c
# )

add_definitions(-D_GNU_SOURCE -D_REENTRANT)

# file(GLOB JEMALLOC_SRC ${CMAKE_CURRENT_SOURCE_DIR}/jemalloc/*.c)
add_library(alaska SHARED ${SRC_FILES})

install(
	TARGETS alaska
	RUNTIME
)
