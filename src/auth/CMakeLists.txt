# CMakeLists.txt for Quantum Authentication Component
# RawrXD IDE - Quantum-Resistant Key Generation and Management

cmake_minimum_required(VERSION 3.16)


set(AUTH_SOURCES
    QuantumAuthUI.cpp
)

set(AUTH_HEADERS
    QuantumAuthUI.hpp
)

add_library(rawrxd_auth STATIC
    ${AUTH_SOURCES}
    ${AUTH_HEADERS}
)

set_target_properties(rawrxd_auth PROPERTIES
    AUTOMOC ON
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

target_link_libraries(rawrxd_auth PUBLIC
)

target_include_directories(rawrxd_auth PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
)

target_compile_definitions(rawrxd_auth PRIVATE
    RAWRXD_QUANTUM_AUTH_ENABLED
    RDRAND_ENTROPY_ENABLED
    RDSEED_ENTROPY_ENABLED
)

# Platform-specific entropy sources
if(WIN32)
    target_compile_definitions(rawrxd_auth PRIVATE
        WIN32_ENTROPY_ENABLED
    )
endif()

message(STATUS "✓ Quantum Authentication component configured")
