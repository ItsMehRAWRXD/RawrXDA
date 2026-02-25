# Custom CMake toolchain for Visual Studio 2022 that bypasses SDK detection issues
# Usage: cmake .. -DCMAKE_TOOLCHAIN_FILE=CustomVSToolchain.cmake

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_VERSION 10.0)

# Use the available Windows SDK
if(EXISTS "C:/Program Files (x86)/Windows Kits/10/Lib/10.0.26100.0")
    set(WINDOWS_SDK_VERSION "10.0.26100.0")
    set(WINDOWS_SDK_ROOT "C:/Program Files (x86)/Windows Kits/10")
elseif(EXISTS "C:/Program Files (x86)/Windows Kits/10/Lib/10.0.22621.0")
    set(WINDOWS_SDK_VERSION "10.0.22621.0")
    set(WINDOWS_SDK_ROOT "C:/Program Files (x86)/Windows Kits/10")
else()
    message(WARNING "Windows SDK not found in standard location")
    set(WINDOWS_SDK_VERSION "10.0.26100.0")
    set(WINDOWS_SDK_ROOT "C:/Program Files (x86)/Windows Kits/10")
endif()

message(STATUS "Using Windows SDK: ${WINDOWS_SDK_VERSION}")
message(STATUS "Windows SDK Root: ${WINDOWS_SDK_ROOT}")

# Set VS toolset
set(CMAKE_GENERATOR_TOOLSET "v143,host=x64" CACHE STRING "")
set(CMAKE_VS_PLATFORM_TOOLSET "v143")

# SDK Lib paths
set(WINDOWS_SDK_LIB_PATH "${WINDOWS_SDK_ROOT}/Lib/${WINDOWS_SDK_VERSION}")
set(WINDOWS_SDK_INCLUDE_PATH "${WINDOWS_SDK_ROOT}/Include/${WINDOWS_SDK_VERSION}")

# Add library directories
link_directories(
    "${WINDOWS_SDK_LIB_PATH}/um/x64"
    "${WINDOWS_SDK_LIB_PATH}/ucrt/x64"
)

# Add include directories
include_directories(
    "${WINDOWS_SDK_INCLUDE_PATH}/um"
    "${WINDOWS_SDK_INCLUDE_PATH}/shared"
    "${WINDOWS_SDK_INCLUDE_PATH}/ucrt"
)

# Compiler settings
set(CMAKE_CXX_FLAGS_INIT "/MP /W4 /EHsc")
set(CMAKE_CXX_FLAGS_RELEASE_INIT "/O2 /Ob2 /DNDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG_INIT "/Zi /Ob0 /Od /RTC1")

# Link settings
set(CMAKE_EXE_LINKER_FLAGS_INIT "/SUBSYSTEM:CONSOLE /MACHINE:X64")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "/SUBSYSTEM:WINDOWS /MACHINE:X64")

message(STATUS "Custom VS 2022 toolchain loaded")
