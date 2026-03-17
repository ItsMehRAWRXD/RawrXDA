# ===============================================================================
# Language Framework - CMake Build Configuration
# ===============================================================================

cmake_minimum_required(VERSION 3.16)
project(language_framework)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Find required packages
find_package(Qt6 COMPONENTS
    Core
    Gui
    Widgets
    Process
    REQUIRED
)

# Source files
set(LANGUAGE_FRAMEWORK_SOURCES
    language_framework.hpp
    language_framework.cpp
)

set(LANGUAGE_WIDGET_SOURCES
    language_widget.hpp
    language_widget.cpp
)

set(LANGUAGE_INTEGRATION_SOURCES
    language_integration.hpp
    language_integration.cpp
)

set(ALL_SOURCES
    ${LANGUAGE_FRAMEWORK_SOURCES}
    ${LANGUAGE_WIDGET_SOURCES}
    ${LANGUAGE_INTEGRATION_SOURCES}
)

# Create library
add_library(language_framework STATIC ${ALL_SOURCES})

# Link against Qt6 libraries
target_link_libraries(language_framework PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Process
)

# Include directories
target_include_directories(language_framework PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Set library properties
set_target_properties(language_framework PROPERTIES
    VERSION 1.0.0
    SOVERSION 1
    PUBLIC_HEADER "language_framework.hpp;language_widget.hpp;language_integration.hpp"
)

# Installation rules
install(TARGETS language_framework
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    PUBLIC_HEADER DESTINATION include/rawrxd/languages
)

# ===============================================================================
# Optional: Build tests
# ===============================================================================

option(BUILD_LANGUAGE_TESTS "Build language framework tests" OFF)

if(BUILD_LANGUAGE_TESTS)
    enable_testing()
    
    # Find testing framework
    find_package(Qt6 COMPONENTS Test REQUIRED)
    
    # Test files (to be created)
    # add_executable(language_tests
    #     tests/test_language_detection.cpp
    #     tests/test_compilation_pipeline.cpp
    # )
    # 
    # target_link_libraries(language_tests PRIVATE
    #     language_framework
    #     Qt6::Test
    # )
    # 
    # add_test(NAME LanguageTests COMMAND language_tests)
endif()

# ===============================================================================
# Optional: Build documentation
# ===============================================================================

option(BUILD_LANGUAGE_DOCS "Build language framework documentation" OFF)

if(BUILD_LANGUAGE_DOCS)
    find_package(Doxygen)
    if(DOXYGEN_FOUND)
        set(DOXYGEN_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/docs)
        doxygen_add_docs(language_docs
            ${CMAKE_CURRENT_SOURCE_DIR}
            COMMENT "Generating language framework documentation"
        )
    endif()
endif()

# ===============================================================================
# Print configuration
# ===============================================================================

message(STATUS "Language Framework Configuration:")
message(STATUS "  Qt Version: ${Qt6_VERSION}")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Tests Enabled: ${BUILD_LANGUAGE_TESTS}")
message(STATUS "  Docs Enabled: ${BUILD_LANGUAGE_DOCS}")
