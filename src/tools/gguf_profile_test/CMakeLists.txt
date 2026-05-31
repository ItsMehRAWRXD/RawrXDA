# GGUF Profile Writer Test (console target)

add_executable(gguf_profile_test
    gguf_profile_test.cpp
    ../../gguf_factory/gguf_writer_minimal.cpp
)

target_include_directories(gguf_profile_test PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

set_target_properties(gguf_profile_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tools
)

if(MSVC)
    target_compile_options(gguf_profile_test PRIVATE /W4 /utf-8)
else()
    target_compile_options(gguf_profile_test PRIVATE -Wall -Wextra)
endif()

