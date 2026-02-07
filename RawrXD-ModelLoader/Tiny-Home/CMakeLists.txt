cmake_minimum_required(VERSION 3.15)
project(TinyHome LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "-Os -static -s")

file(GLOB SOURCES "src/*.cpp")
add_executable(TinyHome ${SOURCES})
