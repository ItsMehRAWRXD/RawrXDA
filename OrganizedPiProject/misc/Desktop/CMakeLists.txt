cmake_minimum_required(VERSION 3.10)
project(SpatialPartitioning)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find GLM library
find_package(glm QUIET)

# If GLM is not found, use our own vector implementation
if(NOT glm_FOUND)
    message(STATUS "GLM not found, using custom vector implementation")
    add_definitions(-DUSE_CUSTOM_VECTOR)
endif()

# Add include directories (exclude problematic system headers)
# include_directories(include)

# Create a simple test executable
add_executable(spatial_test
    src/spatial_test.cpp
)

# Core Engine Demo
add_executable(core_engine_demo
    src/core_engine_demo.cpp
    src/core_engine.cpp
    src/player_system.cpp
)

# Weapon System Demo
add_executable(weapon_system_demo
    src/weapon_system_impl.cpp
    src/weapon_system.cpp
)

# Weapon System Test
add_executable(weapon_test
    src/weapon_test.cpp
    src/weapon_system_impl.cpp
)

# Simple C++ Test
add_executable(simple_test
    src/simple_test.cpp
)

# Minimal Weapon System Test
add_executable(minimal_weapon_test
    src/minimal_weapon_test.cpp
)

# Link libraries
if(glm_FOUND)
    target_link_libraries(spatial_test glm::glm)
    target_link_libraries(core_engine_demo glm::glm)
    target_link_libraries(weapon_system_demo glm::glm)
    target_link_libraries(weapon_test glm::glm)
endif()

# Find and link GLFW
find_package(glfw3 QUIET)
if(glfw3_FOUND)
    target_link_libraries(core_engine_demo glfw)
else()
    # Try to find GLFW manually
    find_library(GLFW_LIBRARY NAMES glfw3 glfw)
    if(GLFW_LIBRARY)
        target_link_libraries(core_engine_demo ${GLFW_LIBRARY})
    else()
        message(WARNING "GLFW not found - core engine demo may not compile")
    endif()
endif()

# Find and link OpenGL
find_package(OpenGL QUIET)
if(OpenGL_FOUND)
    target_link_libraries(core_engine_demo OpenGL::GL)
else()
    # Platform-specific OpenGL linking
    if(WIN32)
        target_link_libraries(core_engine_demo opengl32)
    elseif(APPLE)
        target_link_libraries(core_engine_demo "-framework OpenGL")
    else()
        target_link_libraries(core_engine_demo GL)
    endif()
endif()

# Compiler-specific options
if(MSVC)
    target_compile_options(spatial_test PRIVATE /W4)
    target_compile_options(core_engine_demo PRIVATE /W4)
else()
    target_compile_options(spatial_test PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(core_engine_demo PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Set output directory
set_target_properties(core_engine_demo PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Install targets
install(TARGETS spatial_test core_engine_demo weapon_system_demo weapon_test simple_test minimal_weapon_test
    RUNTIME DESTINATION bin
)
