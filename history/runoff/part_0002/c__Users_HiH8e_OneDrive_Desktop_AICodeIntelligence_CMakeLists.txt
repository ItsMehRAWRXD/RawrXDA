cmake_minimum_required(VERSION 3.16)

project(AICodeIntelligence LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(aicodeintelligence
    src/main.cpp
    src/utils.cpp
    src/indexer.cpp
    src/security_analyzer.cpp
    src/analyzers/clike_analyzer.cpp
    src/analyzers/python_analyzer.cpp
)
if(MSVC)
    # For future HTTP server (Winsock); harmless if unused here
    target_link_libraries(aicodeintelligence PRIVATE ws2_32)
endif()

target_include_directories(aicodeintelligence PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

if(MSVC)
    target_compile_options(aicodeintelligence PRIVATE /W4 /permissive-)
else()
    target_compile_options(aicodeintelligence PRIVATE -Wall -Wextra -Wpedantic)
endif()

enable_testing()

add_executable(aicodeintelligence_tests
    src/utils.cpp
    src/indexer.cpp
    src/security_analyzer.cpp
    src/analyzers/clike_analyzer.cpp
    src/analyzers/python_analyzer.cpp
    tests/test_main.cpp
)
if(MSVC)
    target_link_libraries(aicodeintelligence_tests PRIVATE ws2_32)
endif()
target_include_directories(aicodeintelligence_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
if(MSVC)
    target_compile_options(aicodeintelligence_tests PRIVATE /W4 /permissive-)
else()
    target_compile_options(aicodeintelligence_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME smoke COMMAND aicodeintelligence_tests)
