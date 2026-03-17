cmake_minimum_required(VERSION 3.16)
project(GlyphSmith VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# OpenGL and GLFW dependencies
find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GLEW REQUIRED glew)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${GLEW_INCLUDE_DIRS})

# Source files
file(GLOB_RECURSE RENDERER_SOURCES "src/renderer/*.cpp" "src/renderer/*.h")
file(GLOB_RECURSE CORE_SOURCES "src/core/*.cpp" "src/core/*.h")

# Main executable
add_executable(glyph_smith
    src/main.cpp
    ${RENDERER_SOURCES}
    ${CORE_SOURCES}
)

# Link libraries
target_link_libraries(glyph_smith 
    OpenGL::GL 
    glfw 
    ${GLEW_LIBRARIES}
)

# Compiler flags for performance
target_compile_options(glyph_smith PRIVATE
    -O3 -march=native -mtune=native
    -Wall -Wextra -Wpedantic
    -ffast-math -funroll-loops
)

# Test scene executable
add_executable(test_scene
    src/test_scene.cpp
    ${RENDERER_SOURCES}
    ${CORE_SOURCES}
)

target_link_libraries(test_scene 
    OpenGL::GL 
    glfw 
    ${GLEW_LIBRARIES}
)

# Shader directory
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/shaders DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

# Build output
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/build)

# Custom target for agent testing
add_custom_target(agent_test
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/build/test_scene
    DEPENDS test_scene
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Running agent-generated test scene"
)

# Installation
install(TARGETS glyph_smith test_scene
    RUNTIME DESTINATION bin
)