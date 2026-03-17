# Win32 IDE target (Qt-free)

cmake_minimum_required(VERSION 3.20)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Compute repository root regardless of entrypoint
get_filename_component(RAWRXD_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

# Collect Win32 sources (populated by migration script)
file(GLOB_RECURSE WIN32_IDE_SOURCES
    ${RAWRXD_ROOT}/src/win32/*.cpp
)

if(WIN32 AND WIN32_IDE_SOURCES)
    message(STATUS "Configuring AgenticIDEWin (Qt-free)")

    add_executable(AgenticIDEWin WIN32
        ${WIN32_IDE_SOURCES}
    )

    target_include_directories(AgenticIDEWin PRIVATE
        ${RAWRXD_ROOT}/include/win32
        ${RAWRXD_ROOT}/include/win32/agentic
        ${RAWRXD_ROOT}/include/win32/image_generator
        ${RAWRXD_ROOT}/include/win32/paint
        ${RAWRXD_ROOT}/include
    )

    # Prefer Win32 include headers over others to avoid name collisions
    target_compile_definitions(AgenticIDEWin PRIVATE
        RAWRXD_WIN32_IDE=1
        NOMINMAX
        WIN32_LEAN_AND_MEAN
    )

    if(MSVC)
        target_link_options(AgenticIDEWin PRIVATE /SUBSYSTEM:WINDOWS)
    endif()

    target_link_libraries(AgenticIDEWin PRIVATE
        user32 gdi32 comctl32 comdlg32 windowscodecs ws2_32 winhttp ole32 winmm
    )

    # Ensure C++ standard at target-level for MSVC generators
    set_property(TARGET AgenticIDEWin PROPERTY CXX_STANDARD 20)
    set_property(TARGET AgenticIDEWin PROPERTY CXX_STANDARD_REQUIRED ON)

    set_target_properties(AgenticIDEWin PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )
    list(LENGTH WIN32_IDE_SOURCES _count)
    message(STATUS "✓ AgenticIDEWin configured with ${_count} sources")
else()
    message(STATUS "AgenticIDEWin sources not present yet (run scripts/win32_migration.ps1)")
endif()
