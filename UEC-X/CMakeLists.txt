# UEC-X Universal Extension Core eXtended
# CMake Build Configuration

cmake_minimum_required(VERSION 3.20)
project(UEC-X VERSION 1.0.0 LANGUAGES CXX ASM_MASM)

# =============================================================================
# Options
# =============================================================================

option(UEC_BUILD_MASM64 "Build MASM64 dispatch core" ON)
option(UEC_BUILD_MICROKERNEL "Build C++ microkernel" ON)
option(UEC_BUILD_PROTOCOL "Build protocol layer" ON)
option(UEC_BUILD_ADAPTERS "Build IDE adapters" ON)
option(UEC_BUILD_RUNTIME "Build extension runtime" ON)
option(UEC_BUILD_HOTPATCH "Build hotpatch system" ON)
option(UEC_BUILD_SECURITY "Build security sandbox" ON)
option(UEC_BUILD_TOOLING "Build CLI tooling" ON)
option(UEC_BUILD_TESTS "Build tests" ON)
option(UEC_BUILD_EXAMPLES "Build examples" ON)

option(UEC_ENABLE_SANITIZERS "Enable sanitizers" OFF)
option(UEC_ENABLE_PROFILING "Enable profiling" OFF)
option(UEC_ENABLE_COVERAGE "Enable code coverage" OFF)

# =============================================================================
# C++ Standard
# =============================================================================

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# =============================================================================
# Compiler Flags
# =============================================================================

if(MSVC)
    add_compile_options(/W4 /WX- /permissive- /Zc:__cplusplus)
    add_compile_options(/MP)  # Multi-processor compilation
    add_compile_options(/EHsc)  # Exception handling
    
    if(UEC_ENABLE_SANITIZERS)
        add_compile_options(/fsanitize=address)
    endif()
    
    # Release optimizations
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /O2 /Ob2 /Oi /Ot /GL")
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /LTCG")
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
    add_compile_options(-fPIC)
    
    if(UEC_ENABLE_SANITIZERS)
        add_compile_options(-fsanitize=address,undefined)
        add_link_options(-fsanitize=address,undefined)
    endif()
    
    if(UEC_ENABLE_PROFILING)
        add_compile_options(-pg)
        add_link_options(-pg)
    endif()
    
    if(UEC_ENABLE_COVERAGE)
        add_compile_options(--coverage)
        add_link_options(--coverage)
    endif()
    
    # Release optimizations
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -march=native -flto")
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -flto")
endif()

# =============================================================================
# Definitions
# =============================================================================

add_compile_definitions(
    UEC_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    UEC_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    UEC_VERSION_PATCH=${PROJECT_VERSION_PATCH}
)

if(WIN32)
    add_compile_definitions(UEC_PLATFORM_WINDOWS)
else()
    add_compile_definitions(UEC_PLATFORM_UNIX)
endif()

# =============================================================================
# Dependencies
# =============================================================================

include(FetchContent)

# nlohmann/json
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.2
)
FetchContent_MakeAvailable(json)

# =============================================================================
# Include Directories
# =============================================================================

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/core/microkernel/include
    ${CMAKE_CURRENT_SOURCE_DIR}/protocol
    ${CMAKE_CURRENT_SOURCE_DIR}/adapters
)

# =============================================================================
# MASM64 Core
# =============================================================================

if(UEC_BUILD_MASM64 AND MSVC)
    enable_language(ASM_MASM)
    
    add_library(uec_masm64 SHARED
        core/masm64/dispatch.asm
    )
    
    set_target_properties(uec_masm64 PROPERTIES
        OUTPUT_NAME "uec_masm64"
        SUFFIX ".dll"
    )
    
    target_link_libraries(uec_masm64 PRIVATE
        kernel32
        user32
    )
endif()

# =============================================================================
# Microkernel Library
# =============================================================================

if(UEC_BUILD_MICROKERNEL)
    add_library(uec_microkernel SHARED
        core/microkernel/src/microkernel.cpp
        core/microkernel/src/command_registry.cpp
        core/microkernel/src/event_bus.cpp
        core/microkernel/src/scheduler.cpp
        core/microkernel/src/extension_host.cpp
        core/microkernel/src/security_sandbox.cpp
        core/microkernel/src/kv_store.cpp
    )
    
    target_compile_definitions(uec_microkernel PRIVATE UEC_BUILDING_CORE)
    
    target_link_libraries(uec_microkernel PRIVATE
        nlohmann_json::nlohmann_json
    )
    
    if(WIN32)
        target_link_libraries(uec_microkernel PRIVATE
            kernel32
            user32
            advapi32
        )
    endif()
    
    set_target_properties(uec_microkernel PROPERTIES
        OUTPUT_NAME "uec_microkernel"
    )
endif()

# =============================================================================
# Protocol Library
# =============================================================================

if(UEC_BUILD_PROTOCOL)
    add_library(uec_protocol SHARED
        protocol/jsonrpc_protocol.cpp
    )
    
    target_compile_definitions(uec_protocol PRIVATE UEC_BUILDING_CORE)
    
    target_link_libraries(uec_protocol PRIVATE
        uec_microkernel
        nlohmann_json::nlohmann_json
    )
    
    if(WIN32)
        target_link_libraries(uec_protocol PRIVATE
            kernel32
        )
    endif()
    
    set_target_properties(uec_protocol PROPERTIES
        OUTPUT_NAME "uec_protocol"
    )
endif()

# =============================================================================
# Runtime Library
# =============================================================================

if(UEC_BUILD_RUNTIME)
    add_library(uec_runtime SHARED
        runtime/extension_host/extension_runtime.cpp
        runtime/extension_host/extension_api.cpp
        runtime/language_servers/lsp_bridge.cpp
    )
    
    target_compile_definitions(uec_runtime PRIVATE UEC_BUILDING_CORE)
    
    target_link_libraries(uec_runtime PRIVATE
        uec_microkernel
        uec_protocol
    )
    
    set_target_properties(uec_runtime PROPERTIES
        OUTPUT_NAME "uec_runtime"
    )
endif()

# =============================================================================
# Security Library
# =============================================================================

if(UEC_BUILD_SECURITY)
    add_library(uec_security SHARED
        security/sandbox_impl.cpp
        security/capability_manager.cpp
        security/audit_logger.cpp
    )
    
    target_compile_definitions(uec_security PRIVATE UEC_BUILDING_CORE)
    
    target_link_libraries(uec_security PRIVATE
        uec_microkernel
    )
    
    set_target_properties(uec_security PROPERTIES
        OUTPUT_NAME "uec_security"
    )
endif()

# =============================================================================
# Hotpatch Library
# =============================================================================

if(UEC_BUILD_HOTPATCH)
    add_library(uec_hotpatch SHARED
        hotpatch/patch_engine.cpp
        hotpatch/memory_manager.cpp
        hotpatch/trampoline_generator.cpp
    )
    
    target_compile_definitions(uec_hotpatch PRIVATE UEC_BUILDING_CORE)
    
    target_link_libraries(uec_hotpatch PRIVATE
        uec_microkernel
    )
    
    if(WIN32)
        target_link_libraries(uec_hotpatch PRIVATE
            dbghelp
        )
    endif()
    
    set_target_properties(uec_hotpatch PROPERTIES
        OUTPUT_NAME "uec_hotpatch"
    )
endif()

# =============================================================================
# CLI Tool
# =============================================================================

if(UEC_BUILD_TOOLING)
    add_executable(uec-cli
        tooling/cli/main.cpp
        tooling/cli/commands.cpp
        tooling/cli/interactive_shell.cpp
    )
    
    target_link_libraries(uec-cli PRIVATE
        uec_microkernel
        uec_protocol
        uec_runtime
        uec_security
        uec_hotpatch
    )
    
    set_target_properties(uec-cli PROPERTIES
        OUTPUT_NAME "uec"
    )
endif()

# =============================================================================
# Tests
# =============================================================================

if(UEC_BUILD_TESTS)
    enable_testing()
    
    # Find or fetch Google Test
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.14.0
    )
    FetchContent_MakeAvailable(googletest)
    
    add_executable(uec_tests
        tests/test_main.cpp
        tests/test_microkernel.cpp
        tests/test_command_registry.cpp
        tests/test_event_bus.cpp
        tests/test_scheduler.cpp
        tests/test_kv_store.cpp
        tests/test_protocol.cpp
    )
    
    target_link_libraries(uec_tests PRIVATE
        uec_microkernel
        uec_protocol
        uec_runtime
        uec_security
        GTest::gtest_main
    )
    
    include(GoogleTest)
    gtest_discover_tests(uec_tests)
endif()

# =============================================================================
# Examples
# =============================================================================

if(UEC_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# =============================================================================
# Install
# =============================================================================

install(TARGETS 
    uec_microkernel
    uec_protocol
    uec_runtime
    uec_security
    uec_hotpatch
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(DIRECTORY core/microkernel/include/
    DESTINATION include/uec
)

install(FILES
    specs/UEC_PROTOCOL_SPEC.md
    specs/UEC_ABI_SPEC.md
    specs/UEC_EXTENSION_FORMAT.md
    DESTINATION share/uec/docs
)
