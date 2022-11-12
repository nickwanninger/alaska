# Compile the runtime with some special flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2 -g3 -Wall -Wno-reorder -Wextra -Wno-format -Wno-ignored-attributes -Wno-unused-parameter -Wno-sign-compare ")
set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -fno-exceptions -fno-rtti -nostdlib++ ")

include_directories(
	include/
)

set(SRC_FILES
	# src/set.c
	# src/rbtree.c
	src/runtime.cpp
	src/Arena.cpp
	src/translate.c
)

add_definitions(-D_GNU_SOURCE -D_REENTRANT)
add_library(alaska SHARED ${SRC_FILES})
set_target_properties(alaska PROPERTIES LINKER_LANGUAGE CXX)
install(TARGETS alaska RUNTIME)