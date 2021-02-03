cmake_minimum_required(VERSION 3.11)
project(quadra)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(quadra
	src/main.cpp
	src/elf_loader.cpp
	src/translator.cpp
	src/quadra_architecture.cpp
)

set(DECOMPILER_SOURCE_DIR "${GHIDRA_DIR}/Ghidra/Features/Decompiler/src")

# LLVM
find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")
include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})
llvm_map_components_to_libnames(llvm_libs core support)

# Build decompiler library
add_custom_command(
	OUTPUT ${DECOMPILER_SOURCE_DIR}/decompile/cpp/libdecomp.a
	COMMAND make libdecomp.a
	WORKING_DIRECTORY ${DECOMPILER_SOURCE_DIR}/decompile/cpp
)

add_custom_target(
	sleigh_library
	DEPENDS ${DECOMPILER_SOURCE_DIR}/decompile/cpp/libdecomp.a
)

# Build SLEIGH compiler
add_custom_command(
	OUTPUT ${CMAKE_CURRENT_LIST_DIR}/sleigh_opt
	COMMAND make sleigh_opt && cp sleigh_opt ${CMAKE_CURRENT_LIST_DIR}/sleigh_opt
	WORKING_DIRECTORY ${DECOMPILER_SOURCE_DIR}/decompile/cpp
)

add_custom_target(
	sleigh_compiler
	DEPENDS ${CMAKE_CURRENT_LIST_DIR}/sleigh_opt
)

# Build quadra
include_directories(${DECOMPILER_SOURCE_DIR})
target_link_libraries(quadra ${DECOMPILER_SOURCE_DIR}/decompile/cpp/libdecomp.a ${llvm_libs})
add_dependencies(quadra sleigh_library sleigh_compiler)
if(UNIX)
	target_link_libraries(quadra stdc++fs)
endif()

add_library(mips_o32_linux STATIC
	syscalls/mips_o32_linux.c
)
