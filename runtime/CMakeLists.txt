# add_link_options("-fuse-ld=lld")

set(WARNINGS " -Wall -Wextra -Wno-reorder -Wno-format -Wno-unused-function -Wno-ignored-attributes -Wno-unused-parameter")

# Compile the runtime with some special flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -gdwarf-4 -fdeclspec ${WARNINGS} ")
set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -std=c++20 -Wno-deprecated -nodefaultlibs -fno-exceptions -fno-rtti -nostdlib++ -nostdlib ") 
set(CMAKE_C_COMPILER_ID "Clang")
set(CMAKE_CXX_COMPILER_ID "Clang")
SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

# Do NOT link libc++ implicitly
list(REMOVE_ITEM CMAKE_CXX_IMPLICIT_LINK_LIBRARIES stdc++)

add_definitions(-D_GNU_SOURCE -D_REENTRANT)
include_directories(
	include/
	emu/include/
)




set(SRC_FILES
	# The core of the runtime
	lib/runtime.c
	lib/table.c
	lib/translate.c
	lib/halloc.c
	lib/barrier.c
	lib/rbtree.c
	lib/trace.c
	lib/glue.cpp # Stuff to support a nostdlibc++ environment

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
		services/anchorage/Anchorage.cpp
		services/anchorage/Block.cpp
		services/anchorage/Chunk.cpp
		services/anchorage/Defragmenter.cpp
		services/anchorage/LocalityFactory.cpp
		services/anchorage/Swapper.cpp
		services/anchorage/miniz.c
	)
endif()

if (ALASKA_SERVICE_NONE)
	set(SRC_FILES ${SRC_FILES} services/none/none.c)
endif()

if(ALASKA_CORRECTNESS_EMULATOR)
	# The correctness fallback (emulate using TCG)
	set(SRC_FILES ${SRC_FILES} lib/emulator.c)
endif()

add_library(alaska SHARED ${SRC_FILES})
target_include_directories(alaska PUBLIC include/)
set_target_properties(alaska PROPERTIES LINKER_LANGUAGE C)
target_link_libraries(alaska dl pthread)


if(ALASKA_CORRECTNESS_EMULATOR)
	add_subdirectory(emu)
	target_link_libraries(alaska unicorn)
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


add_bitcode(alaska_translate SOURCES lib/translate.c)
add_bitcode(alaska_bootstrap SOURCES lib/bootstrap.c)
add_bitcode(alaska_compat SOURCES lib/compat.c)


target_link_libraries(alaska alaska_translate_native)



install(FILES include/alaska.h DESTINATION include)
install(FILES include/alaska/config.h DESTINATION include/alaska)

install(
  TARGETS alaska
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  PRIVATE_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})



add_executable(alaska-sim bin/sim.c)
target_link_libraries(alaska-sim alaska) # alaska_support
set_target_properties(alaska-sim PROPERTIES INSTALL_RPATH_USE_LINK_PATH TRUE)
install(
  TARGETS alaska-sim
  ARCHIVE DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_BINDIR}
  PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  PRIVATE_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
