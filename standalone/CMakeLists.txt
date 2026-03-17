# Standalone build wrapper for the Qt-free web bridge.
# This exists so `cmake -S D:/rawrxd/standalone -B ...` works with standard CMake CLI.

cmake_minimum_required(VERSION 3.16)
project(RawrXD-Standalone-WebBridge VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)

if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

set(STANDALONE_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/../src/standalone_web_bridge.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../src/standalone_main.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../src/agent/gguf_proxy_server.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../src/agent/agent_hot_patcher.cpp"
)

add_executable(rawrxd-standalone ${STANDALONE_SOURCES})
target_include_directories(rawrxd-standalone PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/../src"
    "${CMAKE_CURRENT_LIST_DIR}/../3rdparty"
    "${CMAKE_CURRENT_LIST_DIR}/../3rdparty/nlohmann"
)
target_link_libraries(rawrxd-standalone PRIVATE Threads::Threads)

if(WIN32)
    target_link_libraries(rawrxd-standalone PRIVATE ws2_32 advapi32 crypt32)
endif()

install(TARGETS rawrxd-standalone RUNTIME DESTINATION bin)

file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/web)
configure_file("${CMAKE_CURRENT_LIST_DIR}/../standalone_interface.html"
               ${CMAKE_BINARY_DIR}/web/standalone_interface.html COPYONLY)
