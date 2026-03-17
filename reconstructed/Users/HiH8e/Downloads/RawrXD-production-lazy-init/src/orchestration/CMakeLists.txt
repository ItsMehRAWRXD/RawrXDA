cmake_minimum_required(VERSION 3.20)
project(RawrXDOrchestration)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 COMPONENTS Core Network Sql Multimedia Widgets REQUIRED)

add_library(RawrXDOrchestration STATIC
    llm_router.hpp
    llm_router.cpp
    agent_coordinator.hpp
    agent_coordinator.cpp
    voice_processor.hpp
    voice_processor.cpp
    qt6_audio_helper.hpp
    TaskOrchestrator.cpp
    OrchestrationUI.cpp
    ${CMAKE_SOURCE_DIR}/include/orchestration/TaskOrchestrator.h
    ${CMAKE_SOURCE_DIR}/include/orchestration/OrchestrationUI.h
)

target_link_libraries(RawrXDOrchestration
    PUBLIC
        Qt6::Core
        Qt6::Network
        Qt6::Sql
        Qt6::Multimedia
        Qt6::Widgets
)

target_include_directories(RawrXDOrchestration
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/..
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/orchestration
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
