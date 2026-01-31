# CMakeLists.txt for Setup Wizard Component
# RawrXD IDE - Hardware Detection and Configuration Wizard

cmake_minimum_required(VERSION 3.16)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

set(SETUP_WIZARD_SOURCES
    SetupWizard.cpp
)

set(SETUP_WIZARD_HEADERS
    SetupWizard.hpp
)

add_library(rawrxd_setup STATIC
    ${SETUP_WIZARD_SOURCES}
    ${SETUP_WIZARD_HEADERS}
)

set_target_properties(rawrxd_setup PROPERTIES
    AUTOMOC ON
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

target_link_libraries(rawrxd_setup PUBLIC
    Qt6::Core
    Qt6::Widgets
)

target_include_directories(rawrxd_setup PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
)

target_compile_definitions(rawrxd_setup PRIVATE
    RAWRXD_SETUP_WIZARD_ENABLED
)

message(STATUS "✓ Setup Wizard component configured")
