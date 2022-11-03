# Compile the runtime with some special flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -g3 -Wall -Wno-reorder -Wextra -Wno-format -Wno-ignored-attributes -Wno-unused-parameter -Wno-sign-compare ")

include_directories(
	include/
)

set(SRC_FILES
	src/rbtree.c
	src/alaska.c
	src/translate.c
)

add_definitions(-D_GNU_SOURCE -D_REENTRANT)

# file(GLOB JEMALLOC_SRC ${CMAKE_CURRENT_SOURCE_DIR}/jemalloc/*.c)
add_library(alaska SHARED ${SRC_FILES})

install(
	TARGETS alaska
	RUNTIME
)
