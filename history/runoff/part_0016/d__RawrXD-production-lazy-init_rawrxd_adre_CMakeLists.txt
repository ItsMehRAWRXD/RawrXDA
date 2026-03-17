cmake_minimum_required(VERSION 3.16)
project(RawrXD_ADRE)

set(CMAKE_CXX_STANDARD 17)

add_subdirectory(core)
add_subdirectory(ckg)
add_subdirectory(agents)
add_subdirectory(orchestrator)
add_subdirectory(replay)
add_subdirectory(integration)

add_executable(test_audit test_audit.cpp)
target_link_libraries(test_audit core ckg agents orchestrator replay integration)
