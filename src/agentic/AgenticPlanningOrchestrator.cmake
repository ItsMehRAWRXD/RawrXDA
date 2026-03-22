# CMake snippet for AgenticPlanningOrchestrator
# Add to the main RawrXD CMakeLists.txt under src/agentic and src/win32app targets

# Agentic Planning Orchestrator sources
set(AGENTIC_PLANNING_SOURCES
    src/agentic/agentic_planning_orchestrator.hpp
    src/agentic/agentic_planning_orchestrator.cpp
)

# Win32 IDE Planning Panel sources  
set(WIN32_PLANNING_PANEL_SOURCES
    src/win32app/Win32IDE_AgenticPlanningPanel.hpp
    src/win32app/Win32IDE_AgenticPlanningPanel.cpp
)

# If building libRawrXD_Agentic:
# target_sources(RawrXD_Agentic PRIVATE ${AGENTIC_PLANNING_SOURCES})

# If building Win32 IDE executable:
# target_sources(RawrXD-Win32IDE PRIVATE ${WIN32_PLANNING_PANEL_SOURCES})
# target_sources(RawrXD-Win32IDE PRIVATE ${AGENTIC_PLANNING_SOURCES})

# Link nlohmann/json if not already linked
# target_link_libraries(RawrXD-Win32IDE PRIVATE nlohmann_json::nlohmann_json)

# Include directories
# target_include_directories(RawrXD-Win32IDE PRIVATE 
#     ${CMAKE_CURRENT_SOURCE_DIR}/src/agentic
#     ${CMAKE_CURRENT_SOURCE_DIR}/src/win32app
# )
