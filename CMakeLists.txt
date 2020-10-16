cmake_minimum_required(VERSION 3.11)
project(quadra)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(quadra
	src/main.cpp
	src/elf_loader.cpp
)

# Download Ghidra
include(FetchContent)
FetchContent_Declare(
	ghidra
	GIT_REPOSITORY https://github.com/NationalSecurityAgency/ghidra.git
	GIT_TAG        f7f366b261589a65696c17852812566b9e6659bb
)
if(NOT ghidra_POPULATED)
	FetchContent_Populate(ghidra)
	if(NOT EXISTS "${CMAKE_CURRENT_LIST_DIR}/processors")
		file(COPY ${ghidra_SOURCE_DIR}/Ghidra/Processors DESTINATION ${CMAKE_CURRENT_LIST_DIR})
		file(RENAME ${CMAKE_CURRENT_LIST_DIR}/Processors ${CMAKE_CURRENT_LIST_DIR}/processors)
	endif()
endif()

set(DECOMPILER_SOURCE_DIR "${ghidra_SOURCE_DIR}/Ghidra/Features/Decompiler/src")

# Build SLEIGH library
add_custom_command(
	OUTPUT ${DECOMPILER_SOURCE_DIR}/decompile/cpp/libsla.a
	COMMAND make libsla.a
	WORKING_DIRECTORY ${DECOMPILER_SOURCE_DIR}/decompile/cpp
)

add_custom_target(
	sleigh_library
	DEPENDS ${DECOMPILER_SOURCE_DIR}/decompile/cpp/libsla.a
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
target_link_libraries(quadra ${DECOMPILER_SOURCE_DIR}/decompile/cpp/libsla.a)
add_dependencies(quadra sleigh_library sleigh_compiler)
if(UNIX)
	target_link_libraries(quadra stdc++fs)
endif()
