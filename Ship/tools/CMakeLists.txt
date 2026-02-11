cmake_minimum_required(VERSION 3.20)

# CLI Executable
add_executable(RawrXD_CLI cli_main.cpp)
target_include_directories(RawrXD_CLI PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(RawrXD_CLI PRIVATE ws2_32)

# Stress Test Executable  
add_executable(RawrXD_StressTest stress_test.cpp)
target_include_directories(RawrXD_StressTest PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(RawrXD_StressTest PRIVATE ws2_32)

# Install targets
install(TARGETS RawrXD_CLI RawrXD_StressTest RUNTIME DESTINATION bin)
