cmake_minimum_required(VERSION 3.16)
project(RawrXD-QtShell)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 COMPONENTS Widgets REQUIRED)

add_executable(RawrXD-QtShell
    main.cpp
    MainWindow.cpp
    MainWindow.h
    HexMagConsole.cpp
    HexMagConsole.h
    unified_hotpatch_manager.cpp
    unified_hotpatch_manager.hpp
)

target_link_libraries(RawrXD-QtShell PRIVATE Qt6::Widgets)
