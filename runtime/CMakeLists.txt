# add_link_options("-fuse-ld=lld")


set(WARNINGS "-Wall -Werror -Wno-multichar -Wno-unused -Wno-unused-function -Wno-attributes ")

# Compile the runtime with some special flags

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -gdwarf-4 ${WARNINGS} -fno-plt")
set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -std=c++2a -Wsign-compare -Wno-deprecated -fno-exceptions -Wno-ctad-maybe-unsupported -Wno-reorder -fno-rtti -Wno-invalid-offsetof ")


# if clang
if(CMAKE_C_COMPILER_ID STREQUAL "Clang")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-vla-cxx-extension -Wno-error=vla-cxx-extension ")
endif()



set(CMAKE_C_FLAGS_DEBUG "-O0")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")



set(CMAKE_C_FLAGS_RELEASE "-O3")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}")


set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)


if(CMAKE_BUILD_TYPE MATCHES "Release")
  add_definitions(-DALASKA_RELEASE_BUILD=1)
endif()



# Do NOT link libc++ implicitly. This just makes it easier to port to new hardware or embedded environments
list(REMOVE_ITEM CMAKE_CXX_IMPLICIT_LINK_LIBRARIES stdc++)
add_definitions(-D_REENTRANT)

if(NOT ALASKA_TRACING)
  add_definitions(-DFTR_NO_TRACE=1)
endif()



include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
)

install(FILES alaska.h yukon.h DESTINATION include)



##########################################################################################
# Core sources

set(CORE_SOURCES "")
file(GLOB_RECURSE CORE_SOURCES
  "alaska/**/*.cpp"
  "alaska/*.cpp"
  "alaska/**/*.c"
  "alaska/*.c"
)


# Static core library, compiled with hidden visibility.
# Each product (yukon, yukon_stub, alaska) links this statically so LTO can
# inline across translation unit boundaries without PLT trampolines.
add_library(alaska_core_impl STATIC ${CORE_SOURCES})
set_property(TARGET alaska_core_impl PROPERTY POSITION_INDEPENDENT_CODE ON)
target_compile_options(alaska_core_impl PRIVATE
  $<$<COMPILE_LANGUAGE:CXX>:-nostdinc++>
  # -fvisibility=hidden
)
target_include_directories(alaska_core_impl PUBLIC include/)


# Public-facing shared library used by alaska_sim_trace and tests (no LTO required).
# No -fvisibility=hidden here — alaska_sim_trace and tests need the C++ class interfaces exported.
add_library(alaska_core SHARED ${CORE_SOURCES})
set_target_properties(alaska_core PROPERTIES SOVERSION ${ALASKA_VERSION})
target_compile_options(alaska_core PRIVATE
  $<$<COMPILE_LANGUAGE:CXX>:-nostdinc++>
)
target_include_directories(alaska_core PUBLIC include/)
target_link_libraries(alaska_core PRIVATE dl pthread ${ZSTD_STATIC_LIBRARIES})

install(
  TARGETS alaska_core
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  PRIVATE_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})



add_library(alaska_sim SHARED
  sim/HTLB.cpp
  sim/TLB.cpp
  sim/StatisticsManager.cpp
)
install(
  TARGETS alaska_sim
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  PRIVATE_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})


##########################################################################################
# LTO support detection

include(CheckIPOSupported)
check_ipo_supported(RESULT ipo_supported OUTPUT ipo_error)
if(ipo_supported)
  message(STATUS "IPO / LTO enabled for release products")
else()
  message(STATUS "IPO / LTO not supported: ${ipo_error}")
endif()


##########################################################################################
# This defines the runtime that compiled code is linked against
if(NOT ALASKA_CORE_ONLY) # -----------------------------------------------------------------------------------

  find_library(LIBUNWIND_LIBRARIES NAMES unwind)
  message(STATUS "UNWIND: ${LIBUNWIND_LIBRARIES}")

  set(ALASKA_RT_SOURCES
    rt/init.cpp
    rt/halloc.cpp
    rt/compat.c
    rt/barrier.cpp
  )

  if(ALASKA_HTLB_SIM)
    list(APPEND ALASKA_RT_SOURCES rt/sim.cpp)
  endif()

  add_library(alaska ${ALASKA_LIBRARY_TYPE} ${ALASKA_RT_SOURCES})

  target_include_directories(alaska PUBLIC include/)
  set_target_properties(alaska PROPERTIES LINKER_LANGUAGE C)
  set_target_properties(alaska PROPERTIES SOVERSION ${ALASKA_VERSION})
  target_link_libraries(alaska PRIVATE alaska_core_impl dl pthread)
  target_compile_options(alaska PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-nostdinc++>)

  if(ipo_supported)
    set_property(TARGET alaska PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
  endif()

  # if unwind is found, define a macro
  if(LIBUNWIND_LIBRARIES)
    target_compile_definitions(alaska PUBLIC -DALASKA_HAS_LIBUNWIND=1)
    target_link_libraries(alaska PRIVATE ${LIBUNWIND_LIBRARIES})
  endif()

  if(ALASKA_HTLB_SIM)
    target_link_libraries(alaska alaska_sim)
  endif()

  install(
    TARGETS alaska
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    PRIVATE_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

  if(ALASKA_CORE_BITCODE)
  function(add_bitcode name)
    cmake_parse_arguments(
      SRC        # prefix of output variables
      ""         # Boolean arguments
      ""         # Single value arguments
      "SOURCES"  # multi value arguments
      ${ARGN}    # arguments of the function to parse, here we take the all original ones
    )

    add_library(${name}_native STATIC ${SRC_SOURCES})

    target_compile_options(${name}_native PRIVATE -O3)
    target_include_directories(${name}_native PUBLIC include/)
    set_target_properties(${name}_native
      PROPERTIES LINKER_LANGUAGE C
      PUBLIC_HEADER "${HEADER_FILES}"
      PRIVATE_HEADER "${PRIVATE_HEADER_FILES}")
    if(ALASKA_ENABLE_COMPILER)
      llvmir_attach_bc_target(
          TARGET  ${name}_bitcodes
          DEPENDS ${name}_native)
      llvmir_attach_link_target(
          TARGET  ${name}
          DEPENDS ${name}_bitcodes
          OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR})

      install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${name}.bc DESTINATION ${CMAKE_INSTALL_LIBDIR})

      set_property(TARGET ${name} PROPERTY EXCLUDE_FROM_ALL OFF)
    endif(ALASKA_ENABLE_COMPILER)
  endfunction(add_bitcode)



  set(SRC_FILES extra/lifetime.c)
  add_library(lifetimemalloc SHARED ${SRC_FILES})
  target_link_libraries(lifetimemalloc dl)


  add_bitcode(alaska_translate SOURCES rt/translate.cpp)
  add_bitcode(alaska_compat SOURCES rt/compat.c)
   add_bitcode(alaska_stub SOURCES
      stub/net.c
      stub/dtoa.c
      stub/obstack.c
      stub/stub.c
      stub/memcpy.c
  )


  target_link_libraries(alaska PRIVATE alaska_translate_native)
  endif(ALASKA_CORE_BITCODE)

endif(NOT ALASKA_CORE_ONLY)




if(ALASKA_YUKON)
  add_library(yukon SHARED yukon/yukon.cpp)

  target_include_directories(yukon PUBLIC include/)
  set_target_properties(yukon PROPERTIES LINKER_LANGUAGE C)
  set_target_properties(yukon PROPERTIES SOVERSION ${ALASKA_VERSION})
  target_link_libraries(yukon PRIVATE alaska_core_impl dl pthread)
  target_compile_options(yukon PRIVATE
    $<$<COMPILE_LANGUAGE:CXX>:-nostdlib -nostdinc++>
    # -fvisibility=hidden
  )

  if(ipo_supported)
    set_property(TARGET yukon PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
  endif()

  install(
    TARGETS yukon
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    PRIVATE_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})


  add_library(yukon_stub SHARED yukon/yukon_stub.cpp)
  target_include_directories(yukon_stub PUBLIC include/)
  set_target_properties(yukon_stub PROPERTIES LINKER_LANGUAGE C)
  set_target_properties(yukon_stub PROPERTIES SOVERSION ${ALASKA_VERSION})
  target_link_libraries(yukon_stub PRIVATE alaska_core_impl dl pthread)
  target_compile_options(yukon_stub PRIVATE
    $<$<COMPILE_LANGUAGE:CXX>:-nostdlib -nostdinc++>
    # -fvisibility=hidden
  )
  target_link_options(yukon_stub PRIVATE -Wl,-Bsymbolic-functions)

  if(ipo_supported)
    set_property(TARGET yukon_stub PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
  endif()

  install(
    TARGETS yukon_stub
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    PRIVATE_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

endif(ALASKA_YUKON)


add_executable(alaska_sim_trace sim/main.cpp)
target_link_libraries(alaska_sim_trace alaska_core alaska_sim)
install(
    TARGETS alaska_sim_trace
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    PRIVATE_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})




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
  file(GLOB_RECURSE TEST_SOURCES "test/*.cpp")


	add_executable(
		alaska_test
    ${TEST_SOURCES}
	)

	target_link_libraries(
		alaska_test
		GTest::gtest_main
    GTest::gmock
    alaska_sim
		alaska_core dl pthread
	)

	gtest_discover_tests(alaska_test)

endif(ALASKA_ENABLE_TESTING)
