# CMakeLists.txt for Unit Tests
# Google Test and Qt Test Framework configuration for RawrXD IDE components

cmake_minimum_required(VERSION 3.16)

# Enable testing
enable_testing()

# Find required packages
find_package(Qt5 COMPONENTS 
    Core 
    Gui 
    Sql 
    Network 
    Multimedia 
    Concurrent
    REQUIRED
)

# Google Test
include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)

# Prevent installation of gtest
set(INSTALL_GTEST OFF CACHE BOOL "Disable gtest installation" FORCE)

FetchContent_MakeAvailable(googletest)

# ============= Voice Processor Tests =============
add_executable(voice_processor_test
    voice_processor_test.cpp
)

target_link_libraries(voice_processor_test
    PRIVATE
        gtest
        gtest_main
        Qt5::Core
        Qt5::Multimedia
        Qt5::Network
        Qt5::Concurrent
)

target_include_directories(voice_processor_test
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

add_test(
    NAME VoiceProcessorTests
    COMMAND voice_processor_test
)

# ============= AI Merge Resolver Tests =============
add_executable(ai_merge_resolver_test
    ai_merge_resolver_test.cpp
)

target_link_libraries(ai_merge_resolver_test
    PRIVATE
        gtest
        gtest_main
        Qt5::Core
        Qt5::Network
        Qt5::Concurrent
)

target_include_directories(ai_merge_resolver_test
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

add_test(
    NAME AIMergeResolverTests
    COMMAND ai_merge_resolver_test
)

# ============= Semantic Diff Analyzer Tests =============
add_executable(semantic_diff_analyzer_test
    semantic_diff_analyzer_test.cpp
)

target_link_libraries(semantic_diff_analyzer_test
    PRIVATE
        gtest
        gtest_main
        Qt5::Core
        Qt5::Concurrent
)

target_include_directories(semantic_diff_analyzer_test
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

add_test(
    NAME SemanticDiffAnalyzerTests
    COMMAND semantic_diff_analyzer_test
)

# ============= Zero Retention Manager Tests =============
add_executable(zero_retention_manager_test
    zero_retention_manager_test.cpp
)

target_link_libraries(zero_retention_manager_test
    PRIVATE
        gtest
        gtest_main
        Qt5::Core
        Qt5::Concurrent
)

target_include_directories(zero_retention_manager_test
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

add_test(
    NAME ZeroRetentionManagerTests
    COMMAND zero_retention_manager_test
)

# ============= Sandboxed Terminal Tests =============
add_executable(sandboxed_terminal_test
    sandboxed_terminal_test.cpp
)

target_link_libraries(sandboxed_terminal_test
    PRIVATE
        gtest
        gtest_main
        Qt5::Core
        Qt5::Concurrent
)

target_include_directories(sandboxed_terminal_test
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

add_test(
    NAME SandboxedTerminalTests
    COMMAND sandboxed_terminal_test
)

# ============= Model Registry Tests =============
add_executable(model_registry_test
    model_registry_test.cpp
)

target_link_libraries(model_registry_test
    PRIVATE
        Qt5::Test
        Qt5::Core
        Qt5::Gui
        Qt5::Sql
)

target_include_directories(model_registry_test
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

add_test(
    NAME ModelRegistryTests
    COMMAND model_registry_test
)

# ============= Test Configuration Options =============

# Enable verbose test output
set(CMAKE_CTEST_COMMAND ctest --verbose)

# Set compiler flags for testing
if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHsc /W4")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -std=c++17")
endif()

# ============= Test Coverage (Optional) =============

# Uncomment to enable code coverage reporting
# include(CodeCoverage)
# append_coverage_compiler_flags()

# ============= Custom Test Targets =============

# Run all tests
add_custom_target(run_all_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS 
        voice_processor_test
        ai_merge_resolver_test
        semantic_diff_analyzer_test
        zero_retention_manager_test
        sandboxed_terminal_test
        model_registry_test
    COMMENT "Running all unit tests"
)

# Run only Google Test tests
add_custom_target(run_gtest_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
            -R "(VoiceProcessor|AIMergeResolver|SemanticDiffAnalyzer|ZeroRetentionManager|SandboxedTerminal)"
    DEPENDS 
        voice_processor_test
        ai_merge_resolver_test
        semantic_diff_analyzer_test
        zero_retention_manager_test
        sandboxed_terminal_test
    COMMENT "Running Google Test suite"
)

# Run only Qt Test tests
add_custom_target(run_qt_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
            -R "ModelRegistry"
    DEPENDS model_registry_test
    COMMENT "Running Qt Test suite"
)

# Run specific test suite with verbose output
add_custom_target(run_voice_processor_tests
    COMMAND voice_processor_test
    DEPENDS voice_processor_test
    COMMENT "Running VoiceProcessor tests"
)

add_custom_target(run_ai_merge_resolver_tests
    COMMAND ai_merge_resolver_test
    DEPENDS ai_merge_resolver_test
    COMMENT "Running AIMergeResolver tests"
)

add_custom_target(run_semantic_diff_analyzer_tests
    COMMAND semantic_diff_analyzer_test
    DEPENDS semantic_diff_analyzer_test
    COMMENT "Running SemanticDiffAnalyzer tests"
)

add_custom_target(run_zero_retention_manager_tests
    COMMAND zero_retention_manager_test
    DEPENDS zero_retention_manager_test
    COMMENT "Running ZeroRetentionManager tests"
)

add_custom_target(run_sandboxed_terminal_tests
    COMMAND sandboxed_terminal_test
    DEPENDS sandboxed_terminal_test
    COMMENT "Running SandboxedTerminal tests"
)

add_custom_target(run_model_registry_tests
    COMMAND model_registry_test
    DEPENDS model_registry_test
    COMMENT "Running ModelRegistry tests"
)

# ============= Test Summary =============

message(STATUS "")
message(STATUS "========== Test Configuration ==========")
message(STATUS "Google Test Framework: Enabled")
message(STATUS "Qt Test Framework: Enabled")
message(STATUS "")
message(STATUS "Test Targets Created:")
message(STATUS "  - voice_processor_test (33 tests)")
message(STATUS "  - ai_merge_resolver_test (40+ tests)")
message(STATUS "  - semantic_diff_analyzer_test (45+ tests)")
message(STATUS "  - zero_retention_manager_test (50+ tests)")
message(STATUS "  - sandboxed_terminal_test (55+ tests)")
message(STATUS "  - model_registry_test (67 tests)")
message(STATUS "")
message(STATUS "Custom Targets:")
message(STATUS "  - run_all_tests (run all test suites)")
message(STATUS "  - run_gtest_tests (run Google Test suite)")
message(STATUS "  - run_qt_tests (run Qt Test suite)")
message(STATUS "  - run_[component]_tests (run individual component tests)")
message(STATUS "")
message(STATUS "Build and run tests with:")
message(STATUS "  cmake --build . --target run_all_tests")
message(STATUS "")
message(STATUS "Total Test Coverage: ~290 individual test cases")
message(STATUS "=========================================")
message(STATUS "")

# ============= CTest Configuration =============

# Set timeout for long-running tests (in seconds)
set_tests_properties(
    VoiceProcessorTests
    AIMergeResolverTests
    SemanticDiffAnalyzerTests
    ZeroRetentionManagerTests
    SandboxedTerminalTests
    ModelRegistryTests
    PROPERTIES TIMEOUT 300
)

# Configure parallel test execution
set(CTEST_PARALLEL_LEVEL 4 CACHE STRING "Number of parallel test processes")

# Enable detailed output on test failure
set(CTEST_OUTPUT_ON_FAILURE ON CACHE BOOL "Output on test failure")
