add_link_options("-fuse-ld=lld")

set(WARNINGS " -Wno-reorder -Wno-format -Wno-ignored-attributes -Wno-unused-parameter -Wno-sign-compare -Wno-implicit-fallthrough")

# Compile the runtime with some special flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2 -gdwarf-4 -fdeclspec ${WARNINGS} ")
set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -Wno-deprecated -fno-exceptions -fno-rtti -nostdlib++ ")

add_definitions(-D_GNU_SOURCE -D_REENTRANT)
include_directories(
	include/
	emu/include/
)



if(ALASKA_CORRECTNESS_EMULATOR)
add_subdirectory(emu)
endif()

# HACK: this just makes it work
set(CMAKE_C_COMPILER_ID "Clang")
set(CMAKE_CXX_COMPILER_ID "Clang")




# The alaska support library implements some of the larger, performance
# insensitive parts of alaska. Primarially, it contains the memory allocator
# (je_malloc), as compiling it into bitcode does not benefit anyone - it just
# takes more time
# set(SRC_FILES
# )
# add_library(alaska_support SHARED ${SRC_FILES})
# target_link_libraries(alaska_support pthread) # requires pthread


set(SRC_FILES
	# The core runtime implementation
	src/runtime.c
	src/table.c
	src/lock.c
	src/halloc.c
	src/barrier.c
	# src/classify.c
	src/rbtree.c
	src/trace.c

	# Replacing c++ new/delete in a more reasonable way
	# src/glue.cpp

	# libc functions that we wrap
	src/compat.c   
	# src/malloc.c   
	
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


if (ALASKA_SIM_MODE)
set(SRC_FILES ${SRC_FILES} src/sim.c)
endif()

if (ALASKA_PERSONALITY_ANCHORAGE)
set(SRC_FILES ${SRC_FILES} src/personality/anchorage.c)
endif()
if (ALASKA_PERSONALITY_NONE)
set(SRC_FILES ${SRC_FILES} src/personality/none.c)
endif()

if(ALASKA_CORRECTNESS_EMULATOR)
	# The correctness fallback (emulate using TCG)
	set(SRC_FILES ${SRC_FILES} src/emulator.c)
endif()

add_library(alaska_core SHARED ${SRC_FILES})
target_include_directories(alaska_core PUBLIC include/)
set_target_properties(alaska_core PROPERTIES LINKER_LANGUAGE C)
target_link_libraries(alaska_core pthread)


if(ALASKA_CORRECTNESS_EMULATOR)
target_link_libraries(alaska_core unicorn)
endif()



add_library(alaska_inline_lock_native STATIC src/lock.c)
target_include_directories(alaska_inline_lock_native PUBLIC include/)
set_target_properties(alaska_inline_lock_native PROPERTIES LINKER_LANGUAGE C)


SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)


add_executable(alaska-sim bin/sim.c)
target_link_libraries(alaska-sim alaska_core) # alaska_support
set_target_properties(alaska-sim PROPERTIES INSTALL_RPATH_USE_LINK_PATH TRUE)


install(FILES include/alaska.h DESTINATION include)
install(FILES include/classes.inc DESTINATION include)
install(FILES include/alaska.hpp DESTINATION include)

# install(
#   TARGETS alaska_support
#   ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
#   LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
#   PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
#   PRIVATE_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})


install(
  TARGETS alaska_core
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  PRIVATE_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})


install(
  TARGETS alaska-sim
  ARCHIVE DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_BINDIR}
  PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  PRIVATE_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})




add_library(frack SHARED lib/frack.c)
target_link_libraries(frack dl)
install(
  TARGETS frack
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  PRIVATE_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# install(
#   TARGETS alaska_jemalloc
#   ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
#   LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
#   PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
#   PRIVATE_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})


llvmir_attach_bc_target(
  TARGET alaska_inline_lock_bitcodes
  DEPENDS alaska_inline_lock_native)
llvmir_attach_link_target(
  TARGET alaska_inline_lock
  DEPENDS alaska_inline_lock_bitcodes
  OUTPUT_DIR ${CMAKE_INSTALL_LIBDIR})



llvmir_attach_bc_target(
  TARGET alaska_bitcodes
  DEPENDS alaska_core)

llvmir_attach_link_target(
  TARGET alaska
  DEPENDS alaska_bitcodes
  OUTPUT_DIR ${CMAKE_INSTALL_LIBDIR})

add_dependencies(alaska alaska_core)


set_property(TARGET alaska_inline_lock PROPERTY EXCLUDE_FROM_ALL OFF)
set_property(TARGET alaska_bitcodes PROPERTY EXCLUDE_FROM_ALL OFF)
set_property(TARGET alaska PROPERTY EXCLUDE_FROM_ALL OFF)

