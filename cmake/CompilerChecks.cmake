# =============================================================================
# cmake/CompilerChecks.cmake - Validate Compiler Capabilities
# =============================================================================

# Check for C++20 support
include(CheckCXXCompilerFlag)
check_cxx_compiler_flag("/std:c++20" COMPILER_SUPPORTS_CXX20)

if(NOT COMPILER_SUPPORTS_CXX20)
    message(FATAL_ERROR "C++20 support is required. Please use MSVC 19.28+ (VS2022)")
endif()

# Check for required MSVC features
if(MSVC)
    # Check for /permissive- support
    try_compile(COMPILER_SUPPORTS_PERMISSIVE
        "${CMAKE_CURRENT_BINARY_DIR}/compile_tests"
        SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/cmake/test_permissive.cpp"
        CMAKE_FLAGS "-DCMAKE_CXX_FLAGS=/permissive-"
        CXX_STANDARD 20
        OUTPUT_VARIABLE PERMISSIVE_OUTPUT
    )
    
    if(NOT COMPILER_SUPPORTS_PERMISSIVE)
        message(WARNING "Compiler does not support /permissive-. Some ISO C++ features may not work correctly.")
    endif()
endif()

# Create a minimal test file for concepts if it doesn't exist
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/cmake/test_permissive.cpp")
    file(WRITE "${CMAKE_CURRENT_SOURCE_DIR}/cmake/test_permissive.cpp"
        "#include <type_traits>\n"
        "template<typename T>\n"
        "concept Arithmetic = std::is_arithmetic_v<T>;\n"
        "template<Arithmetic T>\n"
        "T add(T a, T b) { return a + b; }\n"
        "int main() { return add(1, 2); }\n"
    )
endif()
