# ExecAI Integration for RawrXD Lazy Init IDE
# Provides GGUF analysis and distilled model execution

cmake_minimum_required(VERSION 3.20)

# Enable MASM64 assembly language support
enable_language(ASM_MASM)

# Find Qt6 components
find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

# ExecAI Runtime Library
add_library(execai_runtime STATIC
    execai_runtime_complete.c
    execai_runtime_complete.h
)

target_include_directories(execai_runtime PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# ExecAI Kernel (MASM64)
add_library(execai_kernel STATIC
    execai_kernel_complete.asm
)

set_property(SOURCE execai_kernel_complete.asm PROPERTY LANGUAGE ASM_MASM)

target_link_libraries(execai_kernel PRIVATE kernel32)

# ExecAI Security Library
add_library(execai_security STATIC
    execai_security.cpp
    execai_security.h
)

target_include_directories(execai_security PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(execai_security PUBLIC Qt6::Core)

# ExecAI Widget (Qt UI)
add_library(execai_widget STATIC
    execai_widget.cpp
    execai_widget.h
)

target_include_directories(execai_widget PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(execai_widget PUBLIC
    Qt6::Core
    Qt6::Widgets
    execai_security
)

# GGUF Analyzer (Pure MASM64)
add_executable(gguf_analyzer_masm64
    ${CMAKE_SOURCE_DIR}/src/gguf_analyzer/RawrXD-GGUFAnalyzer-Complete.asm
)

set_property(SOURCE ${CMAKE_SOURCE_DIR}/src/gguf_analyzer/RawrXD-GGUFAnalyzer-Complete.asm PROPERTY LANGUAGE ASM_MASM)

# Pure MASM64 executable - no CRT, no WIN32 subsystem
set_target_properties(gguf_analyzer_masm64 PROPERTIES
    WIN32_EXECUTABLE FALSE
    LINK_FLAGS "/ENTRY:main /SUBSYSTEM:CONSOLE /NODEFAULTLIB /STACK:0x100000,0x100000"
)

target_link_libraries(gguf_analyzer_masm64 PRIVATE kernel32 shell32)

# GGUF Analyzer Test Suite (commented out due to syntax issues)
# add_executable(test_gguf_analyzer_masm64
#     test_gguf_analyzer_masm64.asm
# )
#
# set_property(SOURCE test_gguf_analyzer_masm64.asm PROPERTY LANGUAGE ASM_MASM)
#
# # Pure MASM64 test executable - no CRT
# set_target_properties(test_gguf_analyzer_masm64 PROPERTIES
#     WIN32_EXECUTABLE FALSE
#     LINK_FLAGS "/ENTRY:main /SUBSYSTEM:CONSOLE /NODEFAULTLIB /STACK:0x100000,0x100000"
# )
#
# target_link_libraries(test_gguf_analyzer_masm64 PRIVATE kernel32)

# ExecAI Distiller (C++ component)
add_executable(execai_distiller
    execai_distiller.cpp
)

target_link_libraries(execai_distiller PRIVATE
    execai_runtime
    execai_kernel
)

# Install targets
install(TARGETS
    gguf_analyzer_masm64
    execai_distiller
    execai_runtime
    execai_kernel
    execai_security
    execai_widget
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)