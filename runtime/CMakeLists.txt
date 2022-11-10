# Compile the runtime with some special flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2 -g3 -Wall -Wno-reorder -Wextra -Wno-format -Wno-ignored-attributes -Wno-unused-parameter -Wno-sign-compare ")
set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -fno-exceptions -fno-rtti -nostdlib++ ")

include_directories(
	include/
)

set(SRC_FILES
	# src/rbtree.c
	src/runtime.cpp
	src/Arena.cpp
	src/translate.c
)

add_definitions(-D_GNU_SOURCE -D_REENTRANT)



# add_library(alaska_library STATIC ${SRC_FILES})
# target_include_directories(alaska_library PUBLIC include/)
# set_target_properties(alaska_library PROPERTIES LINKER_LANGUAGE CXX)

# install(
#   TARGETS alaska_library
#   ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
#   LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
#   PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
#   PRIVATE_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# llvmir_attach_bc_target(
#   TARGET alaska_bitcodes
#   DEPENDS alaska_library)

# llvmir_attach_link_target(
#   TARGET alaska
#   DEPENDS alaska_bitcodes
#   OUTPUT_DIR ${CMAKE_INSTALL_LIBDIR})

# add_dependencies(alaska alaska_library)

# set_property(TARGET alaska_bitcodes PROPERTY
#   EXCLUDE_FROM_ALL OFF)
# set_property(TARGET alaska PROPERTY
#   EXCLUDE_FROM_ALL OFF)

add_library(alaska SHARED ${SRC_FILES})

set_target_properties(alaska PROPERTIES
  LINKER_LANGUAGE CXX)

install(
	TARGETS alaska
	RUNTIME
)

# install(
#   TARGETS alaska
#   ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
#   LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
#   PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
#   PRIVATE_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})


# llvmir_attach_bc_target(
#   TARGET alaska_bitcodes
#   DEPENDS alaska)

# llvmir_attach_link_target(
#   TARGET alaska_bc
#   DEPENDS alaska_bitcodes
#   OUTPUT_DIR ${CMAKE_INSTALL_LIBDIR})

# add_dependencies(alaska alaska_library)

# set_property(TARGET alaska_bitcodes PROPERTY
#   EXCLUDE_FROM_ALL OFF)
# set_property(TARGET alaska PROPERTY
#   EXCLUDE_FROM_ALL OFF)