# ============================================================================
# RawrXD Compiler System - CMakeLists
# Complete production-ready compiler implementation
# ============================================================================

cmake_minimum_required(VERSION 3.20)
project(RawrXD_Compiler)

# ============================================================================
# COMPILER LIBRARY (CORE ENGINE)
# ============================================================================

add_library(rawrxd_compiler_core STATIC
    solo_compiler_engine.cpp
    solo_compiler_engine.hpp
)

target_include_directories(rawrxd_compiler_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

set_target_properties(rawrxd_compiler_core PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

# ============================================================================
# QT IDE COMPILER INTEGRATION
# ============================================================================

find_package(Qt6 COMPONENTS Core Gui Widgets)

if(Qt6_FOUND)
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTORCC ON)
    set(CMAKE_AUTOUIC ON)
    
    add_library(rawrxd_compiler_qt STATIC
        ../qtapp/compiler_interface.cpp
        ../qtapp/compiler_interface.hpp
    )
    
    target_link_libraries(rawrxd_compiler_qt PUBLIC
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        rawrxd_compiler_core
    )
    
    target_include_directories(rawrxd_compiler_qt PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/..
    )
    
    set_target_properties(rawrxd_compiler_qt PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
    )
endif()

# ============================================================================
# CLI COMPILER ENGINE
# ============================================================================

add_library(rawrxd_compiler_cli STATIC
    cli_compiler_engine.cpp
    cli_compiler_engine.hpp
)

target_link_libraries(rawrxd_compiler_cli PUBLIC
    rawrxd_compiler_core
)

target_include_directories(rawrxd_compiler_cli PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

set_target_properties(rawrxd_compiler_cli PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

# ============================================================================
# CLI EXECUTABLE
# ============================================================================

add_executable(rawrxd-cli
    cli_main.cpp
)

target_link_libraries(rawrxd-cli PUBLIC
    rawrxd_compiler_cli
    rawrxd_compiler_core
)

set_target_properties(rawrxd-cli PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

if(WIN32)
    set_target_properties(rawrxd-cli PROPERTIES
        WIN32_EXECUTABLE OFF  # Console app
        SUFFIX ".exe"
    )
endif()

# ============================================================================
# OPTIONAL: INSTALL TARGETS
# ============================================================================

install(TARGETS rawrxd-cli
    RUNTIME DESTINATION bin
)

install(FILES
    solo_compiler_engine.hpp
    cli_compiler_engine.hpp
    DESTINATION include/rawrxd/compiler
)

# ============================================================================
# COMPILER FLAGS & OPTIMIZATIONS
# ============================================================================

if(MSVC)
    target_compile_options(rawrxd_compiler_core PRIVATE /W4 /WX)
    target_compile_options(rawrxd_compiler_cli PRIVATE /W4 /WX)
    target_compile_options(rawrxd-cli PRIVATE /W4 /WX)
    
    # Optimization settings
    target_compile_options(rawrxd_compiler_core PRIVATE /O2)
    target_compile_options(rawrxd_compiler_cli PRIVATE /O2)
else()
    target_compile_options(rawrxd_compiler_core PRIVATE -Wall -Wextra -Werror)
    target_compile_options(rawrxd_compiler_cli PRIVATE -Wall -Wextra -Werror)
    target_compile_options(rawrxd-cli PRIVATE -Wall -Wextra -Werror)
    
    # Optimization settings
    target_compile_options(rawrxd_compiler_core PRIVATE -O2)
    target_compile_options(rawrxd_compiler_cli PRIVATE -O2)
endif()

# ============================================================================
# SUMMARY
# ============================================================================

message(STATUS "╔════════════════════════════════════════════════════════════╗")
message(STATUS "║         RawrXD Compiler System Configuration              ║")
message(STATUS "╚════════════════════════════════════════════════════════════╝")
message(STATUS "")
message(STATUS "  ✓ Solo Compiler Engine (MASM/x86-64)")
message(STATUS "  ✓ QT IDE Integration (if Qt6 found)")
message(STATUS "  ✓ CLI Compiler Frontend")
message(STATUS "  ✓ Lexer with 100+ token types")
message(STATUS "  ✓ Full recursive descent parser")
message(STATUS "  ✓ Semantic analysis with symbol tables")
message(STATUS "  ✓ Multi-target code generation")
message(STATUS "  ✓ Integrated error reporting")
message(STATUS "  ✓ Performance metrics & tracing")
message(STATUS "  ✓ Build cache support")
message(STATUS "")
message(STATUS "Build output: ${CMAKE_BINARY_DIR}/bin/rawrxd-cli")
message(STATUS "")
