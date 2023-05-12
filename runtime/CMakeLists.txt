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




set(SRC_FILES
	# The core of the runtime
	lib/runtime.c
	lib/table.c
	lib/translate.c
	lib/prefetch.c
	lib/halloc.c
	lib/barrier.c
	lib/rbtree.c
	lib/glue.cpp # Stuff to support a nostdlibc++ environment

	# libc functions that we reimplement to avoid
	# glibc doing terrible things with AVX512
	lib/compat.c
)



if (ALASKA_SERVICE_ANCHORAGE)

	include_directories(
		services/anchorage/heaplayers
		)
	set(SRC_FILES ${SRC_FILES}
		services/anchorage/Anchorage.cpp
		services/anchorage/Swapper.cpp
	)
endif()

if (ALASKA_SERVICE_NONE)
	set(SRC_FILES ${SRC_FILES}
		services/none/none.c
	)
endif()


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

add_bitcode(alaska_translate SOURCES lib/translate.c)
add_bitcode(alaska_prefetch SOURCES lib/prefetch.c)
add_bitcode(alaska_bootstrap SOURCES lib/bootstrap.c)
add_bitcode(alaska_compat SOURCES lib/compat.c)


target_link_libraries(alaska alaska_translate_native)


add_library(rsstracker SHARED extra/rsstracker.c)
target_link_libraries(rsstracker dl)



install(FILES include/alaska.h DESTINATION include)
install(FILES include/alaska/config.h DESTINATION include/alaska)

install(
  TARGETS alaska
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  PRIVATE_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

