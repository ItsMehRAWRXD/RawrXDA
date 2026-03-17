cmake_minimum_required(VERSION 3.10)
project(OOP_AI_Renderer)

set(CMAKE_CXX_STANDARD 17)

# Find OpenGL
find_package(OpenGL REQUIRED)

# Add executable
add_executable(test_scene
    src/renderer/renderer.cpp
    src/renderer/main.cpp
)

# Link libraries
target_link_libraries(test_scene ${OPENGL_LIBRARIES})

# Include directories
target_include_directories(test_scene PRIVATE src/renderer)

# Shader files (copy to build directory)
file(COPY src/renderer/pulse.vert DESTINATION ${CMAKE_BINARY_DIR}/shaders)
file(COPY src/renderer/pulse.frag DESTINATION ${CMAKE_BINARY_DIR}/shaders)