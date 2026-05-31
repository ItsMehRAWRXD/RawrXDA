cmake_minimum_required(VERSION 3.20)
project(SovereignStreamer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set the output directory for the executable
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_executable(sovereign_streamer
    main.cpp
    sovereign_streamer.cpp
)

# Set compiler options
if(MSVC)
    target_compile_options(sovereign_streamer PRIVATE /arch:AVX512 /O2 /W3 /EHsc)
    target_link_libraries(sovereign_streamer PRIVATE kernel32.lib)
else()
    target_compile_options(sovereign_streamer PRIVATE -mavx512f -O3 -Wall)
endif()

message(STATUS "Sovereign Streamer build configured.")
