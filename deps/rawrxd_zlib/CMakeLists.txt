# ============================================================================
# RawrXD From-Scratch zlib - CMake build
# Provides zlibstatic and zlib targets compatible with standard zlib
# ============================================================================
cmake_minimum_required(VERSION 3.20)

project(rawrxd_zlib VERSION 1.3.0 LANGUAGES C)

# The from-scratch implementation
add_library(zlibstatic STATIC rawrxd_zlib.c)
target_include_directories(zlibstatic PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(zlibstatic PROPERTIES
    OUTPUT_NAME rawrxd_zlib
    POSITION_INDEPENDENT_CODE ON
    C_STANDARD 11
)

# Also provide a 'zlib' alias for compatibility
add_library(zlib ALIAS zlibstatic)

# Provide ZLIB:: imported target for find_package compatibility
add_library(ZLIB::ZLIB ALIAS zlibstatic)

# Export variables for parent scope
set(ZLIB_FOUND TRUE PARENT_SCOPE)
set(HAVE_ZLIB 1 PARENT_SCOPE)
set(HAVE_COMPRESSION 1 PARENT_SCOPE)
set(ZLIB_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}" PARENT_SCOPE)
set(ZLIB_LIBRARIES zlibstatic PARENT_SCOPE)
set(zlib_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}" PARENT_SCOPE)
set(zlib_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}" PARENT_SCOPE)
