# Git diff/merge orchestration module
add_library(RawrXDGit STATIC
    semantic_diff_analyzer.hpp
    semantic_diff_analyzer.cpp
    ai_merge_resolver.hpp
    ai_merge_resolver.cpp
)

target_link_libraries(RawrXDGit
    PUBLIC
        Qt6::Core
        Qt6::Sql
    PRIVATE
        RawrXDOrchestration
)

target_include_directories(RawrXDGit
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..
)

# C++ standard
set_target_properties(RawrXDGit PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

# Compiler warnings
if(MSVC)
    target_compile_options(RawrXDGit PRIVATE /W4)
else()
    target_compile_options(RawrXDGit PRIVATE -Wall -Wextra -Wpedantic)
endif()
