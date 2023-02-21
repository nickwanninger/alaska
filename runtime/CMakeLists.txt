add_link_options("-fuse-ld=lld")

set(WARNINGS " -Wall -Wno-reorder -Wno-format -Wno-unused-function -Wno-ignored-attributes -Wno-unused-parameter -Wno-sign-compare -Wno-implicit-fallthrough -Wno-gnu-null-pointer-arithmetic")

# Compile the runtime with some special flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fopenmp -O3 -gdwarf-4 -fdeclspec ${WARNINGS} ")
set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -Wno-deprecated -fno-exceptions -fno-rtti ")
set(CMAKE_C_COMPILER_ID "Clang")
set(CMAKE_CXX_COMPILER_ID "Clang")
SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)


add_definitions(-D_GNU_SOURCE -D_REENTRANT)
include_directories(
	include/
	emu/include/
)




set(SRC_FILES
	# The core of the runtime
	lib/runtime.c
	lib/table.c
	lib/lock.c
	lib/halloc.c
	lib/barrier.c
	lib/rbtree.c
	lib/trace.c
	lib/vec.c
	lib/classification.c

	# libc functions that we reimplement to avoid
	# glibc doing terrible things with AVX512
	# (The emulator cannot handle those instructions)
	lib/compat.c
)


if (ALASKA_SIM_MODE)
	set(SRC_FILES ${SRC_FILES} lib/sim.c)
endif()

if (ALASKA_SERVICE_ANCHORAGE)
	set(SRC_FILES ${SRC_FILES}
		# written using nostd c++
		services/anchorage/anchorage.cpp
		services/anchorage/allocator.cpp

		# # link jemalloc as well
		# jemalloc/arena.c jemalloc/background_thread.c jemalloc/base.c jemalloc/bin.c
		# jemalloc/bitmap.c jemalloc/ckh.c jemalloc/ctl.c jemalloc/div.c jemalloc/extent.c
		# jemalloc/extent_dss.c jemalloc/extent_mmap.c jemalloc/hash.c jemalloc/hook.c jemalloc/jemalloc.c
		# jemalloc/large.c jemalloc/log.c jemalloc/malloc_io.c jemalloc/mutex.c jemalloc/mutex_pool.c
		# jemalloc/nstime.c jemalloc/pages.c jemalloc/prng.c jemalloc/prof.c jemalloc/rtree.c
		# jemalloc/safety_check.c jemalloc/sc.c jemalloc/stats.c jemalloc/sz.c jemalloc/tcache.c
		# jemalloc/test_hooks.c jemalloc/ticker.c jemalloc/tsd.c jemalloc/witness.c
	)
endif()

if (ALASKA_SERVICE_NONE)
	set(SRC_FILES ${SRC_FILES} services/none/none.c)
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
	add_subdirectory(emu)
	target_link_libraries(alaska_core unicorn)
endif()




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
	set_target_properties(${name}_native
		PROPERTIES LINKER_LANGUAGE C
		PUBLIC_HEADER "${HEADER_FILES}"
		PRIVATE_HEADER "${PRIVATE_HEADER_FILES}")

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


target_link_libraries(alaska_core alaska_lock_native)



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



add_executable(alaska-sim bin/sim.c)
target_link_libraries(alaska-sim alaska_core) # alaska_support
set_target_properties(alaska-sim PROPERTIES INSTALL_RPATH_USE_LINK_PATH TRUE)
install(
  TARGETS alaska-sim
  ARCHIVE DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_BINDIR}
  PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  PRIVATE_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
