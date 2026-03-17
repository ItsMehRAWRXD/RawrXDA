# RawrXD Enterprise License Creator Tool
cmake_minimum_required(VERSION 3.20)

project(RawrXD_LicenseCreator LANGUAGES CXX)

# Don't require specific Windows SDK version
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(CMAKE_SYSTEM_VERSION 10.0)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Source files
set(LICENSE_CREATOR_SOURCES
    main.cpp
    license_generator.cpp
    license_generator.hpp
)

# Create executable
add_executable(RawrXD_LicenseCreator ${LICENSE_CREATOR_SOURCES})

# Platform-specific linking
if(WIN32)
    target_link_libraries(RawrXD_LicenseCreator PRIVATE
        advapi32    # CryptoAPI
    )
else()
    # Linux: OpenSSL
    find_package(OpenSSL REQUIRED)
    target_link_libraries(RawrXD_LicenseCreator PRIVATE
        OpenSSL::Crypto
    )
endif()

# Output to tools directory
set_target_properties(RawrXD_LicenseCreator PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tools"
    OUTPUT_NAME "RawrXD_LicenseCreator"
)

# Install target
install(TARGETS RawrXD_LicenseCreator
    RUNTIME DESTINATION bin
)

message(STATUS "RawrXD License Creator tool configured")
