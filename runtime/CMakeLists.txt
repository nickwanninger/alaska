# add_link_options("-fuse-ld=lld")

set(WARNINGS " -Wall -Wextra -Wno-reorder -Wno-format -Wno-unused-function -Wno-ignored-attributes -Wno-unused-parameter -fno-omit-frame-pointer")

# Compile the runtime with some special flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -gdwarf-4 -fdeclspec ${WARNINGS} ")
set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -std=c++20 -Wno-deprecated -fno-exceptions -fno-rtti ") 
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -nodefaultlibs -nostdlib++ -nostdlib ") 
set(CMAKE_C_COMPILER_ID "Clang")
set(CMAKE_CXX_COMPILER_ID "Clang")
SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

# Do NOT link libc++ implicitly
list(REMOVE_ITEM CMAKE_CXX_IMPLICIT_LINK_LIBRARIES stdc++)

add_definitions(-D_GNU_SOURCE -D_REENTRANT)
include_directories(
	include/
)


# Alaska and it's services are written in no-std c++. This is to
# enable some of the nice compile-time features that c++ has,
# without the runtime overheads of things like RTTI or exceptions.
# As such, the entire runtime is written in a "C with classes"
# style, and attempts to avoid things like virtual functions
# wherever possible.

set(SRC_FILES
	# The core of the runtime
	lib/runtime.cpp
	lib/table.cpp
	lib/translate.cpp
	lib/prefetch.cpp
	lib/halloc.cpp
	lib/barrier.cpp
	lib/glue.cpp # Stuff to support a nostdlibc++ environment

	# libc functions that we reimplement to avoid
	# glibc doing terrible things with AVX512
	lib/compat.c
)



if (ALASKA_SERVICE_ANCHORAGE)
	# Anchorage's allocator relies on a modified version of
	# Heap-Layers, which we have vendored here:
	include_directories(services/anchorage/heaplayers)
	set(SRC_FILES ${SRC_FILES}
		services/anchorage/Anchorage.cpp
		services/anchorage/Block.cpp
		services/anchorage/Chunk.cpp
		services/anchorage/Defragmenter.cpp
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


add_library(alaska SHARED ${SRC_FILES})
# add_library(alaska STATIC ${SRC_FILES})
target_include_directories(alaska PUBLIC include/)
set_target_properties(alaska PROPERTIES LINKER_LANGUAGE C)
target_link_libraries(alaska dl pthread)


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



set(SRC_FILES extra/lifetime.c)
add_library(lifetimemalloc SHARED ${SRC_FILES})
target_link_libraries(lifetimemalloc dl)

add_bitcode(alaska_translate SOURCES lib/translate.cpp)
add_bitcode(alaska_prefetch SOURCES lib/prefetch.cpp)
add_bitcode(alaska_compat SOURCES lib/compat.c)


add_bitcode(alaska_stub SOURCES
	stub/stub.c
	stub/memcpy.c)




target_link_libraries(alaska alaska_translate_native)


add_library(rsstracker SHARED extra/rsstracker.c)
target_link_libraries(rsstracker dl pthread)



install(FILES include/alaska.h DESTINATION include)
install(FILES include/alaska/config.h DESTINATION include/alaska)

install(
  TARGETS alaska
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  PRIVATE_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

