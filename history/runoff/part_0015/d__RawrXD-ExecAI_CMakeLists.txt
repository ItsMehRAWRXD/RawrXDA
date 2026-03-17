cmake_minimum_required(VERSION 3.25)
project(RawrXD-ExecAI LANGUAGES CXX ASM_MASM C)

# ================================================================
# RawrXD-ExecAI Build Configuration
# Complete production build with MASM64 + C + C++ integration
# ================================================================

# === CORRECTED BUILD FLAGS ===
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)

# === MASM INTEGRATION ===
enable_language(ASM_MASM)
set(CMAKE_ASM_MASM_FLAGS "/c /WX /W3 /nologo /Zi")
set_property(SOURCE execai_kernel_complete.asm PROPERTY LANGUAGE ASM_MASM)
set_property(SOURCE RawrXD-GGUFAnalyzer-Complete.asm PROPERTY LANGUAGE ASM_MASM)

# === FEATURE FLAGS ===
option(ENABLE_MASM_INTEGRATION "Enable MASM compression kernel" ON)
option(ENABLE_800B_SUPPORT "Enable 800B-class architecture" ON)

# === CORRECTED INCLUDES ===
find_package(Qt6 REQUIRED COMPONENTS Core Widgets)
find_package(Threads REQUIRED)

# === EXECUTABLE TARGETS ===
add_executable(execai 
    execai_kernel_complete.asm
    execai_runtime_complete.c
    execai_distiller.cpp
    ui_main_window.cpp
    ui_model_selector.cpp
)

target_compile_definitions(execai PRIVATE
    ENABLE_MASM_INTEGRATION=$<BOOL:${ENABLE_MASM_INTEGRATION}>
    ENABLE_800B_SUPPORT=$<BOOL:${ENABLE_800B_SUPPORT}>
    UNICODE
    _UNICODE
)

target_compile_options(execai PRIVATE
    $<$<COMPILE_LANGUAGE:CXX>:/W4 /WX /arch:AVX2>
    $<$<COMPILE_LANGUAGE:C>:/W4 /WX /arch:AVX2>
)

target_link_libraries(execai PRIVATE
    kernel32
    ntdll
    Qt6::Core
    Qt6::Widgets
    Threads::Threads
)

# === BENCHMARK TARGET ===
add_executable(model_loader_bench 
    model_loader_bench.cpp
    execai_runtime_complete.c
    execai_kernel_complete.asm
)

target_compile_definitions(model_loader_bench PRIVATE
    ENABLE_MASM_INTEGRATION=$<BOOL:${ENABLE_MASM_INTEGRATION}>
)

target_link_libraries(model_loader_bench PRIVATE
    kernel32
    ntdll
    Threads::Threads
)

# === PURE MASM64 ANALYZER TARGET ===
add_executable(gguf_analyzer_masm64
    RawrXD-GGUFAnalyzer-Complete.asm
)

# === TEST TARGET ===
add_executable(test_streaming_inference 
    test_streaming_inference.cpp
    execai_runtime_complete.c
    execai_kernel_complete.asm
)

target_compile_definitions(test_streaming_inference PRIVATE
    ENABLE_MASM_INTEGRATION=$<BOOL:${ENABLE_MASM_INTEGRATION}>
)

target_link_libraries(test_streaming_inference PRIVATE
    kernel32
    ntdll
    Qt6::Core
    Threads::Threads
)

# === ANALYTICS DASHBOARD (Fixed) ===
add_library(phase5_analytics SHARED
    phase5_analytics_dashboard.cpp
)

target_include_directories(phase5_analytics PRIVATE
    ${Qt6Core_INCLUDE_DIRS}
    ${Qt6Widgets_INCLUDE_DIRS}
)

target_link_libraries(phase5_analytics PRIVATE
    Qt6::Core
    Qt6::Widgets
)

# === FIXED MODEL LOADER ===
add_library(model_loader STATIC
    auto_model_loader.cpp
    model_invoker.cpp
    circuit_breaker.cpp
)

target_compile_options(model_loader PRIVATE
    /W4 /WX
)

target_link_libraries(model_loader PRIVATE
    kernel32
    ntdll
)

# === MASM64 ANALYZER TEST HARNESS ===
add_executable(test_gguf_analyzer_masm64
    test_gguf_analyzer_masm64.asm
)

set_property(SOURCE test_gguf_analyzer_masm64.asm PROPERTY LANGUAGE ASM_MASM)

target_link_libraries(test_gguf_analyzer_masm64 PRIVATE
    kernel32
)

# === INSTALLATION ===
install(TARGETS execai model_loader_bench test_streaming_inference gguf_analyzer_masm64 test_gguf_analyzer_masm64
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

# === BUILD VERIFICATION ===
add_custom_target(verify ALL
    COMMAND ${CMAKE_COMMAND} -E echo "Build verification complete"
    DEPENDS execai model_loader_bench test_streaming_inference gguf_analyzer_masm64 test_gguf_analyzer_masm64
)
