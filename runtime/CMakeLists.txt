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
)

find_library(LIBUNWIND_LIBRARIES NAMES unwind )
message(STATUS "UNWIND: ${LIBUNWIND_LIBRARIES}")



# The alaska library is broken up into two parts: the core, and the runtime.
# The core provides the main structures and functions to manage the alaska state.
# The runtime provides a binding that applications link against.

##########################################################################################
# This defines the core

add_library(alaska_core STATIC
	core/HandleTable.cpp
	core/Runtime.cpp
	core/Logger.cpp
	core/Heap.cpp
	core/SizeClass.cpp

	# C++ glue stuff, since we don't link against libc++ in the core library.
	core/glue.cpp
)

target_include_directories(alaska_core PUBLIC include/)
set_target_properties(alaska_core PROPERTIES LINKER_LANGUAGE C)
target_link_libraries(alaska_core dl pthread ${LIBUNWIND_LIBRARIES})


install(
  TARGETS alaska_core
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  PRIVATE_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})


##########################################################################################
# This defines the runtime

add_library(alaska SHARED
	rt/init.cpp
	rt/halloc.cpp
	rt/threads.cpp
	rt/barrier.cpp
)

target_include_directories(alaska PUBLIC include/)
set_target_properties(alaska PROPERTIES LINKER_LANGUAGE C)
target_link_libraries(alaska alaska_core dl pthread ${LIBUNWIND_LIBRARIES})


install(
  TARGETS alaska
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  PRIVATE_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})







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

add_library(rsstracker SHARED extra/rsstracker.c)
target_link_libraries(rsstracker dl pthread)
install(TARGETS rsstracker)


add_executable(htlb extra/htlb.c)
target_link_libraries(htlb dl pthread)
install(TARGETS htlb)

install(FILES include/alaska.h DESTINATION include)


if (ALASKA_ENABLE_TESTING)

	find_package(GTest)

	IF (${GTest_FOUND})
   MESSAGE(STATUS "Found Google Test.")
	ELSE (${GTest_FOUND})
		MESSAGE(STATUS "Could not locate Google Test - Installing with FetchContent.")
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
	ENDIF (${GTest_FOUND})



  enable_testing()
  include(GoogleTest)

	set(CMAKE_C_COMPILER clang)

	add_executable(
		alaska_test
		test/runtime_test.cpp
		test/pagemanager_test.cpp
		test/heap_test.cpp
		test/sizeclass_test.cpp
	)

	target_link_libraries(
		alaska_test
		GTest::gtest_main
		alaska_core dl pthread ${LIBUNWIND_LIBRARIES}
	)

	gtest_discover_tests(alaska_test)


endif(ALASKA_ENABLE_TESTING)
