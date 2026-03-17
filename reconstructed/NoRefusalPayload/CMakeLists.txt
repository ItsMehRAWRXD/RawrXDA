cmake_minimum_required(VERSION 3.15)
project(NoRefusalPayload LANGUAGES CXX ASM_MASM)

# Enable MASM support for x64
if(MSVC)
    set(CMAKE_ASM_MASM_FLAGS "${CMAKE_ASM_MASM_FLAGS} /Zi")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4 /EHsc")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable ASM_MASM language
enable_language(ASM_MASM)

# Main NoRefusal Engine
add_executable(NoRefusalEngine
    src/main.cpp
    src/PayloadSupervisor.cpp
    include/PayloadSupervisor.hpp
    asm/norefusal_core.asm
)

# Set entry point to C++ main, which then invokes the ASM payload
set_target_properties(NoRefusalEngine PROPERTIES
    LINK_FLAGS "/SUBSYSTEM:CONSOLE /ENTRY:mainCRTStartup"
)

# Polymorphic Encoder Engine (library)
add_library(PolymorphicEncoder STATIC
    asm/RawrXD_PolymorphicEncoder.asm
)

# Encoder Integration Demo
add_executable(PolymorphicEncoderDemo
    src/PolymorphicEncoderIntegration.cpp
    src/PayloadSupervisor.cpp
    include/PayloadSupervisor.hpp
)

target_link_libraries(PolymorphicEncoderDemo PRIVATE
    PolymorphicEncoder
)

set_target_properties(PolymorphicEncoderDemo PROPERTIES
    LINK_FLAGS "/SUBSYSTEM:CONSOLE /ENTRY:mainCRTStartup"
)

# Include directories
target_include_directories(NoRefusalEngine PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Link against required Windows libraries
target_link_libraries(NoRefusalEngine PRIVATE
    kernel32
    user32
    ntdll
)

# Set output directory
set_target_properties(NoRefusalEngine PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
