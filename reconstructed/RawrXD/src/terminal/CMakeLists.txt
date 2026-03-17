# Terminal sandboxing and zero-retention module (Qt-free, pure C++20/Win32)
add_library(RawrXDTerminal STATIC
    sandboxed_terminal.hpp
    sandboxed_terminal.cpp
    zero_retention_manager.hpp
    zero_retention_manager.cpp
)

target_link_libraries(RawrXDTerminal
    PUBLIC
        Kernel32
        Advapi32
    PRIVATE
        RawrXDOrchestration
)

target_include_directories(RawrXDTerminal
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..
)

# C++ standard
set_target_properties(RawrXDTerminal PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

target_compile_definitions(RawrXDTerminal PRIVATE
    WIN64 _WIN64 UNICODE _UNICODE _CRT_SECURE_NO_WARNINGS
)

# Compiler warnings
if(MSVC)
    target_compile_options(RawrXDTerminal PRIVATE /W4 /permissive-)
else()
    target_compile_options(RawrXDTerminal PRIVATE -Wall -Wextra -Wpedantic)
endif()
