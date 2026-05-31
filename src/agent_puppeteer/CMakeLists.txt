# ============================================================================
# CMakeLists.txt — Agent Puppeteer Module
# ============================================================================
# Builds the agent_puppeteer extension for RawrXD.
# Integrates with existing SubAgentManager and AgenticExecutorController.
# ============================================================================

cmake_minimum_required(VERSION 3.17)

# Agent Puppeteer library
add_library(agent_puppeteer STATIC
    agent_puppeteer.cpp
)

target_include_directories(agent_puppeteer
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)

# Link with existing RawrXD components
target_link_libraries(agent_puppeteer
    PUBLIC
        subagent_core  # Existing SubAgentManager
        # agentic_controller  # Existing AgenticExecutorController (if separate lib)
)

# C++17 required
target_compile_features(agent_puppeteer PUBLIC cxx_std_17)

# Example executable
add_executable(agent_puppeteer_demo
    agent_puppeteer_example.cpp
)

target_link_libraries(agent_puppeteer_demo
    PRIVATE
        agent_puppeteer
)

# Tests (if test framework available)
if(BUILD_TESTS)
    add_executable(agent_puppeteer_test
        agent_puppeteer_test.cpp
    )
    target_link_libraries(agent_puppeteer_test
        PRIVATE
            agent_puppeteer
            gtest_main
    )
    add_test(NAME agent_puppeteer_test COMMAND agent_puppeteer_test)
endif()
