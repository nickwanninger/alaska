# add_link_options("-fuse-ld=lld")

set(WARNINGS "-Wall -Werror -Wno-multichar -Wno-unused -Wno-unused-function -Wno-attributes ")

# Compile the runtime with some special flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -gdwarf-4 ${WARNINGS} ")
set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -std=c++2a -Wno-deprecated -fno-exceptions -Wno-ctad-maybe-unsupported -Wno-reorder -fno-rtti -Wno-invalid-offsetof ")



set(CMAKE_C_FLAGS_DEBUG "-O0")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")



set(CMAKE_C_FLAGS_RELEASE "-O3")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}")


set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)



# Do NOT link libc++ implicitly. This just makes it easier to port to new hardware or embedded environments
list(REMOVE_ITEM CMAKE_CXX_IMPLICIT_LINK_LIBRARIES stdc++)
add_definitions(-D_REENTRANT)



include_directories(
	include/
)

install(FILES include/alaska.h DESTINATION include)


# The alaska library is broken up into two parts: the core, and the runtime.
# The core provides the main structures and functions to manage the alaska state.
# The runtime provides a binding that applications link against.



##########################################################################################
# This defines the core

set(CORE_SOURCES
	core/HandleTable.cpp
	core/Runtime.cpp
	core/Logger.cpp
	core/Heap.cpp
  core/ThreadCache.cpp
	core/SizeClass.cpp
  core/HugeObjectAllocator.cpp

  core/HeapPage.cpp
  core/SizedPage.cpp
  core/LocalityPage.cpp
  core/Localizer.cpp

  core/Utils.cpp

  # liballoc is a simple, small, malloc implementation that we embed
  # so alaska can temporially allocate objects w/o implementing a non-handle
  # allocator as well
  core/liballoc.c

	# C++ glue stuff, since we don't link against libc++ in the core library.
	core/glue.cpp
)



add_library(alaska_core_objects OBJECT ${CORE_SOURCES})
set_property(TARGET alaska_core_objects PROPERTY POSITION_INDEPENDENT_CODE ON)
target_compile_options(alaska_core_objects PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-nostdinc++>)
# target_compile_options(alaska_core_objects PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-nostdlib++ -nostdinc++>)




add_library(alaska_core SHARED $<TARGET_OBJECTS:alaska_core_objects>)
# add_library(alaska_core SHARED ${CORE_SOURCES})


set_target_properties(alaska_core PROPERTIES LINKER_LANGUAGE C)
set_target_properties(alaska_core PROPERTIES SOVERSION ${ALASKA_VERSION})

target_link_libraries(alaska_core dl pthread)




install(
  TARGETS alaska_core
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  PRIVATE_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})


# A static copy of the above library.
add_library(alaska_core_static STATIC $<TARGET_OBJECTS:alaska_core_objects>)
set_target_properties(alaska_core_static PROPERTIES OUTPUT_NAME "alaska_core")
install(
  TARGETS alaska_core_static
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

install(FILES
  include/alaska/sim/HTLB.hpp
  include/alaska/sim/TLB.hpp
  include/alaska/sim/StatisticsManager.hpp
  DESTINATION include/alaska/sim)



##########################################################################################
# This defines the runtime that compiled code is linked against
if(NOT ALASKA_CORE_ONLY) # -----------------------------------------------------------------------------------

  find_library(LIBUNWIND_LIBRARIES NAMES unwind )
  message(STATUS "UNWIND: ${LIBUNWIND_LIBRARIES}")

  set(SOURCES
    rt/init.cpp
    rt/halloc.cpp
    rt/compat.c
    rt/barrier.cpp
  )


if(ALASKA_HTLB_SIM)
  list(APPEND SOURCES rt/sim.cpp)
endif(ALASKA_HTLB_SIM)

  add_library(alaska SHARED ${SOURCES})

  target_include_directories(alaska PUBLIC include/)
  set_target_properties(alaska PROPERTIES LINKER_LANGUAGE C)
  set_target_properties(alaska PROPERTIES SOVERSION ${ALASKA_VERSION})
  target_link_libraries(alaska alaska_core dl pthread ${LIBUNWIND_LIBRARIES})

  # target_compile_options(alaska PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-nostdlib++ -nostdinc++>)
  target_compile_options(alaska PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-nostdinc++>)

if(ALASKA_HTLB_SIM)
   target_link_libraries(alaska alaska_sim)
endif(ALASKA_HTLB_SIM)

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

  add_bitcode(alaska_translate SOURCES core/translate.cpp)
  add_bitcode(alaska_compat SOURCES rt/compat.c)
  add_bitcode(alaska_stub SOURCES
      stub/net.c
      stub/dtoa.c
      stub/obstack.c
      stub/stub.c
      stub/memcpy.c
  )


  target_link_libraries(alaska alaska_translate_native)

endif(NOT ALASKA_CORE_ONLY) # ---------------------------------------------------------------------------------



if(ALASKA_YUKON)
  add_library(yukon SHARED
    yukon/yukon.cpp
  )

  target_include_directories(yukon PUBLIC include/)
  set_target_properties(yukon PROPERTIES LINKER_LANGUAGE C)
  set_target_properties(yukon PROPERTIES SOVERSION ${ALASKA_VERSION})
  target_link_libraries(yukon alaska_core_static dl pthread ${LIBUNWIND_LIBRARIES})
  target_compile_options(yukon PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-nostdlib++ -nostdinc++>)

  install(
    TARGETS yukon
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    PRIVATE_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})


endif(ALASKA_YUKON)


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


	add_executable(
		alaska_test
		test/runtime_test.cpp
		test/pagemanager_test.cpp
		test/heap_test.cpp
		test/sizeclass_test.cpp
		test/magazine_test.cpp
		test/threadcache_test.cpp
		test/hugeobject_test.cpp
    test/sizedalloc_test.cpp
    test/object_allocator_test.cpp
    test/handle_ptr_test.cpp
    test/htlb_sim_test.cpp
    test/locality_page_test.cpp
	)

	target_link_libraries(
		alaska_test
		GTest::gtest_main
    GTest::gmock
    alaska_sim
		alaska_core_static dl pthread
	)

	gtest_discover_tests(alaska_test)

endif(ALASKA_ENABLE_TESTING)
