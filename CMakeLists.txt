cmake_minimum_required(VERSION 3.20)
project(RawrXD LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)

add_definitions(-DNOMINMAX -DWIN32_LEAN_AND_MEAN -D_CRT_SECURE_NO_WARNINGS)

# Vulkan SDK Setup
if(EXISTS "C:/VulkanSDK/1.4.328.1")
    include_directories("C:/VulkanSDK/1.4.328.1/Include")
    link_directories("C:/VulkanSDK/1.4.328.1/Lib")
endif()

# Common sources
set(COMMON_SOURCES
    src/cpu_inference_engine.cpp
    src/rawrxd_inference.cpp
    src/rawrxd_tokenizer.cpp 
    src/rawrxd_transformer.cpp
    src/rawrxd_sampler.cpp
    src/rawrxd_model_loader.cpp
    src/rawrxd_kernels.cpp
    src/token_generator.cpp
    src/vulkan_compute.cpp
)

set(MAIN_SOURCES
    src/main.cpp
    src/ide_orchestrator.cpp
    src/agentic_ide.cpp
    src/agentic_executor.cpp
    src/agentic_memory_system.cpp
    src/agentic_observability.cpp
    src/universal_model_router.cpp
    src/agentic_engine.cpp
    src/action_executor.cpp
    src/cloud_api_client.cpp
    src/RawrXD_PipeClient.cpp
    src/RawrXD_Renderer_D2D.cpp
    src/utils/Diagnostics.cpp
    src/utils/ErrorReporter.cpp
    src/utils/InferenceSettingsManager.cpp
    ${COMMON_SOURCES}
)

# Main Executable
add_executable(RawrXD ${MAIN_SOURCES})
target_include_directories(RawrXD PRIVATE src include)

# Self Audit Executable
add_executable(RawrXD_SelfAudit 
    src/test_self_audit.cpp 
    src/ide_auditor.cpp
    src/swarm_orchestrator.cpp 
    src/chain_of_thought.cpp 
    src/net/net_impl_win32.cpp
    ${COMMON_SOURCES}
)
target_include_directories(RawrXD_SelfAudit PRIVATE src include)

# Linking
if(MINGW)
    target_link_libraries(RawrXD PRIVATE Threads::Threads ws2_32 d2d1 dwrite windowscodecs uuid ole32 shlwapi version vulkan-1 z)
    target_link_libraries(RawrXD_SelfAudit PRIVATE Threads::Threads ws2_32 uuid ole32 shlwapi version vulkan-1 z)
    
    target_compile_options(RawrXD PRIVATE -O2 -mavx512f -Wa,-mbig-obj)
    target_compile_options(RawrXD_SelfAudit PRIVATE -O2 -mavx512f -Wa,-mbig-obj)
else()
    target_link_libraries(RawrXD PRIVATE Threads::Threads ws2_32 d2d1 dwrite windowscodecs)
    target_link_libraries(RawrXD_SelfAudit PRIVATE Threads::Threads ws2_32)
    
    target_compile_options(RawrXD PRIVATE /arch:AVX512 /O2 /std:c++20 /EHsc /bigobj)
    target_link_options(RawrXD PRIVATE /LARGEADDRESSAWARE)
endif()

set_target_properties(RawrXD_SelfAudit PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
