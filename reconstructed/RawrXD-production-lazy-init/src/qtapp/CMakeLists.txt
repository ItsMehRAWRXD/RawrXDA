cmake_minimum_required(VERSION 3.16)
project(RawrXD-QtShell)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 COMPONENTS Widgets Core Network Concurrent REQUIRED)
find_package(ZLIB REQUIRED)

add_executable(RawrXD-QtShell
    main_qt.cpp
    MainWindow.cpp
    MainWindow.h
    HexMagConsole.cpp
    HexMagConsole.h
    unified_hotpatch_manager.cpp
    unified_hotpatch_manager.hpp
        safe_mode_config.hpp
        safe_mode_config.cpp
    mainwindow_stub_implementations.cpp
    masm_function_stubs.cpp
)

target_link_libraries(RawrXD-QtShell 
    PRIVATE 
        Qt6::Widgets
        Qt6::Core
        Qt6::Network
        Qt6::Concurrent
        ZLIB::ZLIB
    RawrXD-Core
)

# Link Windows libraries for CreateThreadEx, CreatePipeEx
if(WIN32)
    target_link_libraries(RawrXD-QtShell 
        PRIVATE 
            kernel32
            user32
            shell32
            oleaut32
            ole32
    )
endif()

target_include_directories(RawrXD-QtShell PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_include_directories(RawrXD-QtShell PRIVATE "${CMAKE_SOURCE_DIR}/src/core")

