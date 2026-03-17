cmake_minimum_required(VERSION 3.15)
project(RawrXD_Integration_Examples VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# RawrXD installation directory
set(RAWRXD_ROOT "C:/RawrXD" CACHE PATH "RawrXD installation directory")

# Check if RawrXD is installed
if(NOT EXISTS "${RAWRXD_ROOT}")
    message(FATAL_ERROR "RawrXD not found at ${RAWRXD_ROOT}")
endif()

# RawrXD library paths
set(RAWRXD_LIBRARIES_DIR "${RAWRXD_ROOT}/Libraries")
set(RAWRXD_HEADERS_DIR "${RAWRXD_ROOT}/Headers")
set(RAWRXD_BIN_DIR "${RAWRXD_ROOT}/bin")

# Import RawrXD libraries
add_library(RawrXD::Encoder STATIC IMPORTED)
set_target_properties(RawrXD::Encoder PROPERTIES
    IMPORTED_LOCATION "${RAWRXD_LIBRARIES_DIR}/rawrxd_encoder.lib"
)

add_library(RawrXD::PEGen STATIC IMPORTED)
set_target_properties(RawrXD::PEGen PROPERTIES
    IMPORTED_LOCATION "${RAWRXD_LIBRARIES_DIR}/rawrxd_pe_gen.lib"
)

# Advanced integration example
add_executable(rawrxd_demo advanced_integration_example.cpp)
target_link_libraries(rawrxd_demo PRIVATE RawrXD::Encoder)
target_include_directories(rawrxd_demo PRIVATE ${RAWRXD_HEADERS_DIR})

# Simple example (if exists)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/simple_example.cpp")
    add_executable(rawrxd_simple simple_example.cpp)
    target_link_libraries(rawrxd_simple PRIVATE RawrXD::Encoder)
endif()

# Installation
install(TARGETS rawrxd_demo DESTINATION bin)

# Display configuration info
message(STATUS "RawrXD Integration Configuration:")
message(STATUS "  Root directory: ${RAWRXD_ROOT}")
message(STATUS "  Libraries: ${RAWRXD_LIBRARIES_DIR}")
message(STATUS "  Headers: ${RAWRXD_HEADERS_DIR}")
message(STATUS "  Binaries: ${RAWRXD_BIN_DIR}")

# Print available RawrXD components
file(GLOB RAWRXD_LIBS "${RAWRXD_LIBRARIES_DIR}/*.lib")
message(STATUS "Available RawrXD libraries:")
foreach(lib ${RAWRXD_LIBS})
    get_filename_component(libname ${lib} NAME)
    message(STATUS "    - ${libname}")
endforeach()
