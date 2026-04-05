# ============================================================================
# ApertureManager Build Configuration
# Manifest-Aware Aperture Orchestrator for RawrXD Loader
# ============================================================================

cmake_minimum_required(VERSION 3.20)
project(ApertureManager CXX)

# ApertureManager Library
add_library(ApertureManager STATIC
    ../src/RawrXD_ApertureManager.cpp
)

target_include_directories(ApertureManager PUBLIC
    ../include
    ${NLOHMANN_JSON_INCLUDE_DIR}
    "C:/Program Files (x86)/nlohmann_json/include"
    "C:/Program Files (x86)/Windows Kits/10/Include/10.0.22621.0/um"
)

target_link_libraries(ApertureManager PUBLIC
    crypt32
    wincrypt
    kernel32
)

target_compile_features(ApertureManager PUBLIC cxx_std_20)

# ============================================================================
# Integration Test Executable
# ============================================================================

add_executable(ApertureManager_IntegrationTest
    ../src/RawrXD_ApertureManager_IntegrationTest.cpp
)

target_link_libraries(ApertureManager_IntegrationTest PRIVATE
    ApertureManager
)

target_compile_features(ApertureManager_IntegrationTest PRIVATE cxx_std_20)

# Set up test to run against sealed archive
add_test(
    NAME ApertureManager_IntegrationTest
    COMMAND ApertureManager_IntegrationTest
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

set_tests_properties(
    ApertureManager_IntegrationTest
    PROPERTIES
    TIMEOUT 60
    PASS_REGULAR_EXPRESSION "ALL TESTS PASSED"
)
