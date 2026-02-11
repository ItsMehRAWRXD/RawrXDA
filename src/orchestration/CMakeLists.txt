cmake_minimum_required(VERSION 3.20)
project(RawrXDOrchestration)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Qt-free: uses STL + WinSock + Win32 audio APIs
add_library(RawrXDOrchestration STATIC
    llm_router.hpp
    llm_router.cpp
    agent_coordinator.hpp
    agent_coordinator.cpp
    voice_processor.hpp
    voice_processor.cpp
)

target_link_libraries(RawrXDOrchestration
    PUBLIC
        $<$<PLATFORM_ID:Windows>:ws2_32>
        $<$<PLATFORM_ID:Windows>:winmm>
        $<$<PLATFORM_ID:Windows>:winhttp>
)

target_include_directories(RawrXDOrchestration
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)

# Enable warnings
if(MSVC)
    target_compile_options(RawrXDOrchestration PRIVATE /W4)
else()
    target_compile_options(RawrXDOrchestration PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Optimization
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(RawrXDOrchestration PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/O2>
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-O3>
    )
endif()
