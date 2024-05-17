# add_link_options("-fuse-ld=lld")

set(WARNINGS "-Wall -Wextra -Wno-ctad-maybe-unsupported -Wno-cast-qual -Wno-reorder -Wno-format -Wno-unused-function -Wno-ignored-attributes -Wno-unused-parameter -Wno-unused-variable -fno-omit-frame-pointer -Wno-sign-compare -Wno-implicit-fallthrough -Wno-unused-private-field -Wno-covered-switch-default ")

# Compile the runtime with some special flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -gdwarf-4 -fdeclspec ${WARNINGS} ")
set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -std=c++2a -Wno-deprecated -fno-exceptions -fno-rtti ")

# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -nodefaultlibs -nostdlib++ -nostdlib ")
set(CMAKE_C_COMPILER_ID "Clang")
set(CMAKE_CXX_COMPILER_ID "Clang")
SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

# Do NOT link libc++ implicitly
list(REMOVE_ITEM CMAKE_CXX_IMPLICIT_LINK_LIBRARIES stdc++)

add_definitions(-D_GNU_SOURCE -D_REENTRANT)
include_directories(
	include/
  include/heaplayers
)

find_library(LIBUNWIND_LIBRARIES NAMES unwind )
message(STATUS "UNWIND: ${LIBUNWIND_LIBRARIES}")



# Alaska and it's services are written in no-std c++. This is to
# enable some of the nice compile-time features that c++ has,
# without the runtime overheads of things like RTTI or exceptions.
# As such, the entire runtime is written in a "C with classes"
# style, and attempts to avoid things like virtual functions
# wherever possible.

set(SRC_FILES
	# The core of the runtime
	core/core.cpp
	core/table.cpp
  # core/translate.cpp
	core/halloc.cpp
	core/barrier.cpp
	core/glue.cpp # Stuff to support a nostdlibc++ environment


	core/HandleTable.cpp
	core/Runtime.cpp
	core/Logger.cpp

	# libc functions that we reimplement to avoid
	# glibc doing terrible things with AVX512
	core/compat.c
)



if (ALASKA_SERVICE_ANCHORAGE)
	# Anchorage's allocator relies on a modified version of
	# Heap-Layers, which we have vendored here:
	set(SRC_FILES ${SRC_FILES}
		services/anchorage/Anchorage.cpp
		services/anchorage/Block.cpp
    services/anchorage/SubHeap.cpp
		services/anchorage/Swapper.cpp
	)
endif(ALASKA_SERVICE_ANCHORAGE)

if (ALASKA_SERVICE_NONE)
	# If the configuration calls for "no service", then we
	# need to compile simple stubs so the program will work.
	# These stubs will just use the system allocator to make
	# everything work fine.
	set(SRC_FILES ${SRC_FILES} services/none/none.cpp)
endif(ALASKA_SERVICE_NONE)


if (ALASKA_SERVICE_CORDOVA)
	set(SRC_FILES ${SRC_FILES} services/cordova/service.cpp)
endif(ALASKA_SERVICE_CORDOVA)


if (ALASKA_SERVICE_JEMALLOC)
	set(SRC_FILES ${SRC_FILES}
		services/jemalloc/service.cpp
		services/jemalloc/src/jemalloc.c
		services/jemalloc/src/arena.c
		services/jemalloc/src/background_thread.c
		services/jemalloc/src/base.c
		services/jemalloc/src/bin.c
		services/jemalloc/src/bin_info.c
		services/jemalloc/src/bitmap.c
		services/jemalloc/src/buf_writer.c
		services/jemalloc/src/cache_bin.c
		services/jemalloc/src/ckh.c
		services/jemalloc/src/counter.c
		services/jemalloc/src/ctl.c
		services/jemalloc/src/decay.c
		services/jemalloc/src/div.c
		services/jemalloc/src/ecache.c
		services/jemalloc/src/edata.c
		services/jemalloc/src/edata_cache.c
		services/jemalloc/src/ehooks.c
		services/jemalloc/src/emap.c
		services/jemalloc/src/eset.c
		services/jemalloc/src/exp_grow.c
		services/jemalloc/src/extent.c
		services/jemalloc/src/extent_dss.c
		services/jemalloc/src/extent_mmap.c
		services/jemalloc/src/fxp.c
		services/jemalloc/src/san.c
		services/jemalloc/src/san_bump.c
		services/jemalloc/src/hook.c
		services/jemalloc/src/hpa.c
		services/jemalloc/src/hpa_hooks.c
		services/jemalloc/src/hpdata.c
		services/jemalloc/src/inspect.c
		services/jemalloc/src/large.c
		services/jemalloc/src/log.c
		services/jemalloc/src/malloc_io.c
		services/jemalloc/src/mutex.c
		services/jemalloc/src/nstime.c
		services/jemalloc/src/pa.c
		services/jemalloc/src/pa_extra.c
		services/jemalloc/src/pai.c
		services/jemalloc/src/pac.c
		services/jemalloc/src/pages.c
		services/jemalloc/src/peak_event.c
		services/jemalloc/src/prof.c
		services/jemalloc/src/prof_data.c
		services/jemalloc/src/prof_log.c
		services/jemalloc/src/prof_recent.c
		services/jemalloc/src/prof_stats.c
		services/jemalloc/src/prof_sys.c
		services/jemalloc/src/psset.c
		services/jemalloc/src/rtree.c
		services/jemalloc/src/safety_check.c
		services/jemalloc/src/sc.c
		services/jemalloc/src/sec.c
		services/jemalloc/src/stats.c
		services/jemalloc/src/sz.c
		services/jemalloc/src/tcache.c
		services/jemalloc/src/test_hooks.c
		services/jemalloc/src/thread_event.c
		services/jemalloc/src/ticker.c
		services/jemalloc/src/tsd.c
		services/jemalloc/src/witness.c
)
endif(ALASKA_SERVICE_JEMALLOC)


# this is the "object library" target: compiles the sources only once
add_library(alaska_obj OBJECT ${SRC_FILES})
# shared libraries need PIC
set_property(TARGET alaska_obj PROPERTY POSITION_INDEPENDENT_CODE 1)

add_library(alaska SHARED $<TARGET_OBJECTS:alaska_obj>)
add_library(alaska_static STATIC $<TARGET_OBJECTS:alaska_obj>)

target_include_directories(alaska_obj PUBLIC include/)
set_target_properties(alaska PROPERTIES LINKER_LANGUAGE C)

target_link_libraries(alaska dl pthread ${LIBUNWIND_LIBRARIES})


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
    OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR})

  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${name}.bc DESTINATION ${CMAKE_INSTALL_LIBDIR})

	set_property(TARGET ${name} PROPERTY EXCLUDE_FROM_ALL OFF)
endfunction(add_bitcode)



set(SRC_FILES extra/lifetime.c)
add_library(lifetimemalloc SHARED ${SRC_FILES})
target_link_libraries(lifetimemalloc dl)

add_bitcode(alaska_translate SOURCES core/translate.cpp)
add_bitcode(alaska_compat SOURCES core/compat.c)



add_bitcode(alaska_stub SOURCES
	stub/net.c
	stub/dtoa.c
	stub/obstack.c
	stub/stub.c
	stub/memcpy.c
)


target_link_libraries(alaska alaska_translate_native)


target_link_libraries(alaska_static alaska_translate_native)
set_target_properties(alaska_static PROPERTIES OUTPUT_NAME "alaska")


add_library(rsstracker SHARED extra/rsstracker.c)
target_link_libraries(rsstracker dl pthread)
install(TARGETS rsstracker)


add_executable(htlb extra/htlb.c)
target_link_libraries(htlb dl pthread)
install(TARGETS htlb)

install(FILES include/alaska.h DESTINATION include)

install(
  TARGETS alaska
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  PRIVATE_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})


install(
  TARGETS alaska_static
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  PRIVATE_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})


if (ALASKA_ENABLE_TESTING)
  include(FetchContent)
  FetchContent_Declare(
  googletest
    URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  )
  # For Windows: Prevent overriding the parent project's compiler/linker settings
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
	set(INSTALL_GTEST OFF)

  FetchContent_MakeAvailable(googletest)

  enable_testing()
  include(GoogleTest)

	set(CMAKE_C_COMPILER clang)

	add_executable(
		alaska_test
		test/runtime_test.cpp
	)

	target_link_libraries(
		alaska_test
		GTest::gtest_main
		alaska_static dl pthread ${LIBUNWIND_LIBRARIES}
	)

	gtest_discover_tests(alaska_test)


endif(ALASKA_ENABLE_TESTING)
