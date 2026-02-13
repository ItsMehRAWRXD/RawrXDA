# CMakeLists.txt for Feedback System Component
# RawrXD IDE - Community Feedback and Telemetry

cmake_minimum_required(VERSION 3.16)


set(FEEDBACK_SOURCES
    FeedbackSystem.cpp
)

set(FEEDBACK_HEADERS
    FeedbackSystem.hpp
)

add_library(rawrxd_feedback STATIC
    ${FEEDBACK_SOURCES}
    ${FEEDBACK_HEADERS}
)

# Qt-free: pure C++20
set_target_properties(rawrxd_feedback PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

target_link_libraries(rawrxd_feedback PUBLIC
)

target_include_directories(rawrxd_feedback PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
)

target_compile_definitions(rawrxd_feedback PRIVATE
    RAWRXD_FEEDBACK_ENABLED
)

message(STATUS "✓ Feedback System component configured")
