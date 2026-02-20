# Git diff/merge orchestration module
add_library(RawrXDGit STATIC
    semantic_diff_analyzer.hpp
    semantic_diff_analyzer.cpp
    ai_merge_resolver.hpp
    ai_merge_resolver.cpp
)

# Qt-free: uses nlohmann::json + STL + WinHTTP
target_link_libraries(RawrXDGit
    PUBLIC
        $<$<PLATFORM_ID:Windows>:winhttp>
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
