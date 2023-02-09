add_link_options("-fuse-ld=lld")

set(WARNINGS " -Wno-reorder -Wno-format -Wno-ignored-attributes -Wno-unused-parameter -Wno-sign-compare -Wno-implicit-fallthrough")

# Compile the runtime with some special flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fopenmp -O2 -gdwarf-4 -fdeclspec ${WARNINGS} ")
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


function(add_bitcode name)
	cmake_parse_arguments(
		SRC        # prefix of output variables
		""         # Boolean arguments
		""         # Single value arguments
		"SOURCES"  # multi value arguments
		${ARGN}    # arguments of the function to parse, here we take the all original ones
		)


	add_library(${name}_native STATIC ${SRC_SOURCES})
	target_include_directories(${name}_native PUBLIC include/)
	set_target_properties(${name}_native PROPERTIES LINKER_LANGUAGE C)

	llvmir_attach_bc_target(
		TARGET  ${name}_bitcodes
		DEPENDS ${name}_native)
	llvmir_attach_link_target(
		TARGET  ${name}
		DEPENDS ${name}_bitcodes
		OUTPUT_DIR ${CMAKE_INSTALL_LIBDIR})

	set_property(TARGET ${name} PROPERTY EXCLUDE_FROM_ALL OFF)
endfunction(add_bitcode)


add_bitcode(alaska_lock SOURCES lib/lock.c)
add_bitcode(alaska_bootstrap SOURCES lib/bootstrap.c)
add_bitcode(alaska_compat SOURCES lib/compat.c)



set(SRC_FILES
	# The core runtime implementation
	lib/runtime.c
	lib/table.c
	lib/lock.c
	lib/halloc.c
	lib/barrier.c
	# lib/classify.c
	lib/rbtree.c
	lib/trace.c

	# libc functions that we reimplement to avoid
	# glibc doing terrible things with AVX512
	# (The emulator cannot handle them)
	lib/compat.c
	
	lib/jemalloc/jemalloc.c
	lib/jemalloc/atomic.c
	lib/jemalloc/arena.c
	lib/jemalloc/base.c
	lib/jemalloc/bitmap.c
	lib/jemalloc/chunk.c
	lib/jemalloc/chunk_dss.c
	lib/jemalloc/chunk_mmap.c
	lib/jemalloc/ckh.c
	lib/jemalloc/ctl.c
	lib/jemalloc/extent.c
	lib/jemalloc/hash.c
	lib/jemalloc/huge.c
	lib/jemalloc/mb.c
	lib/jemalloc/mutex.c
	lib/jemalloc/nstime.c
	lib/jemalloc/pages.c
	lib/jemalloc/prng.c
	lib/jemalloc/prof.c
	lib/jemalloc/quarantine.c
	lib/jemalloc/rtree.c
	lib/jemalloc/stats.c
	lib/jemalloc/spin.c
	lib/jemalloc/tcache.c
	lib/jemalloc/ticker.c
	lib/jemalloc/tsd.c
	lib/jemalloc/util.c
	lib/jemalloc/witness.c
	lib/jemalloc/valgrind.c
)


if (ALASKA_SIM_MODE)
	set(SRC_FILES ${SRC_FILES} lib/sim.c)
endif()

if (ALASKA_PERSONALITY_ANCHORAGE)
	set(SRC_FILES ${SRC_FILES} lib/personality/anchorage.c)
endif()

if (ALASKA_PERSONALITY_NONE)
	set(SRC_FILES ${SRC_FILES} lib/personality/none.c)
endif()

if(ALASKA_CORRECTNESS_EMULATOR)
	# The correctness fallback (emulate using TCG)
	set(SRC_FILES ${SRC_FILES} lib/emulator.c)
endif()

add_library(alaska_core SHARED ${SRC_FILES})
target_include_directories(alaska_core PUBLIC include/)
set_target_properties(alaska_core PROPERTIES LINKER_LANGUAGE C)
target_link_libraries(alaska_core pthread)


if(ALASKA_CORRECTNESS_EMULATOR)
target_link_libraries(alaska_core unicorn)
endif()


SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)


add_executable(alaska-sim bin/sim.c)
target_link_libraries(alaska-sim alaska_core) # alaska_support
set_target_properties(alaska-sim PROPERTIES INSTALL_RPATH_USE_LINK_PATH TRUE)


install(FILES include/alaska.h DESTINATION include)
install(FILES include/alaska/config.h DESTINATION include/alaska)
install(FILES include/classes.inc DESTINATION include)
install(FILES include/alaska.hpp DESTINATION include)

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




# add_library(frack SHARED lib/frack.c)
# target_link_libraries(frack dl)
# install(
#   TARGETS frack
#   ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
#   LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
#   PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
#   PRIVATE_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
