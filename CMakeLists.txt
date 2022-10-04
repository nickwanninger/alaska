cmake_minimum_required(VERSION 3.13)

project(Alaska C CXX ASM)

enable_testing()

# configure LLVM 
find_package(LLVM REQUIRED CONFIG)

set(LLVM_RUNTIME_OUTPUT_INTDIR ${CMAKE_BINARY_DIR}/${CMAKE_CFG_INTDIR}/)
set(LLVM_LIBRARY_OUTPUT_INTDIR ${CMAKE_BINARY_DIR}/${CMAKE_CFG_INTDIR}/)

list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(HandleLLVMOptions)
include(AddLLVM)

include(config.cmake)


list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/tools/cmake")
include(LLVMIRUtil)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_ASM_COMPILER clang)

set(CMAKE_STAGING_PREFIX ${CMAKE_BINARY_DIR})
set(CMAKE_INSTALL_PREFIX ${CMAKE_BINARY_DIR})
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -fPIC -g3 ")




include_directories(
	${LLVM_INCLUDE_DIRS} 
	dep/noelle/install/include
	dep/noelle/install/include/svf
	dep/virgil/include
)

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PROGRAM}")
endif()





include_directories(
	${LLVM_INCLUDE_DIRS}
	virgil/include
	include/
)


add_llvm_library(Alaska MODULE
	src/alaska/Pass.cpp


	PLUGIN_TOOL
	opt
	# src/alaska/PinAnalysis.cpp
)

# add_llvm_library(AlaskaNormalize MODULE src/alaska-norm/Pass.cpp)

include_directories(src/rt/include)

add_subdirectory(src/rt)
add_subdirectory(test/)
