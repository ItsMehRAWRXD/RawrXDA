# =============================================================================
# WebView2Container CMake Configuration
# =============================================================================

# Minimum CMake version
cmake_minimum_required(VERSION 3.16)

# Find WebView2
find_package(Microsoft.Web.WebView2 REQUIRED)

# WebView2Container library
set(WEBVIEW2_SOURCES
    WebView2Container_stubs.cpp
    WebView2Container.h
)

# Create the library
add_library(WebView2Container STATIC ${WEBVIEW2_SOURCES})

# Set C++17 standard (required for WebView2)
target_compile_features(WebView2Container PUBLIC cxx_std_17)

# Include directories
target_include_directories(WebView2Container PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link WebView2 and system libraries
target_link_libraries(WebView2Container PUBLIC
    Microsoft.Web.WebView2::WebView2
    ole32
    oleaut32
    user32
    version
)

# Compiler definitions
target_compile_definitions(WebView2Container PUBLIC
    WIN32_LEAN_AND_MEAN
    UNICODE
    _UNICODE
)

# For Visual Studio, set additional properties
if(MSVC)
    target_compile_options(WebView2Container PRIVATE /W4)
    set_target_properties(WebView2Container PROPERTIES
        VS_GLOBAL_WindowsTargetPlatformVersion "10.0"
    )
endif()

# =============================================================================
# Example usage target (optional)
# =============================================================================
if(WEBVIEW2_BUILD_EXAMPLE)
    add_executable(WebView2Example example_usage.cpp)
    target_link_libraries(WebView2Example PRIVATE WebView2Container)
endif()

# =============================================================================
# Installation (optional)
# =============================================================================
if(WEBVIEW2_INSTALL)
    install(TARGETS WebView2Container
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        RUNTIME DESTINATION bin
    )
    
    install(FILES WebView2Container.h
        DESTINATION include
    )
endif()

# =============================================================================
# Package configuration
# =============================================================================
include(CMakePackageConfigHelpers)

# Create version file
write_basic_package_version_file(
    WebView2ContainerConfigVersion.cmake
    VERSION 1.0.0
    COMPATIBILITY AnyNewerVersion
)

# Create config file
configure_package_config_file(
    WebView2ContainerConfig.cmake.in
    WebView2ContainerConfig.cmake
    INSTALL_DESTINATION lib/cmake/WebView2Container
)

# Export targets
export(TARGETS WebView2Container
    FILE WebView2ContainerTargets.cmake
    NAMESPACE WebView2Container::
)