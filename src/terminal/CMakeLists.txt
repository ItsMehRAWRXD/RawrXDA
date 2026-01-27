# Terminal sandboxing and zero-retention module
add_library(RawrXDTerminal STATIC
    sandboxed_terminal.hpp
    sandboxed_terminal.cpp
    zero_retention_manager.hpp
    zero_retention_manager.cpp
)

target_link_libraries(RawrXDTerminal
    PUBLIC
        Qt6::Core
        Qt6::Network
    PRIVATE
        RawrXDOrchestration
)

target_include_directories(RawrXDTerminal
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..
)

# C++ standard
set_target_properties(RawrXDTerminal PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

# Compiler warnings
if(MSVC)
    target_compile_options(RawrXDTerminal PRIVATE /W4)
else()
    target_compile_options(RawrXDTerminal PRIVATE -Wall -Wextra -Wpedantic)
endif()
