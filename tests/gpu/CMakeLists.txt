cmake_minimum_required(VERSION 3.20)
project(tile_gemm_test CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Use the same Vulkan SDK the main RawrXD build uses.
find_package(Vulkan REQUIRED)

add_executable(tile_gemm_test test_tile_gemm.cpp)
target_link_libraries(tile_gemm_test PRIVATE Vulkan::Vulkan)

add_executable(q4k_q8_1_test test_q4k_q8_1.cpp)
target_link_libraries(q4k_q8_1_test PRIVATE Vulkan::Vulkan)

add_executable(q4k_q8_1_bench bench_q4k_q8_1.cpp)
target_link_libraries(q4k_q8_1_bench PRIVATE Vulkan::Vulkan)

add_executable(q4k_q8_1_lds_test test_q4k_q8_1.cpp)
target_link_libraries(q4k_q8_1_lds_test PRIVATE Vulkan::Vulkan)

# Compile quantized u32 shader templates to SPIR-V for harness runs.
set(RAWRXD_SHADER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../src/gpu/shaders")
set(RAWRXD_SHADER_TEMPLATE_DIR "${RAWRXD_SHADER_DIR}/templates")
set(RAWRXD_SHADER_SPV_DIR "${RAWRXD_SHADER_DIR}/_spv")

if (Vulkan_GLSLANG_VALIDATOR_EXECUTABLE)
    set(RAWRXD_Q_U32_SPV_FILES
        "${RAWRXD_SHADER_SPV_DIR}/fused_q4_0_u32.spv"
        "${RAWRXD_SHADER_SPV_DIR}/fused_q5_k_u32.spv"
        "${RAWRXD_SHADER_SPV_DIR}/fused_q6_k_u32.spv"
    )

    add_custom_command(
        OUTPUT "${RAWRXD_SHADER_SPV_DIR}/fused_q4_0_u32.spv"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${RAWRXD_SHADER_SPV_DIR}"
        COMMAND "${Vulkan_GLSLANG_VALIDATOR_EXECUTABLE}" -V --target-env vulkan1.2
                "${RAWRXD_SHADER_TEMPLATE_DIR}/fused_q4_0_u32_template.comp"
                -o "${RAWRXD_SHADER_SPV_DIR}/fused_q4_0_u32.spv"
        DEPENDS "${RAWRXD_SHADER_TEMPLATE_DIR}/fused_q4_0_u32_template.comp"
        VERBATIM
    )

    add_custom_command(
        OUTPUT "${RAWRXD_SHADER_SPV_DIR}/fused_q5_k_u32.spv"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${RAWRXD_SHADER_SPV_DIR}"
        COMMAND "${Vulkan_GLSLANG_VALIDATOR_EXECUTABLE}" -V --target-env vulkan1.2
                "${RAWRXD_SHADER_TEMPLATE_DIR}/fused_q5_k_u32_template.comp"
                -o "${RAWRXD_SHADER_SPV_DIR}/fused_q5_k_u32.spv"
        DEPENDS "${RAWRXD_SHADER_TEMPLATE_DIR}/fused_q5_k_u32_template.comp"
        VERBATIM
    )

    add_custom_command(
        OUTPUT "${RAWRXD_SHADER_SPV_DIR}/fused_q6_k_u32.spv"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${RAWRXD_SHADER_SPV_DIR}"
        COMMAND "${Vulkan_GLSLANG_VALIDATOR_EXECUTABLE}" -V --target-env vulkan1.2
                "${RAWRXD_SHADER_TEMPLATE_DIR}/fused_q6_k_u32_template.comp"
                -o "${RAWRXD_SHADER_SPV_DIR}/fused_q6_k_u32.spv"
        DEPENDS "${RAWRXD_SHADER_TEMPLATE_DIR}/fused_q6_k_u32_template.comp"
        VERBATIM
    )

    add_custom_target(gpu_quant_spv DEPENDS ${RAWRXD_Q_U32_SPV_FILES})
endif()

# Make the SPIR-V path discoverable at runtime via argv[1] if needed.
if(MSVC)
    target_compile_options(tile_gemm_test PRIVATE /W3 /EHsc)
    target_compile_options(q4k_q8_1_test PRIVATE /W3 /EHsc)
    target_compile_options(q4k_q8_1_bench PRIVATE /W3 /EHsc)
    target_compile_options(q4k_q8_1_lds_test PRIVATE /W3 /EHsc)
endif()
