# CMakeLists.txt - Build System Migration Guide
# Transition from Qt-based to pure Win32 build system

# ============================================================================
# BEFORE: Qt-Based CMakeLists.txt (DEPRECATED)
# ============================================================================

# cmake_minimum_required(VERSION 3.16)
# project(RawrXD VERSION 1.0.0 LANGUAGES CXX)
# 
# set(CMAKE_CXX_STANDARD 17)
# set(CMAKE_CXX_STANDARD_REQUIRED ON)
# 
# # ❌ Qt Framework Setup
# find_package(Qt5 REQUIRED COMPONENTS Core Widgets Gui Network)
# 
# # ❌ Qt MOC/RCC Processing
# set(CMAKE_AUTOMOC ON)
# set(CMAKE_AUTORCC ON)
# set(CMAKE_INCLUDE_CURRENT_DIR ON)
# 
# # Source files with Qt-based code
# set(SOURCES
#     src/qtapp/main_qt.cpp
#     src/qtapp/MainWindow.cpp
#     src/qtapp/HexMagConsole.cpp
#     src/agentic/agentic_executor.cpp
#     # ... Qt-dependent files
# )
# 
# add_executable(RawrXD_IDE ${SOURCES})
# target_link_libraries(RawrXD_IDE PRIVATE Qt5::Core Qt5::Widgets Qt5::Gui Qt5::Network)


# ============================================================================
# AFTER: Win32-Based CMakeLists.txt (NEW)
# ============================================================================

cmake_minimum_required(VERSION 3.20)
project(RawrXD_Win32 VERSION 1.0.0 LANGUAGES CXX)

# ============================================================================
# Compiler & C++ Standard
# ============================================================================
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHsc /W4 /DNOMINMAX")

# ============================================================================
# RawrXD Component Paths
# ============================================================================
set(RAWRXD_SRC_DIR "${CMAKE_SOURCE_DIR}/src")
set(RAWRXD_SHIP_DIR "D:/RawrXD/Ship")
set(RAWRXD_INCLUDE_DIR "${RAWRXD_SRC_DIR}")

# Add Ship directory to linker search path for DLL imports
link_directories(${RAWRXD_SHIP_DIR})

# ============================================================================
# Core Win32 Libraries (from previous migrations)
# ============================================================================
# All Win32 components are available as pre-built DLLs/LIBs in Ship/

set(RAWRXD_LIBS
    RawrXD_Foundation.lib
    RawrXD_MainWindow_Win32.lib
    RawrXD_TerminalManager_Win32.lib
    RawrXD_Executor.lib
    RawrXD_TextEditor_Win32.lib
    RawrXD_PlanOrchestrator.lib
    RawrXD_AgentCoordinator.lib
    RawrXD_FileManager_Win32.lib
    RawrXD_SettingsManager_Win32.lib
)

# ============================================================================
# System Libraries (Win32 API)
# ============================================================================
set(SYSTEM_LIBS
    kernel32.lib
    user32.lib
    gdi32.lib
    winspool.lib
    comdlg32.lib
    advapi32.lib
    shell32.lib
    ole32.lib
    oleaut32.lib
    uuid.lib
    comctl32.lib
    imm32.lib
    wsock32.lib
    ws2_32.lib
    mswsock.lib
    shlobj.lib
)

# ============================================================================
# Include Directories
# ============================================================================
include_directories(
    ${RAWRXD_INCLUDE_DIR}
    ${RAWRXD_SHIP_DIR}
    "C:/Program Files (x86)/Windows Kits/10/Include/10.0.22621.0/um"
)

# ============================================================================
# EXECUTABLE: RawrXD_IDE (Main Application)
# ============================================================================
set(IDE_SOURCES
    # MIGRATED: Win32 entry point (was main_qt.cpp)
    src/qtapp/main_win32.cpp
    
    # MIGRATED: Win32 main window (was MainWindow.cpp)
    src/qtapp/MainWindow_Win32.cpp
    
    # MIGRATED: Win32 terminal widget (was TerminalWidget.cpp)
    src/qtapp/TerminalWidget_Win32.cpp
    
    # MIGRATED: Win32 hex console (was HexMagConsole.cpp)
    src/qtapp/HexConsole_Win32.cpp
)

add_executable(RawrXD_IDE WIN32 ${IDE_SOURCES})

# Link RawrXD components (Win32 versions)
target_link_libraries(RawrXD_IDE PRIVATE
    RawrXD_Foundation.lib
    RawrXD_MainWindow_Win32.lib
    RawrXD_TerminalManager_Win32.lib
    RawrXD_TextEditor_Win32.lib
)

# Link system libraries
target_link_libraries(RawrXD_IDE PRIVATE ${SYSTEM_LIBS})

# ============================================================================
# LIBRARY: RawrXD_Executor (Agentic Execution Engine)
# ============================================================================
set(EXECUTOR_SOURCES
    src/agentic/agentic_executor_win32.cpp
    src/agentic/tool_execution_engine.cpp
)

add_library(RawrXD_Executor SHARED ${EXECUTOR_SOURCES})

target_link_libraries(RawrXD_Executor PRIVATE
    RawrXD_Foundation.lib
    ${SYSTEM_LIBS}
)

# Export DLL
set_target_properties(RawrXD_Executor PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${RAWRXD_SHIP_DIR}
    RUNTIME_OUTPUT_DIRECTORY ${RAWRXD_SHIP_DIR}
)

# ============================================================================
# LIBRARY: RawrXD_PlanOrchestrator (Planning & Orchestration)
# ============================================================================
set(ORCHESTRATOR_SOURCES
    src/orchestration/plan_orchestrator_win32.cpp
    src/orchestration/task_scheduler.cpp
)

add_library(RawrXD_PlanOrchestrator SHARED ${ORCHESTRATOR_SOURCES})

target_link_libraries(RawrXD_PlanOrchestrator PRIVATE
    RawrXD_Foundation.lib
    RawrXD_Executor.lib
    ${SYSTEM_LIBS}
)

set_target_properties(RawrXD_PlanOrchestrator PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${RAWRXD_SHIP_DIR}
    RUNTIME_OUTPUT_DIRECTORY ${RAWRXD_SHIP_DIR}
)

# ============================================================================
# LIBRARY: RawrXD_FileManager (File System Operations)
# ============================================================================
set(FILEMANAGER_SOURCES
    src/utils/qt_directory_manager_win32.cpp
    src/utils/file_operations.cpp
)

add_library(RawrXD_FileManager_Win32 SHARED ${FILEMANAGER_SOURCES})

target_link_libraries(RawrXD_FileManager_Win32 PRIVATE
    RawrXD_Foundation.lib
    ${SYSTEM_LIBS}
)

set_target_properties(RawrXD_FileManager_Win32 PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${RAWRXD_SHIP_DIR}
    RUNTIME_OUTPUT_DIRECTORY ${RAWRXD_SHIP_DIR}
)

# ============================================================================
# LIBRARY: RawrXD_SettingsManager (Registry-Based Configuration)
# ============================================================================
set(SETTINGS_SOURCES
    src/utils/qt_settings_wrapper_win32.cpp
    src/utils/registry_manager.cpp
)

add_library(RawrXD_SettingsManager_Win32 SHARED ${SETTINGS_SOURCES})

target_link_libraries(RawrXD_SettingsManager_Win32 PRIVATE
    RawrXD_Foundation.lib
    ${SYSTEM_LIBS}
)

set_target_properties(RawrXD_SettingsManager_Win32 PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${RAWRXD_SHIP_DIR}
    RUNTIME_OUTPUT_DIRECTORY ${RAWRXD_SHIP_DIR}
)

# ============================================================================
# Post-Build: Copy DLLs to Output Directory
# ============================================================================
add_custom_command(TARGET RawrXD_IDE POST_BUILD
    COMMENT "Copying RawrXD DLLs to output directory..."
    COMMAND ${CMAKE_COMMAND} -E copy
        ${RAWRXD_SHIP_DIR}/RawrXD_Foundation.dll
        ${RAWRXD_SHIP_DIR}/RawrXD_MainWindow_Win32.dll
        ${RAWRXD_SHIP_DIR}/RawrXD_TerminalManager_Win32.dll
        ${RAWRXD_SHIP_DIR}/RawrXD_Executor.dll
        ${RAWRXD_SHIP_DIR}/RawrXD_TextEditor_Win32.dll
        ${RAWRXD_SHIP_DIR}/RawrXD_PlanOrchestrator.dll
        ${RAWRXD_SHIP_DIR}/RawrXD_FileManager_Win32.dll
        ${RAWRXD_SHIP_DIR}/RawrXD_SettingsManager_Win32.dll
        $<TARGET_FILE_DIR:RawrXD_IDE>/
)

# ============================================================================
# Diagnostic Output
# ============================================================================
message(STATUS "")
message(STATUS "=== RawrXD Win32 Build Configuration ===")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Source Directory: ${RAWRXD_SRC_DIR}")
message(STATUS "Component Directory: ${RAWRXD_SHIP_DIR}")
message(STATUS "")
message(STATUS "Linking Components:")
message(STATUS "  ✓ RawrXD_Foundation.lib")
message(STATUS "  ✓ RawrXD_MainWindow_Win32.lib")
message(STATUS "  ✓ RawrXD_Executor.lib")
message(STATUS "  ✓ RawrXD_PlanOrchestrator.lib")
message(STATUS "  ✓ RawrXD_FileManager_Win32.lib")
message(STATUS "  ✓ RawrXD_SettingsManager_Win32.lib")
message(STATUS "")
message(STATUS "Targets:")
message(STATUS "  • RawrXD_IDE (executable)")
message(STATUS "  • RawrXD_Executor (DLL)")
message(STATUS "  • RawrXD_PlanOrchestrator (DLL)")
message(STATUS "  • RawrXD_FileManager_Win32 (DLL)")
message(STATUS "  • RawrXD_SettingsManager_Win32 (DLL)")
message(STATUS "")

# ============================================================================
# Key Migration Changes
# ============================================================================
# ❌ REMOVED:
#    find_package(Qt5 ...)
#    set(CMAKE_AUTOMOC ON)
#    set(CMAKE_AUTORCC ON)
#    Qt5::Core, Qt5::Widgets, Qt5::Gui, Qt5::Network
#
# ✅ ADDED:
#    C++20 standard library (std::thread, std::mutex, std::function)
#    Win32 API libraries (kernel32, user32, gdi32, etc.)
#    RawrXD component DLLs (pre-built Win32 replacements)
#    link_directories for DLL import libraries
#
# ✅ ARCHITECTURE:
#    • Core executable links against DLL import libraries
#    • All Win32 replacements available in Ship directory
#    • No compile-time dependency on Qt
#    • All components follow DLL interface contract
