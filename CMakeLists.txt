cmake_minimum_required(VERSION 3.0.0)

set(GDTU_CMAKE_DIR   "${CMAKE_CURRENT_LIST_DIR}/cmake"   CACHE PATH "The the path to the cmake directory")
list(APPEND CMAKE_MODULE_PATH ${GDTU_CMAKE_DIR})

# Set cmake_policies
include(GdtuPolicy)
gdtu_policy()

# project name and version should be set after cmake_policy CMP0048
set(PROJECT_VERSION "0.4.2")
project(solidity VERSION ${PROJECT_VERSION})

# Let's find our dependencies
include(GdtuDependencies)

# Figure out what compiler and system are we using
include(GdtuCompilerSettings)

# Include helper macros
include(GdtuExecutableHelper)

# Include utils
include(GdtuUtils)

include(GdtuOptions)
configure_project(TESTS)

add_subdirectory(libdevcore)
add_subdirectory(libevmasm)
add_subdirectory(libsolidity)
add_subdirectory(solc)

if (NOT EMSCRIPTEN)
	add_subdirectory(liblll)
	add_subdirectory(lllc)
endif()

if (TESTS AND NOT EMSCRIPTEN)
	add_subdirectory(test)
endif()
