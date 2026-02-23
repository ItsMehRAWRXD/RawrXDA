# ===========================================================================
# autonomous_orchestrator.cmake — CMake fragment for Autonomous Orchestrator
# ===========================================================================
# Add this to your main CMakeLists.txt or include it:
#   include(autonomous_orchestrator.cmake)
# ===========================================================================

# Autonomous Orchestrator Library
add_library(autonomous_orchestrator STATIC
    src/agent/autonomous_orchestrator.cpp
    src/agent/autonomous_orchestrator.hpp
    src/agent/orchestrator_cli_handler.cpp
    src/agent/orchestrator_cli_handler.hpp
)

target_include_directories(autonomous_orchestrator PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/agent
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/nlohmann_json/include
)

target_link_libraries(autonomous_orchestrator PUBLIC
    agentic_deep_thinking_engine
    meta_planner
    autonomous_subagent
    agentic_failure_detector
    agentic_puppeteer
    nlohmann_json::nlohmann_json
)

target_compile_features(autonomous_orchestrator PUBLIC cxx_std_20)

# Add to main executables
if(TARGET RawrXD-Shell)
    target_link_libraries(RawrXD-Shell PRIVATE autonomous_orchestrator)
endif()

if(TARGET RawrXD_IDE)
    target_link_libraries(RawrXD_IDE PRIVATE autonomous_orchestrator)
endif()

# Optional: Standalone orchestrator CLI
add_executable(orchestrator_cli
    src/agent/orchestrator_cli_main.cpp
)

target_link_libraries(orchestrator_cli PRIVATE
    autonomous_orchestrator
)

# Installation
install(TARGETS autonomous_orchestrator
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(FILES
    src/agent/autonomous_orchestrator.hpp
    src/agent/orchestrator_cli_handler.hpp
    DESTINATION include/rawrxd
)

# Optional: Install PowerShell script
install(FILES
    autonomous_orchestrator_cli.ps1
    DESTINATION bin
    PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ
                GROUP_EXECUTE GROUP_READ
                WORLD_EXECUTE WORLD_READ
)

message(STATUS "✓ Autonomous Orchestrator configured")
message(STATUS "  - Multi-agent cycling: 1x-99x supported"  )
message(STATUS "  - Quality modes: Auto, Balance, Max")
message(STATUS "  - Self-adjusting time limits enabled")
message(STATUS "  - Production-ready validation framework included")
