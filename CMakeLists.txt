
# set(CMAKE_CXX_STANDARD 14)
# set(CMAKE_CXX_STANDARD_REQUIRED ON)

string(ASCII 27 Esc)
set(Purple "${Esc}[35m")
set(Green "${Esc}[32m")
set(Red "${Esc}[31m")
set(ColorReset "${Esc}[m")

function(alaska_switch OPT DEFAULT)
  set(${OPT} ${DEFAULT} CACHE BOOL "")

  if(${OPT} MATCHES "ON|On|on|TRUE|true|1")
    add_compile_definitions(${OPT}=1)
    # set(${OPT} ON PARENT_SCOPE)
    message(STATUS "${Purple}OPTION${ColorReset} ${OPT} ${Green}ON${ColorReset}")
  elseif(${OPT} MATCHES "OFF|Off|off|FALSE|false|0")
    set(${OPT} OFF PARENT_SCOPE)
    message(STATUS "${Purple}OPTION${ColorReset} ${OPT} ${Red}OFF${ColorReset}")
  else()
    message(FATAL_ERROR "${OPT} must be either ON or OFF")
  endif()
endfunction()


function(alaska_int_option OPT DEFAULT)
  set(${OPT} ${DEFAULT} CACHE STRING "")
  add_compile_definitions(${OPT}=${${OPT}})
  message(STATUS "${Purple}OPTION${ColorReset} ${OPT} ${Green}${${OPT}}${ColorReset}")
endfunction()



alaska_switch(ALASKA_ENABLE_COMPILER ON)
alaska_switch(ALASKA_ENABLE_TESTING  ON)

alaska_int_option(ALASKA_SIZE_BITS 24)

# set(ALASKA_ENABLE_COMPILER ON CACHE BOOL "Build the Alaska Compiler, binding to LLVM")
# set(ALASKA_DISABLE_TESTING OFF CACHE BOOL "Disable testing the core runtime with gtest")

set(ALASKA_VERSION 2)

# -------------------------------------------------------------------------------------------

cmake_minimum_required(VERSION 3.13)
project(Alaska C CXX ASM)


if(CMAKE_C_COMPILER_ID STREQUAL "Clang")
  message(STATUS "Using Clang")
else()
  message(STATUS "Not using Clang. Disabling compilation of the Alaska Compiler because we cannot produce bitcode files with the current compiler")
  set(ALASKA_ENABLE_COMPILER FALSE)
endif()


# If we are building the core only, don't enable the compiler!
if(ALASKA_CORE_ONLY)
  set(ALASKA_ENABLE_COMPILER FALSE)
endif()

add_compile_definitions("ALASKA_INSTALL_PREFIX=\"${CMAKE_INSTALL_PREFIX}\"")
add_definitions(-include "${CMAKE_CURRENT_SOURCE_DIR}/runtime/include/alaska/config.h")

set(CMAKE_INSTALL_LIBDIR ${CMAKE_INSTALL_PREFIX}/lib)
set(CMAKE_INSTALL_BINDIR ${CMAKE_INSTALL_PREFIX}/bin)
set(CMAKE_INSTALL_INCLUDEDIR ${CMAKE_INSTALL_PREFIX}/include)
set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_LIBDIR}")
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++2a -fPIC -g3 -Wno-unknown-warning-option -Wno-covered-switch-default ")
set(CMAKE_INSTALL_MESSAGE LAZY)
set(CMAKE_VERBOSE_MAKEFILE OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/tools/cmake")
include(LLVMIRUtil)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE INTERNAL "")
# https://discourse.nixos.org/t/get-clangd-to-find-standard-headers-in-nix-shell/11268/10
if(CMAKE_EXPORT_COMPILE_COMMANDS)
  set(CMAKE_CXX_STANDARD_INCLUDE_DIRECTORIES ${CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES})
endif(CMAKE_EXPORT_COMPILE_COMMANDS)


find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PROGRAM}")
endif()



add_subdirectory(runtime)


if(ALASKA_ENABLE_COMPILER)
  # configure LLVM
  find_package(LLVM 16 REQUIRED CONFIG)

  set(LLVM_RUNTIME_OUTPUT_INTDIR ${CMAKE_BINARY_DIR}/${CMAKE_CFG_INTDIR}/)
  set(LLVM_LIBRARY_OUTPUT_INTDIR ${CMAKE_BINARY_DIR}/${CMAKE_CFG_INTDIR}/)


  list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
  include(HandleLLVMOptions)
  include(AddLLVM)
  add_subdirectory(compiler)
endif(ALASKA_ENABLE_COMPILER)
