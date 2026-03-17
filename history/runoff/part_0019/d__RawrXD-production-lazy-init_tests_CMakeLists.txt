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
    Test
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

# ============= Inference Engine Configuration Tests =============
add_executable(inference_engine_config_test
    unit/test_inference_engine_config.cpp
)

target_link_libraries(inference_engine_config_test
    PRIVATE
        Qt5::Test
        Qt5::Core
)

target_include_directories(inference_engine_config_test
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

add_test(
    NAME InferenceEngineConfigTests
    COMMAND inference_engine_config_test
)

# ============= Frictionless Model Sharding Tests =============
add_executable(frictionless_sharding_test
    test_frictionless_sharding.cpp
)

target_link_libraries(frictionless_sharding_test
    PRIVATE
        gtest
        gtest_main
        Qt5::Core
        Qt5::Concurrent
)

target_include_directories(frictionless_sharding_test
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/include
)

add_test(
    NAME FrictionlessShardingTests
    COMMAND frictionless_sharding_test
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
        inference_engine_config_test
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
            -R "ModelRegistry|InferenceEngineConfig"
    DEPENDS 
        model_registry_test
        inference_engine_config_test
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

add_custom_target(run_inference_engine_config_tests
    COMMAND inference_engine_config_test
    DEPENDS inference_engine_config_test
    COMMENT "Running inference engine configuration regression tests"
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
message(STATUS "  - inference_engine_config_test (3 config checks)")
message(STATUS "")
message(STATUS "Custom Targets:")
message(STATUS "  - run_all_tests (run all test suites)")
message(STATUS "  - run_gtest_tests (run Google Test suite)")
message(STATUS "  - run_qt_tests (run Qt Test suite)")
message(STATUS "  - run_[component]_tests (run individual component tests)")
message(STATUS "  - run_inference_engine_config_tests (run default directory regression)")
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
    InferenceEngineConfigTests
    PROPERTIES TIMEOUT 300
)

# Configure parallel test execution
set(CTEST_PARALLEL_LEVEL 4 CACHE STRING "Number of parallel test processes")

# Enable detailed output on test failure
set(CTEST_OUTPUT_ON_FAILURE ON CACHE BOOL "Output on test failure")

# ============================================================================
# Production Readiness Test Suites (Added for Testing & CI requirement)
# ============================================================================

# Test configuration options
option(ENABLE_PRODUCTION_TESTS "Enable production readiness tests" ON)
option(ENABLE_READINESS_TESTS "Enable readiness checker tests" ON)
option(ENABLE_MASM_BUILD_TESTS "Enable MASM build integration tests" ON)
option(ENABLE_UI_WORKFLOW_TESTS "Enable UI workflow tests" ON)

if(ENABLE_PRODUCTION_TESTS)
    message(STATUS "Production readiness tests enabled")
    
    # Find Google Test and Google Mock
    include(GoogleTest)
    
    # ============================================================================
    # Readiness Checker Regression Tests
    # ============================================================================
    
    if(ENABLE_READINESS_TESTS)
        add_executable(test_readiness_checker
            unit/test_readiness_checker.cpp
        )
        
        target_link_libraries(test_readiness_checker
            PRIVATE
                gtest
                gtest_main
                gmock
                gmock_main
                Qt5::Core
                Qt5::Network
        )
        
        target_include_directories(test_readiness_checker
            PRIVATE
                ${CMAKE_SOURCE_DIR}/src
                ${CMAKE_SOURCE_DIR}/src/monitoring
        )
        
        set_target_properties(test_readiness_checker PROPERTIES
            AUTOMOC ON
            AUTORCC ON
            AUTOUIC ON
        )
        
        gtest_discover_tests(test_readiness_checker
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/unit
            PROPERTIES
                LABELS "unit;readiness;production"
                TIMEOUT 60
        )
        
        message(STATUS "  - Readiness checker tests configured")
    endif()
    
    # ============================================================================
    # MASM Build Integration Tests
    # ============================================================================
    
    if(ENABLE_MASM_BUILD_TESTS)
        add_executable(test_masm_build_integration
            integration/test_masm_build_integration.cpp
        )
        
        target_link_libraries(test_masm_build_integration
            PRIVATE
                gtest
                gtest_main
                gmock
                gmock_main
                Qt5::Core
                Qt5::Widgets
        )
        
        target_include_directories(test_masm_build_integration
            PRIVATE
                ${CMAKE_SOURCE_DIR}/src
        )
        
        set_target_properties(test_masm_build_integration PROPERTIES
            AUTOMOC ON
            AUTORCC ON
            AUTOUIC ON
        )
        
        gtest_discover_tests(test_masm_build_integration
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/integration
            PROPERTIES
                LABELS "integration;masm;production"
                TIMEOUT 300
        )
        
        message(STATUS "  - MASM build integration tests configured")
    endif()
    
    # ============================================================================
    # UI Workflow Tests (Project Explorer & Hotpatch)
    # ============================================================================
    
    if(ENABLE_UI_WORKFLOW_TESTS)
        add_executable(test_project_explorer_hotpatch
            ui/test_project_explorer_hotpatch.cpp
        )
        
        target_link_libraries(test_project_explorer_hotpatch
            PRIVATE
                gtest
                gtest_main
                gmock
                gmock_main
                Qt5::Core
                Qt5::Widgets
                Qt5::Test
        )
        
        target_include_directories(test_project_explorer_hotpatch
            PRIVATE
                ${CMAKE_SOURCE_DIR}/src
                ${CMAKE_SOURCE_DIR}/src/ui
        )
        
        set_target_properties(test_project_explorer_hotpatch PROPERTIES
            AUTOMOC ON
            AUTORCC ON
            AUTOUIC ON
        )
        
        gtest_discover_tests(test_project_explorer_hotpatch
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/ui
            PROPERTIES
                LABELS "ui;workflow;production"
                TIMEOUT 120
                ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        )
        
        message(STATUS "  - UI workflow tests configured")
    endif()
    
    # ============================================================================
    # Custom Test Targets
    # ============================================================================
    
    # Run all production readiness tests
    add_custom_target(production-tests
        COMMAND ${CMAKE_CTEST_COMMAND} -L production --output-on-failure --verbose
        DEPENDS 
            ${ENABLE_READINESS_TESTS:+test_readiness_checker}
            ${ENABLE_MASM_BUILD_TESTS:+test_masm_build_integration}
            ${ENABLE_UI_WORKFLOW_TESTS:+test_project_explorer_hotpatch}
        COMMENT "Running production readiness test suite..."
    )
    
    # Run only readiness tests
    if(ENABLE_READINESS_TESTS)
        add_custom_target(readiness-tests
            COMMAND test_readiness_checker --gtest_output=xml:readiness-results.xml
            DEPENDS test_readiness_checker
            COMMENT "Running readiness checker tests with mocked network/ports/disk..."
        )
    endif()
    
    # Run only MASM build tests
    if(ENABLE_MASM_BUILD_TESTS)
        add_custom_target(masm-build-tests
            COMMAND test_masm_build_integration --gtest_output=xml:masm-build-results.xml
            DEPENDS test_masm_build_integration
            COMMENT "Running MASM build integration tests with Problems panel parsing..."
        )
    endif()
    
    # Run only UI workflow tests
    if(ENABLE_UI_WORKFLOW_TESTS)
        add_custom_target(ui-workflow-tests
            COMMAND test_project_explorer_hotpatch --gtest_output=xml:ui-workflow-results.xml
            DEPENDS test_project_explorer_hotpatch
            COMMENT "Running UI tests for Project Explorer and hotpatch workflows..."
        )
    endif()
    
    # ============================================================================
    # Test Installation
    # ============================================================================
    
    install(TARGETS 
        ${ENABLE_READINESS_TESTS:+test_readiness_checker}
        ${ENABLE_MASM_BUILD_TESTS:+test_masm_build_integration}
        ${ENABLE_UI_WORKFLOW_TESTS:+test_project_explorer_hotpatch}
        RUNTIME DESTINATION tests
        OPTIONAL
    )
    
    message(STATUS "Production readiness test suite configured successfully")
endif()

# ============================================================================
# NEW COMPREHENSIVE TEST SUITES (Production-Ready)
# ============================================================================
# Four major test suites with full observability and metrics collection
# - Terminal Integration Tests (13 tests)
# - Model Loading Tests (15 tests)
# - Streaming Inference Tests (15 tests)
# - Agent Coordination Tests (15 tests)

# Fetch GoogleTest if not already available
if(NOT TARGET gtest)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
    )
    set(INSTALL_GTEST OFF CACHE BOOL "Disable gtest installation" FORCE)
    FetchContent_MakeAvailable(googletest)
endif()

# Enable testing at the root level
enable_testing()

# ============================================================================
# TEST METRICS AND LOGGING INFRASTRUCTURE
# ============================================================================
# Provides structured metrics collection and observability for all tests

add_library(test_infrastructure STATIC
    ../src/testing/test_metrics.cpp
    ../src/testing/test_logging.cpp
)

target_include_directories(test_infrastructure PUBLIC
    ${CMAKE_SOURCE_DIR}/src/testing
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(test_infrastructure PUBLIC
    Qt6::Core
    gtest
    gtest_main
)

# ============================================================================
# 1. TERMINAL INTEGRATION TESTS (13 tests)
# ============================================================================
# Tests: Process spawning, I/O capture, error parsing
# Latency <5000ms, handles large output (~10MB), real-time streaming

if(EXISTS "${CMAKE_SOURCE_DIR}/tests/unit/test_terminal_integration.cpp")
    add_executable(test_terminal_integration
        unit/test_terminal_integration.cpp
    )
    
    target_link_libraries(test_terminal_integration PRIVATE
        gtest
        gtest_main
        Qt6::Core
        Qt6::Concurrent
        test_infrastructure
    )
    
    target_include_directories(test_terminal_integration PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/testing
    )
    
    set_target_properties(test_terminal_integration PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
        FOLDER "Tests/Unit"
    )
    
    # Register with ctest
    gtest_discover_tests(test_terminal_integration
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
        PROPERTIES LABELS "terminal;integration;unit"
        TEST_PREFIX "TerminalIntegration."
    )
    
    message(STATUS "✓ Terminal Integration Tests: 13 test cases registered")
else()
    message(WARNING "test_terminal_integration.cpp not found - skipping terminal tests")
endif()

# ============================================================================
# 2. MODEL LOADING TESTS (15 tests)
# ============================================================================
# Tests: GGUF format parsing, memory mapping, quantization validation
# Supports 7B/13B models at Q4_0/Q5_0/F16 quantizations
# Concurrent loading, checksum validation, memory estimation

if(EXISTS "${CMAKE_SOURCE_DIR}/tests/unit/test_model_loading.cpp")
    add_executable(test_model_loading
        unit/test_model_loading.cpp
    )
    
    target_link_libraries(test_model_loading PRIVATE
        gtest
        gtest_main
        Qt6::Core
        Qt6::Concurrent
        test_infrastructure
    )
    
    target_include_directories(test_model_loading PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/testing
    )
    
    set_target_properties(test_model_loading PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
        FOLDER "Tests/Unit"
    )
    
    # Register with ctest
    gtest_discover_tests(test_model_loading
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
        PROPERTIES LABELS "model;loading;unit"
        TEST_PREFIX "ModelLoading."
    )
    
    message(STATUS "✓ Model Loading Tests: 15 test cases registered")
else()
    message(WARNING "test_model_loading.cpp not found - skipping model loading tests")
endif()

# ============================================================================
# 3. STREAMING INFERENCE TESTS (15 tests)
# ============================================================================
# Tests: Async token generation, latency tracking, context management
# Backpressure handling, stop sequence detection, memory tracking
# Concurrent sessions, throughput measurement (tokens/sec)

if(EXISTS "${CMAKE_SOURCE_DIR}/tests/unit/test_streaming_inference.cpp")
    add_executable(test_streaming_inference
        unit/test_streaming_inference.cpp
    )
    
    target_link_libraries(test_streaming_inference PRIVATE
        gtest
        gtest_main
        Qt6::Core
        Qt6::Concurrent
        test_infrastructure
    )
    
    target_include_directories(test_streaming_inference PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/testing
    )
    
    set_target_properties(test_streaming_inference PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
        FOLDER "Tests/Unit"
    )
    
    # Register with ctest
    gtest_discover_tests(test_streaming_inference
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
        PROPERTIES LABELS "streaming;inference;unit"
        TEST_PREFIX "StreamingInference."
    )
    
    message(STATUS "✓ Streaming Inference Tests: 15 test cases registered")
else()
    message(WARNING "test_streaming_inference.cpp not found - skipping streaming inference tests")
endif()

# ============================================================================
# 4. AGENT COORDINATION TESTS (15 tests)
# ============================================================================
# Tests: Task routing, agent load balancing, state transitions
# Retry mechanisms, error recovery, concurrent task distribution
# Agent availability tracking, priority task handling

if(EXISTS "${CMAKE_SOURCE_DIR}/tests/unit/test_agent_coordination.cpp")
    add_executable(test_agent_coordination
        unit/test_agent_coordination.cpp
    )
    
    target_link_libraries(test_agent_coordination PRIVATE
        gtest
        gtest_main
        Qt6::Core
        Qt6::Concurrent
        test_infrastructure
    )
    
    target_include_directories(test_agent_coordination PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/testing
    )
    
    set_target_properties(test_agent_coordination PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
        FOLDER "Tests/Unit"
    )
    
    # Register with ctest
    gtest_discover_tests(test_agent_coordination
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
        PROPERTIES LABELS "agent;coordination;unit"
        TEST_PREFIX "AgentCoordination."
    )
    
    message(STATUS "✓ Agent Coordination Tests: 15 test cases registered")
else()
    message(WARNING "test_agent_coordination.cpp not found - skipping agent coordination tests")
endif()

# ============================================================================
# TEST SUITE EXECUTION TARGETS
# ============================================================================

# Run all new comprehensive tests
add_custom_target(comprehensive-tests
    COMMAND ${CMAKE_CTEST_COMMAND} 
        -L "terminal|model|streaming|agent" 
        --output-on-failure 
        -V
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
    COMMENT "Running all comprehensive test suites (58 total tests)..."
)

# Run only unit tests
add_custom_target(unit-tests
    COMMAND ${CMAKE_CTEST_COMMAND} 
        -L "unit" 
        --output-on-failure 
        -V
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
    COMMENT "Running unit tests only..."
)

# Run unit tests with parallel execution (4 jobs)
add_custom_target(unit-tests-parallel
    COMMAND ${CMAKE_CTEST_COMMAND} 
        -L "unit" 
        --output-on-failure 
        -V
        -j 4
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
    COMMENT "Running unit tests in parallel (4 jobs)..."
)

# Quick smoke test (terminal + model loading only)
add_custom_target(smoke-tests
    COMMAND ${CMAKE_CTEST_COMMAND} 
        -L "terminal|model" 
        --output-on-failure
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
    COMMENT "Running smoke tests (28 tests)..."
)

# Performance tests with timing collection
add_custom_target(performance-tests
    COMMAND ${CMAKE_CTEST_COMMAND} 
        -L "unit" 
        --output-on-failure 
        --verbose
        --timeout 600
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
    COMMENT "Running performance tests with timing collection..."
)

# Generate test metrics report (requires test infrastructure)
add_custom_target(test-metrics-report
    COMMAND ${CMAKE_COMMAND} -E echo "Test metrics collection:"
    COMMAND ${CMAKE_COMMAND} -E echo "  - Prometheus format: test_metrics.prom"
    COMMAND ${CMAKE_COMMAND} -E echo "  - JSON report: test_metrics.json"
    COMMAND ${CMAKE_COMMAND} -E echo "  - HTML dashboard: test_dashboard.html"
    COMMAND ${CMAKE_COMMAND} -E echo "  - CSV export: test_results.csv"
    COMMENT "Test infrastructure ready for metrics export..."
)

# ============================================================================
# TEST SHARDING CONFIGURATION
# ============================================================================
# Supports splitting tests across multiple machines/threads
# Usage: ctest -I 1,10,5 -j 4  (tests 1-10, 5 per chunk, 4 parallel)

set(CTEST_MAX_PARALLEL_LEVEL 4 CACHE STRING "Maximum parallel test execution level")
set(CTEST_LABELS_FOR_SUBPROJECTS "terminal;model;streaming;agent" CACHE STRING "Test categories for organization")

# ============================================================================
# TEST SUMMARY AND NEXT STEPS
# ============================================================================

message(STATUS "")
message(STATUS "==================== TEST CONFIGURATION SUMMARY ====================")
message(STATUS "")
message(STATUS "✓ COMPREHENSIVE TEST SUITES REGISTERED:")
message(STATUS "  - Terminal Integration: 13 tests (process spawning, I/O, error parsing)")
message(STATUS "  - Model Loading: 15 tests (GGUF format, memory management, quantization)")
message(STATUS "  - Streaming Inference: 15 tests (token streaming, latency, throughput)")
message(STATUS "  - Agent Coordination: 15 tests (task routing, load balancing, state mgmt)")
message(STATUS "  TOTAL: 58 production-ready test cases")
message(STATUS "")
message(STATUS "✓ TEST INFRASTRUCTURE:")
message(STATUS "  - Metrics Collection: Prometheus + JSON + CSV export")
message(STATUS "  - Structured Logging: JSON + HTML + text formats")
message(STATUS "  - Percentile Analysis: p50/p95/p99 latency tracking")
message(STATUS "  - Slow Test Detection: Identifies >150% of baseline")
message(STATUS "  - Flaky Test Detection: Finds mixed pass/fail patterns")
message(STATUS "")
message(STATUS "EXECUTION COMMANDS:")
message(STATUS "  ctest -L unit                    # Run all unit tests")
message(STATUS "  ctest -L unit -j 4               # Run in parallel (4 jobs)")
message(STATUS "  make comprehensive-tests         # Run all 58 tests")
message(STATUS "  make smoke-tests                 # Quick smoke test (28 tests)")
message(STATUS "  make unit-tests-parallel         # Parallel execution")
message(STATUS "  ctest -R TerminalIntegration     # Run terminal tests only")
message(STATUS "  ctest -R ModelLoading            # Run model loading tests only")
message(STATUS "  ctest -R StreamingInference      # Run streaming tests only")
message(STATUS "  ctest -R AgentCoordination       # Run agent coordination tests only")
message(STATUS "")
message(STATUS "CONTINUOUS INTEGRATION:")
message(STATUS "  GitHub Actions workflows configured for:")
message(STATUS "    - macOS CI (Intel + Apple Silicon)")
message(STATUS "    - Nightly builds with fuzz/stress/memory tests")
message(STATUS "    - Performance regression tracking (baseline comparison)")
message(STATUS "    - Staging blue-green deployment")
message(STATUS "")
message(STATUS "NEXT STEPS:")
message(STATUS "  1. Run: make comprehensive-tests")
message(STATUS "  2. Review test output and metrics")
message(STATUS "  3. Configure GitHub Actions secrets (REGISTRY_USERNAME, SLACK_WEBHOOK)")
message(STATUS "  4. Create Dockerfile for staging deployment")
message(STATUS "  5. Set baseline metrics on main branch")
message(STATUS "")
message(STATUS "=====================================================================")
message(STATUS "")
