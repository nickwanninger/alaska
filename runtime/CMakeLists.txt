add_link_options("-fuse-ld=lld")

set(WARNINGS " -Wno-reorder -Wno-format -Wno-ignored-attributes -Wno-unused-parameter -Wno-sign-compare -Wno-implicit-fallthrough")

# Compile the runtime with some special flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -fdeclspec ${WARNINGS} ")
set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -Wno-deprecated -fno-exceptions -fno-rtti -nostdlib++ ")

add_definitions(-D_GNU_SOURCE -D_REENTRANT)
include_directories(include/)

# HACK: this just makes it work
set(CMAKE_C_COMPILER_ID "Clang")
set(CMAKE_CXX_COMPILER_ID "Clang")


set(SRC_FILES
	src/jemalloc/jemalloc.c
	src/jemalloc/atomic.c
	src/jemalloc/arena.c
	src/jemalloc/base.c
	src/jemalloc/bitmap.c
	src/jemalloc/chunk.c
	src/jemalloc/chunk_dss.c
	src/jemalloc/chunk_mmap.c
	src/jemalloc/ckh.c
	src/jemalloc/ctl.c
	src/jemalloc/extent.c
	src/jemalloc/hash.c
	src/jemalloc/huge.c
	src/jemalloc/mb.c
	src/jemalloc/mutex.c
	src/jemalloc/nstime.c
	src/jemalloc/pages.c
	src/jemalloc/prng.c
	src/jemalloc/prof.c
	src/jemalloc/quarantine.c
	src/jemalloc/rtree.c
	src/jemalloc/stats.c
	src/jemalloc/spin.c
	src/jemalloc/tcache.c
	src/jemalloc/ticker.c
	src/jemalloc/tsd.c
	src/jemalloc/util.c
	src/jemalloc/witness.c
	src/jemalloc/valgrind.c
)
add_library(alaska_allocator STATIC ${SRC_FILES})

set(SRC_FILES
	src/runtime.c  # the core runtime
	src/fallback.c # fallback to segfault+emulate
	src/compat.c   # libc functions that we wrap
)


message(STATUS "COMPILER ID: ${CMAKE_CXX_COMPILER_ID}")

add_library(alaska_library STATIC ${SRC_FILES})
target_include_directories(alaska_library PUBLIC include/)
set_target_properties(alaska_library PROPERTIES LINKER_LANGUAGE C)

install(FILES include/alaska.h DESTINATION include)



install(
  TARGETS alaska_allocator
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  PRIVATE_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})


install(
  TARGETS alaska_library
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  PRIVATE_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

llvmir_attach_bc_target(
  TARGET alaska_bitcodes
  DEPENDS alaska_library)

llvmir_attach_link_target(
  TARGET alaska
  DEPENDS alaska_bitcodes
  OUTPUT_DIR ${CMAKE_INSTALL_LIBDIR})

add_dependencies(alaska alaska_library)

set_property(TARGET alaska_bitcodes PROPERTY EXCLUDE_FROM_ALL OFF)
set_property(TARGET alaska PROPERTY EXCLUDE_FROM_ALL OFF)
