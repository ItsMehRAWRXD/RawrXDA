# CMakeLists.txt - Complete enterprise IDE build system
cmake_minimum_required(VERSION 3.20)

project(AutonomousIDE VERSION 1.0.0 LANGUAGES CXX)

# C++20 Standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler flags
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -Wall -Wextra -Wpedantic -fsanitize=address")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -DNDEBUG")

# Qt6 Configuration
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS
    Core
    Widgets
    Network
    Concurrent
    Test
)

# OpenSSL for encryption (optional - fallback to stubs if not found)
find_package(OpenSSL)
if(NOT OpenSSL_FOUND)
    message(STATUS "OpenSSL not found - using crypto stubs")
    set(OPENSSL_CRYPTO_LIBRARY "")
    set(OPENSSL_INCLUDE_DIR "")
endif()

# ZLIB for compression (optional - using internal compression)
find_package(ZLIB)
if(NOT ZLIB_FOUND)
    message(STATUS "ZLIB not found - using internal compression stubs")
endif()

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${OPENSSL_INCLUDE_DIR}
    ${ZLIB_INCLUDE_DIRS}
)

# Core library sources
set(CORE_SOURCES
    autonomous_model_manager.cpp
    intelligent_codebase_engine.cpp
    autonomous_feature_engine.cpp
    hybrid_cloud_manager.cpp
    error_recovery_system.cpp
    performance_monitor.cpp
    enterprise_security_manager.cpp
    enterprise_monitoring.cpp
)

set(CORE_HEADERS
    autonomous_model_manager.h
    intelligent_codebase_engine.h
    autonomous_feature_engine.h
    hybrid_cloud_manager.h
    error_recovery_system.h
    performance_monitor.h
    enterprise_security_manager.h
    enterprise_monitoring.h
)

# UI sources
set(UI_SOURCES
    ide_main_window.cpp
    autonomous_widgets.cpp
)

set(UI_HEADERS
    ide_main_window.h
)

# Create core library
add_library(AutonomousCore STATIC
    ${CORE_SOURCES}
    ${CORE_HEADERS}
)

target_link_libraries(AutonomousCore
    Qt6::Core
    Qt6::Network
    Qt6::Concurrent
    OpenSSL::SSL
    OpenSSL::Crypto
    ZLIB::ZLIB
)

# Create main executable (GUI)
add_executable(AutonomousIDE
    main.cpp
    ${UI_SOURCES}
    ${UI_HEADERS}
)

target_link_libraries(AutonomousIDE
    AutonomousCore
    Qt6::Core
    Qt6::Widgets
    Qt6::Network
    Qt6::Concurrent
)

# Create CLI executable
add_executable(orchestra-cli
    cli_main.cpp
)

target_include_directories(orchestra-cli PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(orchestra-cli
    Qt6::Core
)

if(WIN32)
    # Windows-specific settings
    set_target_properties(orchestra-cli PROPERTIES
        WIN32_EXECUTABLE OFF  # Keep console for CLI
    )
    target_link_libraries(orchestra-cli ws2_32)
elseif(UNIX)
    # Unix-specific settings
    target_link_libraries(orchestra-cli pthread dl)
endif()

# Platform-specific settings for GUI
if(WIN32)
    set_target_properties(AutonomousIDE PROPERTIES WIN32_EXECUTABLE ON)
    target_compile_definitions(AutonomousIDE PRIVATE
        _WIN32_WINNT=0x0A00
        UNICODE
        _UNICODE
    )
elseif(APPLE)
    set_target_properties(AutonomousIDE PROPERTIES
        MACOSX_BUNDLE ON
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist
    )
elseif(UNIX)
    target_link_libraries(AutonomousIDE pthread dl)
endif()

# Installation rules
install(TARGETS AutonomousIDE orchestra-cli
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(FILES ${CORE_HEADERS}
    DESTINATION include/autonomous_ide
)

# CPack configuration for installers
set(CPACK_PACKAGE_NAME "AutonomousIDE")
set(CPACK_PACKAGE_VENDOR "RawrXD")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Enterprise Autonomous IDE with AI capabilities")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_INSTALL_DIRECTORY "AutonomousIDE")

if(WIN32)
    set(CPACK_GENERATOR "NSIS;ZIP")
    set(CPACK_NSIS_DISPLAY_NAME "Autonomous IDE")
    set(CPACK_NSIS_PACKAGE_NAME "AutonomousIDE")
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
elseif(APPLE)
    set(CPACK_GENERATOR "DragNDrop;TGZ")
elseif(UNIX)
    set(CPACK_GENERATOR "DEB;RPM;TGZ")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "RawrXD <contact@rawrxd.com>")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6, libqt6core6, libqt6widgets6, libssl3")
endif()

include(CPack)

# Testing configuration
enable_testing()

# Existing test directories
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src")
    add_subdirectory(src)
endif()
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test")
    add_subdirectory(test)
endif()
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_qt")
    add_subdirectory(test_qt)
endif()

# Static analysis integration
find_program(CPPCHECK cppcheck)
if(CPPCHECK)
    add_custom_target(cppcheck
        COMMAND ${CPPCHECK}
            --enable=all
            --std=c++20
            --suppress=missingIncludeSystem
            --quiet
            ${CMAKE_CURRENT_SOURCE_DIR}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running cppcheck static analysis"
    )
endif()

find_program(CLANG_TIDY clang-tidy)
if(CLANG_TIDY)
    add_custom_target(clang-tidy
        COMMAND ${CLANG_TIDY}
            -p ${CMAKE_BINARY_DIR}
            ${CORE_SOURCES} ${UI_SOURCES}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running clang-tidy static analysis"
    )
endif()

# Export compile commands for IDEs
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Summary
message(STATUS "===========================================")
message(STATUS "Autonomous IDE Configuration Summary")
message(STATUS "===========================================")
message(STATUS "Version: ${PROJECT_VERSION}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Standard: C++${CMAKE_CXX_STANDARD}")
message(STATUS "Install prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "===========================================")