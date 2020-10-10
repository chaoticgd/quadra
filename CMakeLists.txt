cmake_minimum_required(VERSION 3.11)
project(quadra)

add_executable(quadra
	main.cpp
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
	file(COPY ${ghidra_SOURCE_DIR}/Ghidra/Processors DESTINATION ${CMAKE_CURRENT_LIST_DIR})
	file(RENAME ${CMAKE_CURRENT_LIST_DIR}/Processors ${CMAKE_CURRENT_LIST_DIR}/processors)
endif()

set(DECOMPILER_SOURCE_DIR "${ghidra_SOURCE_DIR}/Ghidra/Features/Decompiler/src")

# Build SLEIGH
add_custom_command(
	OUTPUT ${DECOMPILER_SOURCE_DIR}/decompile/cpp/libsla.a
	COMMAND make libsla.a
	WORKING_DIRECTORY ${DECOMPILER_SOURCE_DIR}/decompile/cpp
)

add_custom_target(
	sleigh
	DEPENDS ${DECOMPILER_SOURCE_DIR}/decompile/cpp/libsla.a
)

# Build quadra
include_directories(${DECOMPILER_SOURCE_DIR})
target_link_libraries(quadra ${DECOMPILER_SOURCE_DIR}/decompile/cpp/libsla.a)
add_dependencies(quadra sleigh)
