# Reverse-CMake helper target (uses PowerShell + CMake File API)
add_custom_target(reverse-cmake
    COMMAND pwsh -ExecutionPolicy Bypass -File "${CMAKE_SOURCE_DIR}/tools/reverse-cmake.ps1" -SourceDir "${CMAKE_SOURCE_DIR}" -BuildDir "${CMAKE_BINARY_DIR}" -OutDir "${CMAKE_SOURCE_DIR}/bin/reverse-cmake"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Generating reverse CMake summary via File API"
)
add_custom_target(hexmag-chain
    COMMAND pwsh -ExecutionPolicy Bypass -File "${CMAKE_SOURCE_DIR}/tools/hexmag-orchestrator.ps1" -HexmagPath "${CMAKE_SOURCE_DIR}/hexmag.sqlite" -SourceDir "${CMAKE_SOURCE_DIR}" -OutDir "${CMAKE_SOURCE_DIR}/bin/hexmag" -GraphDot "${CMAKE_SOURCE_DIR}/bin/reverse-cmake/fallback_graph.dot"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Running Hexmag orchestrator to chain MASM stub generation and task setup"
)

add_custom_target(self-masmize
    COMMAND pwsh -ExecutionPolicy Bypass -File "${CMAKE_SOURCE_DIR}/tools/cpp-to-masm-selfcompile.ps1" -SourceDir "${CMAKE_SOURCE_DIR}" -OutDir "${CMAKE_SOURCE_DIR}/bin/self-masmize"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Reverse-engineering C++ units to MASM artifacts (listings + stubs)"
)

# Integrate generated MASM stubs (from hexmag) into QtShell build if present
set(HEXMAG_STUB_DIR "${CMAKE_SOURCE_DIR}/bin/hexmag")
set(HEXMAG_STUB_SCAFFOLDER "${CMAKE_SOURCE_DIR}/tools/stub_scaffolder.ps1")
set(HEXMAG_SCAFFOLD_OUTPUTS
    "${HEXMAG_STUB_DIR}/src_stubs_scaffold.cpp"
    "${HEXMAG_STUB_DIR}/qtapp_stubs_scaffold.cpp"
    "${HEXMAG_STUB_DIR}/include_stubs_scaffold.cpp")

add_custom_command(
    OUTPUT ${HEXMAG_SCAFFOLD_OUTPUTS}
    COMMAND pwsh -ExecutionPolicy Bypass -File "${HEXMAG_STUB_SCAFFOLDER}" -StubDir "${HEXMAG_STUB_DIR}" -OutDir "${HEXMAG_STUB_DIR}"
    DEPENDS hexmag-chain
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Generating C++ scaffolding for Hexmag stubs"
)
add_custom_target(hexmag-scaffold DEPENDS ${HEXMAG_SCAFFOLD_OUTPUTS})

file(GLOB HEXMAG_STUBS "${HEXMAG_STUB_DIR}/*_stubs.asm")
file(GLOB HEXMAG_SCAFFOLD ${HEXMAG_SCAFFOLD_OUTPUTS})
if(HEXMAG_SCAFFOLD)
    add_library(masm_hexmag_stubs_obj OBJECT ${HEXMAG_SCAFFOLD})
    add_dependencies(masm_hexmag_stubs_obj hexmag-scaffold)
elseif(HEXMAG_STUBS)
    add_library(masm_hexmag_stubs_obj OBJECT ${HEXMAG_STUBS})
    set_source_files_properties(${HEXMAG_STUBS} PROPERTIES LANGUAGE ASM_MASM)
    add_dependencies(masm_hexmag_stubs_obj hexmag-chain)
endif()

if(TARGET RawrXD-QtShell AND TARGET masm_hexmag_stubs_obj)
    target_link_libraries(RawrXD-QtShell PRIVATE $<TARGET_OBJECTS:masm_hexmag_stubs_obj>)
    message(STATUS "Linked hexmag-generated stubs (scaffold preferred) into RawrXD-QtShell")
endif()

# MASM/C++ hotpatch bridge (lets MASM shims drive UnifiedHotpatchManager)
if(TARGET RawrXD-QtShell)
    target_sources(RawrXD-QtShell PRIVATE
        ${CMAKE_SOURCE_DIR}/src/qtapp/masm_hotpatch_bridge.cpp
        ${CMAKE_SOURCE_DIR}/src/masm/hotpatch_bridge.asm)
    set_source_files_properties(${CMAKE_SOURCE_DIR}/src/masm/hotpatch_bridge.asm PROPERTIES LANGUAGE ASM_MASM)
endif()
cmake_minimum_required(VERSION 3.20)
project(RawrXD-ModelLoader VERSION 1.0.13)
add_definitions(-DNOMINMAX)

# Enable MASM x64 assembler for pure runtime layer
enable_language(ASM_MASM)

# Configure MASM compiler for ml64.exe
if(MSVC)
    # Suppress C++ flags from being injected into MASM compilation
    # Set both FLAGS and COMPILE_OBJECT to ensure clean MASM-only invocation
    set(CMAKE_ASM_MASM_FLAGS "/nologo /Zi /c /Cp /W3")
    
    # Force explicit flag list to prevent C++ flag contamination
    # This overrides Visual Studio's masm.targets which tries to inject /EHsc and /O2
    set(CMAKE_ASM_MASM_COMPILE_OBJECT 
        "<CMAKE_ASM_MASM_COMPILER> <DEFINES> <INCLUDES> <FLAGS> /Fo<OBJECT> <SOURCE>")
    
    # Explicitly clear any C++ flags that might leak into MASM
    set(CMAKE_ASM_MASM_FLAGS_RELEASE "/nologo /Zi /c /Cp /W3" CACHE STRING "MASM Release flags" FORCE)
    set(CMAKE_ASM_MASM_FLAGS_DEBUG "/nologo /Zi /c /Cp /W3" CACHE STRING "MASM Debug flags" FORCE)
    
    message(STATUS "MASM compiler configured: ${CMAKE_ASM_MASM_COMPILER}")
    message(STATUS "MASM compile flags: ${CMAKE_ASM_MASM_FLAGS}")
endif()

add_subdirectory(src/masm)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ============================================================
# 64-BIT ARCHITECTURE ENFORCEMENT
# ============================================================
# RawrXD Agentic IDE is x64-ONLY. All dependencies (Qt, GGML, Vulkan)
# are compiled as 64-bit. No 32-bit (x86) support or fallback.
# See: 64BIT_DEPENDENCY_AUDIT.md and DLL_DISTRIBUTION_PACKAGE.md

# Force x64 architecture to prevent any 32-bit DLL issues
if(MSVC)
    set(CMAKE_GENERATOR_PLATFORM x64)
    set(CMAKE_SIZEOF_VOID_P 8)
    # Validate 64-bit configuration
    if(NOT CMAKE_GENERATOR_PLATFORM STREQUAL "x64")
        message(FATAL_ERROR "CRITICAL: CMAKE_GENERATOR_PLATFORM must be x64, got ${CMAKE_GENERATOR_PLATFORM}")
    endif()
    if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
        message(FATAL_ERROR "CRITICAL: CMAKE_SIZEOF_VOID_P must be 8 (x64), got ${CMAKE_SIZEOF_VOID_P}")
    endif()
    message(STATUS "✓ 64-bit configuration validated: pointer size = ${CMAKE_SIZEOF_VOID_P}, platform = ${CMAKE_GENERATOR_PLATFORM}")
endif()

# Pin MSVC runtime for deterministic builds - ALWAYS use /MD (dynamic CRT)
# This ensures all DLLs link against the same MSVC runtime DLL
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    # Force /MD for C/C++ targets only (not MASM) to prevent CRT mismatch
    add_compile_options(
        $<$<COMPILE_LANGUAGE:C,CXX>:$<$<CONFIG:Debug>:/MDd>>
        $<$<COMPILE_LANGUAGE:C,CXX>:$<$<CONFIG:Release>:/MD>>
        $<$<COMPILE_LANGUAGE:C,CXX>:$<$<CONFIG:RelWithDebInfo>:/MD>>
        $<$<COMPILE_LANGUAGE:C,CXX>:$<$<CONFIG:MinSizeRel>:/MD>>
    )
endif()

# Detect compiler and set output directory suffix
if(MINGW)
    set(COMPILER_SUFFIX "mingw")
elseif(MSVC)
    set(COMPILER_SUFFIX "msvc")
else()
    set(COMPILER_SUFFIX "other")
endif()

# Build options
option(ENABLE_COPILOT "Enable Copilot streaming integration for Win32 IDE" OFF)
option(ENABLE_VULKAN "Enable Vulkan GPU acceleration" ON)
option(ENABLE_MASM_INTEGRATION "Include MASM hotpatch files in QtShell build" OFF)
option(TEST_BRUTAL "Enable Brutal codec test harness" ON)

# Force Windows SDK version that ships all required headers
set(CMAKE_SYSTEM_VERSION "10.0.22621.0")

# Set Qt path
# Qt path: prefer environment-provided Qt6_DIR (e.g. C:/Qt/6.7.3/msvc2022_64/lib/cmake/Qt6)
# or CMAKE_PREFIX_PATH (e.g. C:/Qt/6.7.3/msvc2022_64)
if(DEFINED ENV{Qt6_DIR})
    message(STATUS "Using Qt6 from ENV Qt6_DIR=$ENV{Qt6_DIR}")
    # Qt6_DIR should point to .../lib/cmake/Qt6, so use parent for PREFIX_PATH
    get_filename_component(QT_CMAKE_DIR "$ENV{Qt6_DIR}" DIRECTORY)
    get_filename_component(QT_LIB_DIR "${QT_CMAKE_DIR}" DIRECTORY)
    get_filename_component(QT_ROOT_DIR "${QT_LIB_DIR}" DIRECTORY)
    set(CMAKE_PREFIX_PATH "${QT_ROOT_DIR}")
    message(STATUS "Set CMAKE_PREFIX_PATH to ${CMAKE_PREFIX_PATH}")
endif()

# Explicitly specify Qt6 paths
set(Qt6_DIR "C:/Qt/6.7.3/msvc2022_64/lib/cmake/Qt6")
list(APPEND CMAKE_PREFIX_PATH "C:/Qt/6.7.3/msvc2022_64")

# Try to find Qt6 with Charts, fall back to without if not available
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network Sql Concurrent Test)

# Qt Charts is optional - try to find it but don't fail if missing
find_package(Qt6 COMPONENTS Charts QUIET)
if(Qt6Charts_FOUND)
    message(STATUS "Qt6Charts found - full charting support enabled")
    set(HAVE_QT_CHARTS TRUE)
else()
    message(WARNING "Qt6Charts not found - charting features will be disabled")
    set(HAVE_QT_CHARTS FALSE)
endif()

# Qt HttpServer is optional - try to find it but don't fail if missing
find_package(Qt6 COMPONENTS HttpServer QUIET)
if(Qt6HttpServer_FOUND)
    message(STATUS "Qt6HttpServer found - API server features enabled")
    set(HAVE_QT_HTTPSERVER TRUE)
else()
    message(WARNING "Qt6HttpServer not found - API server features will be disabled")
    set(HAVE_QT_HTTPSERVER FALSE)
endif()

find_package(CURL QUIET)
if(CURL_FOUND)
    message(STATUS "CURL found: ${CURL_VERSION_STRING}")
    set(HAVE_CURL 1)
else()
    message(WARNING "CURL not found - HTTP client will use fallback. Set CURL_DIR or install libcurl for full functionality.")
    set(HAVE_CURL 0)
endif()

find_package(ZSTD QUIET)
if(ZSTD_FOUND)
    message(STATUS "ZSTD found: ${ZSTD_VERSION_STRING}")
    set(HAVE_ZSTD 1)
else()
    message(WARNING "ZSTD not found - compression will use passthrough. Set ZSTD_ROOT or install zstd for full functionality.")
    set(HAVE_ZSTD 0)
endif()

find_package(OpenSSL QUIET)
if(OpenSSL_FOUND)
    message(STATUS "OpenSSL found: ${OPENSSL_VERSION}")
    link_libraries(OpenSSL::Crypto OpenSSL::SSL)
    set(HAVE_OPENSSL 1)
else()
    message(WARNING "OpenSSL not found - JWT verification will be disabled. Set OPENSSL_ROOT_DIR or install OpenSSL for full security features.")
    set(HAVE_OPENSSL 0)
endif()

if(Qt6_FOUND)
    message(STATUS "Qt6 found: ${Qt6_VERSION}")
    add_definitions(-DHAVE_QT_CORE -DQT_NO_OPENGL)
    include_directories(${Qt6_INCLUDE_DIRS})
    link_libraries(Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network Qt6::Sql Qt6::Concurrent Qt6::Test Qt6::Charts)
    
    # ============================================================
    # AUTOMATIC DLL DEPLOYMENT - Deploy ALL dependencies automatically
    # ============================================================
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${Qt6_DIR}/../../../bin")

    # Locate 64-bit MSVC redist (CRT) so we can copy msvcp/vcruntime/concrt DLLs explicitly.
    if(MSVC)
        set(MSVC_REDIST_DIR "")
        set(_msvc_redist_candidates
            "$ENV{VCToolsRedistDir}"
            "C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Redist/MSVC"
            "C:/Program Files (x86)/Microsoft Visual Studio/2022/Community/VC/Redist/MSVC"
        )

        foreach(_cand IN LISTS _msvc_redist_candidates)
            if(NOT _cand)
                continue()
            endif()
            # Candidate may already point at the versioned root (contains x64/...).
            if(EXISTS "${_cand}/x64/Microsoft.VC${MSVC_TOOLSET_VERSION}.CRT")
                set(MSVC_REDIST_DIR "${_cand}/x64/Microsoft.VC${MSVC_TOOLSET_VERSION}.CRT")
                break()
            endif()

            # Otherwise, walk immediate subdirs to find the latest versioned CRT folder.
            file(GLOB _msvc_redist_subdirs "${_cand}/*")
            list(SORT _msvc_redist_subdirs ORDER DESCENDING)
            foreach(_sub IN LISTS _msvc_redist_subdirs)
                if(IS_DIRECTORY "${_sub}" AND EXISTS "${_sub}/x64/Microsoft.VC${MSVC_TOOLSET_VERSION}.CRT")
                    set(MSVC_REDIST_DIR "${_sub}/x64/Microsoft.VC${MSVC_TOOLSET_VERSION}.CRT")
                    break()
                endif()
            endforeach()

            if(MSVC_REDIST_DIR)
                break()
            endif()
        endforeach()

        if(NOT MSVC_REDIST_DIR)
            message(WARNING "MSVC_REDIST_DIR not found; set VCToolsRedistDir or install VS redist so msvcp/vcruntime DLLs (m140) are copied.")
        else()
            message(STATUS "MSVC redist CRT path: ${MSVC_REDIST_DIR}")
        endif()
    endif()
    
    function(deploy_runtime_dependencies target_name)
        if(WIN32 AND WINDEPLOYQT_EXECUTABLE)
            # Use windeployqt to automatically deploy Qt DLLs, plugins, and MSVC runtime
            add_custom_command(TARGET ${target_name} POST_BUILD
                COMMAND "${WINDEPLOYQT_EXECUTABLE}" 
                    --release 
                    --compiler-runtime 
                    --no-translations 
                    --no-system-d3d-compiler
                    --no-opengl-sw
                    "$<TARGET_FILE:${target_name}>"
                WORKING_DIRECTORY "$<TARGET_FILE_DIR:${target_name}>"
                COMMENT "Deploying Qt dependencies for ${target_name}..."
            )
            
            # Additionally copy Qt6Sql.dll explicitly (sometimes missed by windeployqt)
            add_custom_command(TARGET ${target_name} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "$<TARGET_FILE:Qt6::Sql>"
                    "$<TARGET_FILE_DIR:${target_name}>/"
                COMMENT "Ensuring Qt6Sql.dll is deployed for ${target_name}"
            )
            # Copy Windows CRT forwarder DLLs (api-ms-win-crt-*.dll) for x64
            file(GLOB CRT_DLLS "${MSVC_REDIST_DIR}/x64/api-ms-win-crt-*.dll")
            foreach(CRT_DLL IN LISTS CRT_DLLS)
                add_custom_command(TARGET ${target_name} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${CRT_DLL}"
                        "$<TARGET_FILE_DIR:${target_name}>/"
                    COMMENT "Copying ${CRT_DLL} to ${target_name} output"
                )
            endforeach()
            
            # Copy MSVC CRT runtimes explicitly (covers msvcp/vcruntime/concrt "m140" family)
            if(MSVC_REDIST_DIR)
                set(_msvc_crt_dlls
                    msvcp140.dll
                    msvcp140_1.dll
                    msvcp140_2.dll
                    msvcp140_atomic_wait.dll
                    vcruntime140.dll
                    vcruntime140_1.dll
                    concrt140.dll
                    vccorlib140.dll
                )
                foreach(_dll IN LISTS _msvc_crt_dlls)
                    if(EXISTS "${MSVC_REDIST_DIR}/${_dll}")
                        add_custom_command(TARGET ${target_name} POST_BUILD
                            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                                "${MSVC_REDIST_DIR}/${_dll}"
                                "$<TARGET_FILE_DIR:${target_name}>/"
                            COMMENT "Deploying ${_dll} for ${target_name}"
                        )
                    endif()
                endforeach()
            endif()

            # Copy MSVC OpenMP runtime (vcomp140.dll) - search multiple locations
            set(_vcomp_paths
                "C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Redist/MSVC/14.44.35207/onecore/x64/Microsoft.VC143.OpenMP/vcomp140.dll"
                "C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Redist/MSVC/14.44.35112/onecore/x64/Microsoft.VC143.OpenMP/vcomp140.dll"
                "C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Redist/MSVC/14.44.35207/x64/Microsoft.VC143.OpenMP/vcomp140.dll"
            )
            foreach(_vcomp_path IN LISTS _vcomp_paths)
                if(EXISTS "${_vcomp_path}")
                    add_custom_command(TARGET ${target_name} POST_BUILD
                        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                            "${_vcomp_path}"
                            "$<TARGET_FILE_DIR:${target_name}>/"
                        COMMENT "Deploying vcomp140.dll for ${target_name}"
                    )
                    break()
                endif()
            endforeach()
            
            message(STATUS "✓ Automatic DLL deployment enabled for ${target_name}")
        else()
            message(WARNING "windeployqt not found - manual DLL deployment required for ${target_name}")
        endif()
    endfunction()
    
else()
    message(WARNING "Qt6 not found. Qt-based benchmarks will be skipped.")
endif()

# ============================================================
# GGML Submodule Integration (Quantized Transformer Kernels)
# ============================================================
if(EXISTS "${CMAKE_SOURCE_DIR}/3rdparty/ggml/CMakeLists.txt")
    message(STATUS "Adding ggml submodule for quantized inference")
    
    # ggml build options - disable unnecessary components
    set(GGML_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GGML_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GGML_AVX2 ON CACHE BOOL "" FORCE)  # Enable AVX2 for speed
    set(GGML_STATIC ON CACHE BOOL "" FORCE)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    
    # Enable GPU backends (can be overridden by user via -DGGML_VULKAN=OFF)
    if(NOT DEFINED GGML_VULKAN)
        set(GGML_VULKAN ON CACHE BOOL "" FORCE)  # Vulkan for AMD/NVIDIA/Intel
    endif()
    set(GGML_CUDA OFF CACHE BOOL "" FORCE)   # CUDA for NVIDIA only
    set(GGML_METAL OFF CACHE BOOL "" FORCE)  # Metal for macOS only
    
    # Add our include directory first to provide missing Windows SDK headers
    include_directories(BEFORE SYSTEM ${CMAKE_SOURCE_DIR}/include)
    
    # Export include path for nested ExternalProject builds (Vulkan shader gen)
    set(CMAKE_INCLUDE_PATH "${CMAKE_SOURCE_DIR}/include" CACHE STRING "" FORCE)
    set(ENV{INCLUDE} "${CMAKE_SOURCE_DIR}/include;$ENV{INCLUDE}")
    
    add_subdirectory(3rdparty/ggml EXCLUDE_FROM_ALL)
    
    # Create ggml interface library for clean linking
    add_library(ggml_interface INTERFACE)
    target_link_libraries(ggml_interface INTERFACE ggml)
    target_include_directories(ggml_interface INTERFACE 
        ${CMAKE_SOURCE_DIR}/3rdparty/ggml/include
        ${CMAKE_SOURCE_DIR}/3rdparty/ggml/src
    )
    
    # Suppress ggml warnings and add our stubs
    if(TARGET ggml)
        target_include_directories(ggml BEFORE PRIVATE ${CMAKE_SOURCE_DIR}/include)
        if(MSVC)
            target_compile_options(ggml PRIVATE /W0)
        else()
            target_compile_options(ggml PRIVATE -w)
        endif()
    endif()
else()
    message(WARNING "ggml submodule not found - quantized inference will use fallback")
endif()

# ============================================================
# GPU Backend Detection (CUDA, ROCm/HIP, Vulkan)
# ============================================================
option(ENABLE_CUDA "Enable NVIDIA CUDA support" ON)
option(ENABLE_HIP "Enable AMD ROCm/HIP support" ON)
option(ENABLE_VULKAN "Enable Vulkan Compute support" ON)

# Try to find CUDA (NVIDIA)
if(ENABLE_CUDA)
    find_package(CUDAToolkit QUIET)
    if(CUDAToolkit_FOUND)
        message(STATUS "CUDA Toolkit found: ${CUDAToolkit_VERSION}")
        add_compile_definitions(HAVE_CUDA=1)
        set(GPU_BACKEND_LIBS ${GPU_BACKEND_LIBS} CUDA::cudart CUDA::cuda_driver)
    else()
        message(STATUS "CUDA Toolkit not found - NVIDIA GPU support disabled")
    endif()
endif()

# Try to find ROCm/HIP (AMD)
if(ENABLE_HIP)
    find_package(hip QUIET)
    if(hip_FOUND)
        message(STATUS "ROCm/HIP found: ${hip_VERSION}")
        add_compile_definitions(HAVE_HIP=1)
        set(GPU_BACKEND_LIBS ${GPU_BACKEND_LIBS} hip::host)
    else()
        # Try alternative: check if hipcc compiler exists
        find_program(HIPCC_EXECUTABLE hipcc)
        if(HIPCC_EXECUTABLE)
            message(STATUS "HIP compiler found at ${HIPCC_EXECUTABLE}")
            add_compile_definitions(HAVE_HIP=1)
            # Manual HIP setup for Windows
            if(DEFINED ENV{HIP_PATH})
                set(HIP_INCLUDE_DIRS "$ENV{HIP_PATH}/include")
                set(HIP_LIBRARIES "$ENV{HIP_PATH}/lib/amdhip64.lib")
                include_directories(${HIP_INCLUDE_DIRS})
                link_libraries(${HIP_LIBRARIES})
                message(STATUS "Using HIP from HIP_PATH: $ENV{HIP_PATH}")
            endif()
        else()
            message(STATUS "ROCm/HIP not found - AMD GPU support disabled")
            message(STATUS "To enable AMD GPU: Install ROCm for Windows and set HIP_PATH")
        endif()
    endif()
endif()

# Try to find Vulkan (Cross-platform)
if(ENABLE_VULKAN)
    find_package(Vulkan QUIET)
    if(Vulkan_FOUND)
        message(STATUS "Vulkan found: ${Vulkan_VERSION}")
        add_compile_definitions(HAVE_VULKAN=1)
        set(GPU_BACKEND_LIBS ${GPU_BACKEND_LIBS} Vulkan::Vulkan)
    else()
        message(STATUS "Vulkan not found - Vulkan Compute support disabled")
    endif()
endif()

if(NOT CUDAToolkit_FOUND AND NOT hip_FOUND AND NOT HIPCC_EXECUTABLE AND NOT Vulkan_FOUND)
    message(WARNING "========================================")
    message(WARNING "NO GPU BACKEND DETECTED")
    message(WARNING "Application will run in CPU-only mode")
    message(WARNING "========================================")
    message(WARNING "Detected GPU: Run 'Get-WmiObject Win32_VideoController | Select Name'")
    message(WARNING "For NVIDIA: Install CUDA Toolkit from developer.nvidia.com")
    message(WARNING "For AMD: Install ROCm for Windows from rocm.docs.amd.com")
    message(WARNING "========================================")
endif()

# ============================================================
# Advanced Orchestration Modules (Phase 1-6 Implementation)
# ============================================================
message(STATUS "Adding advanced orchestration modules for AI-native IDE")
add_subdirectory(src/orchestration)
add_subdirectory(src/git)
add_subdirectory(src/terminal)

# Qt-based IDE shell (separate executable, no Vulkan)
if(MSVC)
    find_package(Qt6 COMPONENTS Widgets Network Concurrent QUIET)
    if(Qt6_FOUND) # Enabled to build Qt shell
        message(STATUS "Building Qt Shell with Qt version: ${Qt6_VERSION}")
        
        # Enable Qt MOC for QObject classes
        set(CMAKE_AUTOMOC ON)

        # Create a simple Qt console app for now
        add_executable(RawrXD-QtShell 
            src/qtapp/main_qt.cpp
            src/qtapp/MainWindow.cpp
            src/qtapp/MainWindow.h
            # MainWindow_AI_Integration.cpp removed - duplicate definitions merged into MainWindow.cpp
            src/qtapp/ActivityBar.cpp
            src/qtapp/ActivityBar.h
            src/qtapp/ActivityBarButton.cpp
            src/qtapp/ActivityBarButton.h
            src/qtapp/TerminalWidget.cpp
            src/qtapp/TerminalWidget.h
            src/qtapp/TerminalManager.cpp
            src/qtapp/TerminalManager.h
            src/gguf_loader.cpp        # Native C++ loader wrapped by gguf_loader_qt
            src/gguf_vocab_resolver.cpp  # Universal vocab size detection for all GGUF models
            src/qtapp/automated_ide_tester.hpp
            src/qtapp/automated_ide_tester.cpp
            include/gguf_loader.h      # Ensure header is in sources for proper dependency tracking
            src/qtapp/gguf_loader.hpp  # Qt wrapper header
            src/qtapp/gguf_loader.cpp  # Qt wrapper implementation
            src/qtapp/inference_engine.hpp
            src/qtapp/inference_engine.cpp
            src/qtapp/transformer_inference.hpp
            src/qtapp/transformer_inference.cpp
            src/qtapp/bpe_tokenizer.hpp
            src/qtapp/bpe_tokenizer.cpp
            src/qtapp/sentencepiece_tokenizer.hpp
            src/qtapp/sentencepiece_tokenizer.cpp
            src/qtapp/vocabulary_loader.hpp
            src/qtapp/vocabulary_loader.cpp
            src/qtapp/gguf_server.hpp
            src/qtapp/gguf_server.cpp
            src/qtapp/inflate_deflate_cpp.cpp
            src/qtapp/ai_switcher.hpp
            src/qtapp/ai_switcher.cpp
            src/qtapp/unified_backend.hpp
            src/qtapp/unified_backend.cpp
            src/qtapp/widgets/layer_quant_widget.hpp
            src/qtapp/widgets/layer_quant_widget.cpp
            src/qtapp/streaming_inference.hpp
            src/qtapp/streaming_inference.cpp
            src/qtapp/model_monitor.hpp
            src/qtapp/model_monitor.cpp
            src/qtapp/command_palette.hpp
            src/qtapp/command_palette.cpp
            src/qtapp/ai_chat_panel.hpp
            src/qtapp/ai_chat_panel.cpp
            src/qtapp/model_memory_hotpatch.hpp
            src/qtapp/model_memory_hotpatch.cpp
            src/qtapp/byte_level_hotpatcher.hpp
            src/qtapp/byte_level_hotpatcher.cpp
            src/qtapp/gguf_server_hotpatch.hpp
            src/qtapp/gguf_server_hotpatch.cpp
            src/qtapp/unified_hotpatch_manager.hpp
            src/qtapp/unified_hotpatch_manager.cpp
            src/qtapp/proxy_hotpatcher.hpp
            src/qtapp/proxy_hotpatcher.cpp
            # MASM Feature Management System (212 features across 32 categories)
            src/qtapp/masm_feature_manager.hpp
            src/qtapp/masm_feature_manager.cpp
            src/qtapp/masm_feature_settings_panel.hpp
            src/qtapp/masm_feature_settings_panel.cpp
            # Real-Time Integration Layer (3 systems: Coordinator, Terminal Pool, Editor Integration)
            src/real_time_integration_coordinator.hpp
            src/real_time_integration_coordinator.cpp
            src/real_time_terminal_pool.hpp
            src/real_time_terminal_pool.cpp
            src/real_time_editor_integration.hpp
            src/real_time_editor_integration.cpp
            include/orchestration/TaskOrchestrator.h
            src/orchestration/TaskOrchestrator.cpp
            include/orchestration/OrchestrationUI.h
            src/orchestration/OrchestrationUI.cpp
            # Production-ready enterprise components
            src/qtapp/model_queue.hpp
            src/qtapp/model_queue.cpp
            src/qtapp/streaming_inference_api.hpp
            src/qtapp/streaming_inference_api.cpp
            src/qtapp/gpu_backend.hpp
            src/qtapp/gpu_backend.cpp
            src/qtapp/metrics_collector.hpp
            src/qtapp/metrics_collector.cpp
              src/qtapp/backup_manager.hpp
              src/qtapp/backup_manager.cpp
              # src/qtapp/compliance_logger.hpp  # TEMPORARILY DISABLED - Windows header enum conflicts
              # src/qtapp/compliance_logger.cpp  # MOC issue RESOLVED but case expression errors remain
              src/qtapp/sla_manager.hpp
              src/qtapp/sla_manager.cpp
            # ============================================================
            # Phase 3: Production Enhancement Components (5,280 LOC)
            # ============================================================
            # src/qtapp/security_manager.cpp
            # src/qtapp/security_manager.h
            # src/qtapp/distributed_trainer.cpp    # Temporarily disabled due to compilation issues
            # src/qtapp/distributed_trainer.h      # Temporarily disabled due to compilation issues
            # ENHANCED Interpretability Panel - Production Ready (1,500+ LOC)
            src/qtapp/interpretability_panel_enhanced.hpp
            src/qtapp/interpretability_panel_enhanced.cpp
            include/checkpoint_manager.h
            src/qtapp/checkpoint_manager.cpp
            include/tokenizer_selector.h
            src/qtapp/tokenizer_selector.cpp
            # MASM stubs - provides C function implementations when MASM is disabled
            # (These are only used when ENABLE_MASM_INTEGRATION=OFF)
            $<$<NOT:$<BOOL:${ENABLE_MASM_INTEGRATION}>>:src/qtapp/masm_orchestration_stubs.cpp>
            include/ci_cd_settings.h
            src/qtapp/ci_cd_settings.cpp
            # Project detection and metadata management
            src/qtapp/utils/project_detector.cpp
            # File and directory operations
            src/qtapp/utils/qt_file_writer.cpp
            src/qtapp/utils/qt_directory_manager.cpp
            src/qtapp/settings_dialog.h
            src/qtapp/settings_dialog.cpp
            src/qtapp/settings_manager.h
            src/qtapp/settings_manager.cpp
            src/qtapp/latency_monitor.h
            src/qtapp/latency_monitor.cpp
            src/qtapp/latency_status_panel.h
            src/qtapp/latency_status_panel.cpp
            src/qtapp/advanced_checkpoint_manager.cpp
            src/qtapp/advanced_checkpoint_manager.h
            src/qtapp/tokenizer_language_selector.cpp
            src/qtapp/tokenizer_language_selector.h
            src/qtapp/ci_pipeline_manager.cpp
            src/qtapp/ci_pipeline_manager.h
            src/qtapp/codec.h
            src/qtapp/codec.cpp
            # ============================================================
            # Phase 4: AI Code Assistant (Code Completion with MASM Transport)
            # ============================================================
            src/qtapp/ai_code_assistant.h
            src/qtapp/ai_code_assistant.cpp
            src/qtapp/ai_code_assistant_panel.h
            src/qtapp/ai_code_assistant_panel.cpp
            src/agent/agentic_puppeteer.hpp
            src/agent/agentic_puppeteer.cpp
            src/agent/agentic_failure_detector.hpp
            src/agent/agentic_failure_detector.cpp
            src/agent/agentic_copilot_bridge.hpp
            src/agent/agentic_copilot_bridge.cpp
            # ============================================================
            # Hot-Patching System: Real-time hallucination detection & correction
            # ============================================================
            # src/agent/agent_hot_patcher.cpp
            # src/agent/gguf_proxy_server.cpp
            # src/agent/ide_agent_bridge_hot_patching_integration.cpp
            src/llm_adapter/QuantBackend.cpp
            src/llm_adapter/QuantBackend.h
            src/agent/auto_bootstrap.cpp
            src/qtapp/widgets/masm_editor_widget.cpp
            src/qtapp/widgets/hotpatch_panel.cpp
            src/qtapp/widgets/layer_quant_widget.cpp
            # src/agent/zero_touch.cpp
            # kernels/quant_ladder_avx2.cpp  # Conflicts with ggml quantization functions
            # src/agent/planner.cpp
            # src/agent/self_patch.cpp
            # src/agent/sign_binary.cpp
            # src/agent/release_agent.cpp
            src/agent/meta_learn.cpp
            src/agent/hot_reload.cpp
            src/agent/auto_update.cpp
            src/agent/planner.cpp
            src/agent/self_patch.cpp
            src/agent/release_agent.cpp
            src/agent/zero_touch.cpp
            src/agent/sentry_integration.hpp
            src/agent/sentry_integration.cpp
            src/agent/code_signer.hpp
            src/agent/code_signer.cpp
            src/agent/telemetry_collector.hpp
            src/agent/telemetry_collector.cpp
            # Agent execution and planning modules
            src/agent/action_executor.hpp
            src/agent/action_executor.cpp
            src/agent/meta_planner.hpp
            src/agent/meta_planner.cpp
            # Project UI components
            src/qtapp/widgets/project_explorer.cpp
            src/qtapp/widgets/project_explorer.h
            src/qtapp/widgets/breadcrumb_navigation.cpp
            src/qtapp/widgets/breadcrumb_navigation.hpp
            # TODO Management
            src/qtapp/todo_manager.h
            src/qtapp/todo_manager.cpp
            # Agent Chat enhancements with breadcrumb and model selector
            src/qtapp/agent_chat_breadcrumb.hpp
            src/qtapp/agent_chat_breadcrumb.cpp
            # Code Minimap Widget - Visual code overview
            src/qtapp/code_minimap.h
            src/qtapp/code_minimap.cpp
            src/qtapp/editor_with_minimap.h
            src/qtapp/editor_with_minimap.cpp
            # Code quality improvement modules
            src/utils/ErrorReporter.hpp
            src/utils/ErrorReporter.cpp
            src/model_loader/GGUFConstants.hpp
            src/utils/Diagnostics.hpp
            src/utils/Diagnostics.cpp
            # ============================================================
            # RawrXD Cryptographic Library (OpenSSL replacement)
            # Production-ready from-scratch implementation
            # ============================================================
            src/crypto/rawrxd_crypto.h
            src/crypto/rawrxd_crypto_base.cpp
            src/crypto/rawrxd_crypto_bigint.cpp
            src/crypto/rawrxd_crypto_rsa_ecc.cpp
            src/crypto/rawrxd_crypto_aes.cpp
            # Full Utility Tool Registry with Production Readiness
            # TODO: Fix Windows RPC macro conflicts with nlohmann::json in tool_registry.cpp
            # include/tool_registry.hpp
            # src/tool_registry.cpp
            # ============================================================
            # GitHub-Style Enterprise Tools Panel (44 Tools)
            # Production-ready tool management with toggle controls
            # ============================================================
            src/qtapp/enterprise_tools_panel.h
            src/qtapp/enterprise_tools_panel.cpp
            # Experimental Features Menu (quantization, KV cache, streaming, pruning)
            src/qtapp/experimental_features_menu.hpp
            src/qtapp/experimental_features_menu.cpp
            # MASM Orchestration Wrapper (temporary until MASM compilation fixed)
            src/masm/masm_orchestration_wrapper.cpp
        )

        # MASM Orchestration OBJECT Library (beaconism pattern)
        # Creates object files that get directly linked into RawrXD-QtShell
        # NOTE: Only includes MASM files that compile without errors
        if(ENABLE_MASM_INTEGRATION)
            add_library(masm_ide_orchestration_obj OBJECT
                src/masm/final-ide/ai_orchestration_glue.asm
                src/masm/final-ide/system_init_stubs.asm
                src/masm/final-ide/agent_orchestrator_main.asm
                # Pure MASM core + brutal bridges
                src/masm/final-ide/rawr_core_masm.asm
                src/masm/final-ide/masm_brutal_bridges.asm
                # Disabled: ai_orchestration_coordinator.asm - multiplydefined with ai_orchestration_glue
                # src/masm/final-ide/ai_orchestration_coordinator.asm
                # Disabled: autonomous_task_executor_clean.asm - duplicate of original
                # src/masm/final-ide/autonomous_task_executor_clean.asm
                # Disabled: output_pane_logger_clean.asm - duplicate of original
                # src/masm/final-ide/output_pane_logger_clean.asm
                # Disabled: masm_gpu_backend_clean.asm - duplicate of original
                # src/masm/final-ide/masm_gpu_backend_clean.asm
                src/masm/final-ide/masm_security_manager.asm
                src/masm/final-ide/agentic_failure_detector.asm
                src/masm/final-ide/masm_inference_engine.asm
                src/masm/final-ide/agentic_copilot_bridge.asm
                src/masm/final-ide/agentic_masm_system.asm
                # Disabled: agent_utility_agents.asm - multiply defined with system_init_stubs
                # src/masm/final-ide/agent_utility_agents.asm
                src/masm/final-ide/agent_meta_learn.asm
                src/masm/final-ide/masm_metrics_collector.asm
                src/masm/final-ide/masm_tokenizer.asm
                src/masm/final-ide/masm_proxy_server.asm
                src/masm/final-ide/console_log_simple.asm
                # src/masm/final-ide/agent_chat_hotpatch_bridge.asm  # invalid instruction operands
                src/masm/final-ide/masm_qt_bridge.asm
                src/masm/final-ide/missing_stubs_minimal.asm
                # Disabled: ai_orchestration_glue_clean.asm - duplicate of original
                # src/masm/final-ide/ai_orchestration_glue_clean.asm
                # Disabled: system_init_stubs_clean.asm - duplicate of original
                # src/masm/final-ide/system_init_stubs_clean.asm
                src/masm/final-ide/ui_phase1_implementations_fixed.asm
                # Disabled: chat_persistence_phase2.asm - rip usage and undefined labels
                # src/masm/final-ide/chat_persistence_phase2.asm
                # Disabled: agentic_nlp_phase3.asm - complex syntax issues
                # src/masm/final-ide/agentic_nlp_phase3.asm
                # Disabled: menu_system.asm - needs review
                # src/masm/final-ide/menu_system.asm
                # Disabled: masm_theme_system_complete.asm - needs review
                # src/masm/final-ide/masm_theme_system_complete.asm
                # Disabled: masm_file_browser_complete.asm - needs review
                # src/masm/final-ide/masm_file_browser_complete.asm
                # Disabled: phase2_integration.asm - needs review
                # src/masm/final-ide/phase2_integration.asm
            )
            # Set MASM-specific properties on all sources
            set_target_properties(masm_ide_orchestration_obj PROPERTIES
                LINKER_LANGUAGE CXX
            )
            # Mark all sources as MASM
            get_target_property(MASM_SOURCES masm_ide_orchestration_obj SOURCES)
            set_source_files_properties(${MASM_SOURCES} PROPERTIES LANGUAGE ASM_MASM)
            message(STATUS "MASM orchestration OBJECT library created with ${CMAKE_CURRENT_LIST_LINE} sources")
        endif()
        
        add_library(self_test_gate STATIC
            src/agent/self_test.cpp
            src/agent/self_test_gate.cpp
            src/agent/rollback.cpp
        )
        target_link_libraries(self_test_gate PRIVATE Qt6::Core Qt6::Network)
        target_include_directories(self_test_gate PRIVATE ${CMAKE_SOURCE_DIR}/src/agent)

        # Small shared quant utils library (used by QtShell and tests)
        add_library(quant_utils STATIC src/qtapp/quant_utils.cpp)
        target_include_directories(quant_utils PUBLIC ${CMAKE_SOURCE_DIR}/src/qtapp ${CMAKE_SOURCE_DIR}/include)
        target_link_libraries(quant_utils PUBLIC Qt6::Core)
        
        # RawrXD Cryptographic Library - No external dependencies needed
        # All cryptographic operations use our from-scratch implementation
        add_compile_definitions(HAVE_RAWRXD_CRYPTO=1)
        message(STATUS "RawrXD Cryptographic Library integrated - full crypto support enabled")
        
        # Find zlib for CheckpointManager compression (optional with fallback)
        find_package(ZLIB QUIET)
        if(ZLIB_FOUND)
            message(STATUS "ZLIB found - CheckpointManager compression enabled")
            add_compile_definitions(HAVE_ZLIB=1)
        else()
            message(STATUS "ZLIB not found - CheckpointManager will use no compression")
        endif()

        # Link Qt modules including WebSockets for swarm collaboration
        # Use beaconism pattern for MASM object files
        target_link_libraries(RawrXD-QtShell PRIVATE
            Qt6::Core
            Qt6::Gui
            Qt6::Widgets
            Qt6::Network
            Qt6::Sql
            Qt6::Concurrent
            brutal_gzip
            self_test_gate
            quant_utils
            $<$<BOOL:${ENABLE_MASM_INTEGRATION}>:$<TARGET_OBJECTS:masm_ide_orchestration_obj>>
            $<$<TARGET_EXISTS:masm_universal_format_loader>:masm_universal_format_loader>
            # Temporarily disable Phase 2 for testing
            # $<$<TARGET_EXISTS:masm_phase2_parsers>:masm_phase2_parsers>
            $<$<TARGET_EXISTS:masm_test_generator>:masm_test_generator>
            Psapi
            dwmapi
            uxtheme
            d2d1
            dwrite
            ole32
            shlwapi
        )
        
        # Add include directories for QtShell to access both qtapp and main source headers
        target_include_directories(RawrXD-QtShell PRIVATE 
            ${CMAKE_SOURCE_DIR}/src
            ${CMAKE_SOURCE_DIR}/src/qtapp
            ${CMAKE_SOURCE_DIR}/include
        )
        
        # Conditionally link OpenSSL if found
        if(OpenSSL_FOUND)
            target_link_libraries(RawrXD-QtShell PRIVATE OpenSSL::Crypto OpenSSL::SSL)
        endif()
        
        # Conditionally link ZLIB if found
        if(ZLIB_FOUND)
            target_link_libraries(RawrXD-QtShell PRIVATE ZLIB::ZLIB)
        endif()
        
        # Try to find Qt WebSockets (optional for swarm editing)
        find_package(Qt6 COMPONENTS WebSockets QUIET)
        if(Qt6WebSockets_FOUND)
            target_link_libraries(RawrXD-QtShell PRIVATE Qt6::WebSockets)
            target_compile_definitions(RawrXD-QtShell PRIVATE HAVE_QT_WEBSOCKETS=1)
            message(STATUS "Qt WebSockets found - swarm collaboration enabled")
        else()
            message(STATUS "Qt WebSockets not found - swarm collaboration disabled")
        endif()
        
        # Link ggml if available
        if(TARGET ggml)
            target_link_libraries(RawrXD-QtShell PRIVATE ggml_interface)
            target_compile_definitions(RawrXD-QtShell PRIVATE HAVE_GGML=1)
            message(STATUS "RawrXD-QtShell: ggml quantization enabled")
        endif()
        
        target_include_directories(RawrXD-QtShell PRIVATE 
            include 
            ${CMAKE_SOURCE_DIR}/src 
            ${CMAKE_SOURCE_DIR}/src/qtapp 
            ${CMAKE_SOURCE_DIR}/src/llm_adapter 
            ${CMAKE_SOURCE_DIR}/src/agent
            ${CMAKE_SOURCE_DIR}/src/utils
            ${CMAKE_SOURCE_DIR}/src/model_loader
        )
        
        set_target_properties(RawrXD-QtShell PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
            WIN32_EXECUTABLE TRUE
            AUTOMOC ON
            AUTOMOC_MOC_OPTIONS "-I${CMAKE_SOURCE_DIR}/include;-I${CMAKE_CURRENT_SOURCE_DIR}/src/qtapp"
        )

        # ------------------------------------------------------------
        # OPTIONAL UI PANELS – compiled but not shown on startup
        # ------------------------------------------------------------
        # MASM editor widget (assembly editing with brutal_gzip compression)
        target_sources(RawrXD-QtShell PRIVATE 
            src/qtapp/widgets/masm_editor_widget.cpp
        )

        # Model tuner / multi‑file search panel (also uses brutal_gzip)
        target_sources(RawrXD-QtShell PRIVATE 
            src/qtapp/widgets/multi_file_search.cpp
        )

        if(MSVC)
            target_compile_options(RawrXD-QtShell PRIVATE /EHsc $<$<CONFIG:Release>:/O2>)
        else()
            target_compile_options(RawrXD-QtShell PRIVATE -fexceptions $<$<CONFIG:Release>:-O2>)
        endif()
        
        # Add build stubs for missing symbols
        target_sources(RawrXD-QtShell PRIVATE src/build_stubs.cpp)
        
        # Automatic DLL deployment for RawrXD-QtShell
        deploy_runtime_dependencies(RawrXD-QtShell)
        
        # ---------- Production Feature Test: Real component testing ----------
        add_executable(production_feature_test
            src/qtapp/production_feature_test.cpp
            src/qtapp/gpu_backend.cpp
            src/qtapp/metrics_collector.cpp
            src/qtapp/backup_manager.cpp
            src/qtapp/sla_manager.cpp
        )
        
        target_link_libraries(production_feature_test PRIVATE
            Qt6::Core
            Qt6::Gui
            Qt6::Widgets
            Qt6::Network
            ${GPU_BACKEND_LIBS}
        )
        
        # Link Vulkan if enabled
        if(Vulkan_FOUND)
            target_link_libraries(production_feature_test PRIVATE Vulkan::Vulkan)
        endif()
        
        target_include_directories(production_feature_test PRIVATE 
            include 
            ${CMAKE_SOURCE_DIR}/src 
            ${CMAKE_SOURCE_DIR}/src/qtapp
        )
        
        # Explicitly apply GPU backend definitions
        if(CUDAToolkit_FOUND)
            target_compile_definitions(production_feature_test PRIVATE HAVE_CUDA=1)
        endif()
        if(hip_FOUND OR HIPCC_EXECUTABLE)
            target_compile_definitions(production_feature_test PRIVATE HAVE_HIP=1)
        endif()
        if(Vulkan_FOUND)
            target_compile_definitions(production_feature_test PRIVATE HAVE_VULKAN=1)
        endif()
        
        set_target_properties(production_feature_test PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        )
        
        # ---------- Simple GPU Test: Quick Vulkan verification ----------
        add_executable(simple_gpu_test
            src/qtapp/simple_gpu_test.cpp
            src/qtapp/gpu_backend.cpp
            src/qtapp/metrics_collector.cpp
        )
        
        target_link_libraries(simple_gpu_test PRIVATE
            Qt6::Core
            ${GPU_BACKEND_LIBS}
        )
        
        # Link ggml for Vulkan compute support
        if(TARGET ggml)
            target_link_libraries(simple_gpu_test PRIVATE ggml)
        endif()
        
        if(Vulkan_FOUND)
            target_link_libraries(simple_gpu_test PRIVATE Vulkan::Vulkan)
        endif()
        
        target_include_directories(simple_gpu_test PRIVATE 
            include 
            ${CMAKE_SOURCE_DIR}/src 
            ${CMAKE_SOURCE_DIR}/src/qtapp
            ${CMAKE_SOURCE_DIR}/3rdparty/ggml/include
        )
        
        # Explicitly apply GPU backend definitions
        if(CUDAToolkit_FOUND)
            target_compile_definitions(simple_gpu_test PRIVATE HAVE_CUDA=1)
        endif()
        if(hip_FOUND OR HIPCC_EXECUTABLE)
            target_compile_definitions(simple_gpu_test PRIVATE HAVE_HIP=1)
        endif()
        if(Vulkan_FOUND)
            target_compile_definitions(simple_gpu_test PRIVATE HAVE_VULKAN=1)
        endif()
        
        set_target_properties(simple_gpu_test PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        )
        
        # ---------- GPU Inference Benchmark: Real GGUF model performance test ----------
        add_executable(gpu_inference_benchmark
            src/qtapp/gpu_inference_benchmark.cpp
            src/qtapp/inference_engine.cpp
            src/qtapp/gguf_loader.cpp
            src/gguf_loader.cpp
            src/gguf_vocab_resolver.cpp  # Universal vocab size detection
            src/qtapp/transformer_inference.cpp
            src/qtapp/bpe_tokenizer.cpp
            src/qtapp/sentencepiece_tokenizer.cpp
            src/qtapp/vocabulary_loader.cpp
            src/qtapp/gpu_backend.cpp
            src/qtapp/metrics_collector.cpp
            src/qtapp/quant_utils.cpp
        )
        
        target_link_libraries(gpu_inference_benchmark PRIVATE
            Qt6::Core
            brutal_gzip
            ${GPU_BACKEND_LIBS}
        )
        
        if(TARGET ggml)
            target_link_libraries(gpu_inference_benchmark PRIVATE ggml)
        endif()
        
        if(Vulkan_FOUND)
            target_link_libraries(gpu_inference_benchmark PRIVATE Vulkan::Vulkan)
            target_compile_definitions(gpu_inference_benchmark PRIVATE HAVE_VULKAN=1)
        endif()
        
        target_include_directories(gpu_inference_benchmark PRIVATE 
            include 
            ${CMAKE_SOURCE_DIR}/src 
            ${CMAKE_SOURCE_DIR}/src/qtapp
            ${CMAKE_SOURCE_DIR}/3rdparty/ggml/include
        )
        
        set_target_properties(gpu_inference_benchmark PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        )
        
        # ---------- Minimal Qt Test ----------
        add_executable(minimal_qt_test
            src/minimal_qt_test.cpp
        )
        
        target_link_libraries(minimal_qt_test PRIVATE 
            Qt6::Core
            Qt6::Gui
            Qt6::Widgets
        )
        
        target_include_directories(minimal_qt_test PRIVATE 
            include 
            ${CMAKE_SOURCE_DIR}/src 
            ${CMAKE_SOURCE_DIR}/src/qtapp
            ${CMAKE_SOURCE_DIR}/3rdparty/ggml/include
        )
        
        set_target_properties(minimal_qt_test PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        )
        
        # ---------- GGUF Hotpatch Tester: Command-line tool for real hotpatch testing ----------
        add_executable(gguf_hotpatch_tester
            src/qtapp/gguf_hotpatch_tester.cpp
            src/qtapp/inference_engine.cpp
            src/qtapp/gguf_loader.cpp
            src/gguf_loader.cpp
            src/gguf_vocab_resolver.cpp  # Universal vocab size detection
            src/qtapp/transformer_inference.cpp
            src/qtapp/bpe_tokenizer.cpp
            src/qtapp/sentencepiece_tokenizer.cpp
            src/qtapp/vocabulary_loader.cpp
            src/qtapp/gpu_backend.cpp
            src/qtapp/metrics_collector.cpp
            src/qtapp/quant_utils.cpp
        )
        
        target_link_libraries(gguf_hotpatch_tester PRIVATE
            Qt6::Core
            brutal_gzip
            ${GPU_BACKEND_LIBS}
        )
        
        if(TARGET ggml)
            target_link_libraries(gguf_hotpatch_tester PRIVATE ggml)
        endif()
        
        if(Vulkan_FOUND)
            target_link_libraries(gguf_hotpatch_tester PRIVATE Vulkan::Vulkan)
            target_compile_definitions(gguf_hotpatch_tester PRIVATE HAVE_VULKAN=1)
        endif()
        
        target_include_directories(gguf_hotpatch_tester PRIVATE 
            include 
            ${CMAKE_SOURCE_DIR}/src 
            ${CMAKE_SOURCE_DIR}/src/qtapp
            ${CMAKE_SOURCE_DIR}/3rdparty/ggml/include
        )
        
        set_target_properties(gguf_hotpatch_tester PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        )
        
        # ---------- Test Chat Streaming: Console streaming chat test ----------
        add_executable(test_chat_streaming
            src/qtapp/test_chat_streaming.cpp
            src/qtapp/inference_engine.cpp
            src/qtapp/gguf_loader.cpp
            src/gguf_loader.cpp
            src/gguf_vocab_resolver.cpp  # Universal vocab size detection
            src/qtapp/transformer_inference.cpp
            src/qtapp/bpe_tokenizer.cpp
            src/qtapp/sentencepiece_tokenizer.cpp
            src/qtapp/vocabulary_loader.cpp
            src/qtapp/gpu_backend.cpp
            src/qtapp/metrics_collector.cpp
            src/qtapp/quant_utils.cpp
        )
        
        target_link_libraries(test_chat_streaming PRIVATE
            Qt6::Core
            Qt6::Gui
            Qt6::Widgets
            brutal_gzip
            ${GPU_BACKEND_LIBS}
        )
        
        if(TARGET ggml)
            target_link_libraries(test_chat_streaming PRIVATE ggml)
        endif()
        
        if(Vulkan_FOUND)
            target_link_libraries(test_chat_streaming PRIVATE Vulkan::Vulkan)
            target_compile_definitions(test_chat_streaming PRIVATE HAVE_VULKAN=1)
        endif()
        
        target_include_directories(test_chat_streaming PRIVATE 
            include 
            ${CMAKE_SOURCE_DIR}/src 
            ${CMAKE_SOURCE_DIR}/src/qtapp
            ${CMAKE_SOURCE_DIR}/3rdparty/ggml/include
        )
        
        set_target_properties(test_chat_streaming PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        )
        
        # ---------- RawrXD-Agent: Autonomous coding agent executable ----------
        add_executable(RawrXD-Agent
            src/agent/agent_main.cpp
            src/agent/planner.cpp
            src/agent/self_patch.cpp
            src/agent/sign_binary.cpp
            src/agent/release_agent.cpp
            src/agent/meta_learn.cpp
            src/agent/hot_reload.cpp
        )
        
        target_link_libraries(RawrXD-Agent PRIVATE 
            Qt6::Core 
            Qt6::Network
            Qt6::Concurrent
            Qt6::Sql
            self_test_gate
        )
        
        target_include_directories(RawrXD-Agent PRIVATE 
            include 
            ${CMAKE_SOURCE_DIR}/src 
            ${CMAKE_SOURCE_DIR}/src/agent
            ${CMAKE_SOURCE_DIR}/src/orchestration
            ${CMAKE_SOURCE_DIR}/src/git
            ${CMAKE_SOURCE_DIR}/src/terminal
        )
        
        set_target_properties(RawrXD-Agent PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
            WIN32_EXECUTABLE FALSE
        )
        
        if(MSVC)
            target_compile_options(RawrXD-Agent PRIVATE /EHsc $<$<CONFIG:Release>:/O2>)
        else()
            target_compile_options(RawrXD-Agent PRIVATE -fexceptions $<$<CONFIG:Release>:-O2>)
        endif()
        
        message(STATUS "RawrXD-Agent: Autonomous coding agent enabled")
        
    else()
        message(STATUS "Qt6 not found, skipping RawrXD-QtShell")
    endif()
else()
    message(STATUS "MSVC not detected; skipping Qt6 targets to avoid MinGW/MSVC mismatch")
endif()

# Win32-based IDE (MSVC only to match Qt)
if(MSVC) # Re-enabled to test
    add_executable(RawrXD-Win32IDE 
        src/win32app/main_win32.cpp
        src/win32app/Win32IDE.cpp
        src/win32app/Win32IDE.h
        src/win32app/Win32IDE_Sidebar.cpp
        src/win32app/Win32IDE_VSCodeUI.cpp
        src/win32app/Win32IDE_PowerShellPanel.cpp
        src/win32app/Win32IDE_PowerShell.cpp
        src/win32app/Win32IDE_Logger.cpp
        src/win32app/TransparentRenderer.cpp
        src/win32app/TransparentRenderer.h
        src/win32app/Win32TerminalManager.cpp
        src/win32app/Win32TerminalManager.h
        src/win32app/Win32IDE_FileOps.cpp
        src/win32app/Win32IDE_Commands.cpp
        src/win32app/Win32IDE_Debugger.cpp
        src/win32app/Win32IDE_AgenticBridge.h
        src/win32app/Win32IDE_AgenticBridge.cpp
        src/win32app/Win32IDE_AgentCommands.cpp
        src/win32app/Win32IDE_Autonomy.h
        src/win32app/Win32IDE_Autonomy.cpp
        src/gguf_loader.cpp
        src/gguf_vocab_resolver.cpp
        src/streaming_gguf_loader.cpp
        src/utils/ErrorReporter.cpp
        src/utils/Diagnostics.cpp
        # Benchmark menu integration (headers required for Q_OBJECT moc)
        include/benchmark_menu_widget.hpp
        include/benchmark_runner.hpp
        src/benchmark_menu_widget.cpp
        src/benchmark_runner.cpp
        $<$<BOOL:${ENABLE_VULKAN}>:src/win32app/VulkanRenderer.cpp>
        $<$<BOOL:${ENABLE_COPILOT}>:src/win32app/Win32IDE_CopilotIntegration.cpp>
        $<$<BOOL:${ENABLE_COPILOT}>:src/win32app/CopilotStreaming.cpp>
        # Agentic navigation integration
        src/agentic/AgenticNavigator.cpp
        src/agentic/AgenticCopilotIntegration.cpp
        # Qt-based IDE Main Window
        src/ide_main_window.h
        src/ide_main_window.cpp
        src/autonomous_widgets.h
        src/autonomous_widgets.cpp
        # Model Router Components
        # Universal Model Router Integration (Cloud AI + Local GGUF)
        src/universal_model_router.cpp
        src/cloud_api_client.cpp
        src/model_interface.cpp
        # Model Router Qt Adapter for IDE Integration
        src/model_router_adapter.h
        src/model_router_adapter.cpp
        # Model Router GUI Widget
        src/model_router_widget.h
        src/model_router_widget.cpp
        # Cloud Settings Dialog
        src/cloud_settings_dialog.h
        src/cloud_settings_dialog.cpp
        # Metrics Dashboard
        src/metrics_dashboard.h
        src/metrics_dashboard.cpp
        # Model Router Console
        src/model_router_console.h
        src/model_router_console.cpp
        # Autonomous IDE Components (missing implementations for linker)
        src/autonomous_model_manager.cpp
        src/autonomous_resource_manager.cpp
        src/agentic_learning_system.cpp
        src/compression_interface.cpp
        src/autonomous_feature_engine.cpp
        src/intelligent_codebase_engine.cpp
        src/hybrid_cloud_manager.cpp
        src/error_recovery_system.cpp
        src/performance_monitor.cpp
        # Real-time completion and inference
        src/real_time_completion_engine.cpp
        src/qtapp/inference_engine.cpp
        # Additional tokenizer/inference support
        src/qtapp/bpe_tokenizer.cpp
        src/qtapp/sentencepiece_tokenizer.cpp
        src/qtapp/vocabulary_loader.cpp
        src/qtapp/transformer_inference.cpp
        src/qtapp/gguf_loader.cpp
        # UI Widgets and Panels (Q_OBJECT compilation)
        src/qtapp/health_check_server.cpp
        src/qtapp/layer_quant_widget.cpp
        src/qtapp/agentic_failure_detector.cpp
        src/qtapp/agentic_puppeteer.cpp
        src/qtapp/agent_mode_handler.cpp
        src/qtapp/ask_mode_handler.cpp
        src/qtapp/plan_mode_handler.cpp
        src/qtapp/StreamingGGUFLoader.cpp
        # Headers requiring MOC processing for Q_OBJECT
        src/qtapp/health_check_server.hpp
        src/qtapp/layer_quant_widget.hpp
        src/qtapp/agent_mode_handler.hpp
        src/qtapp/ask_mode_handler.hpp
        src/qtapp/plan_mode_handler.hpp
        src/qtapp/StreamingGGUFLoader.hpp
        src/qtapp/interpretability_panel_production.hpp
    )
    
    target_link_libraries(RawrXD-Win32IDE PRIVATE
        comctl32
        d3d11
        dxgi
        dcomp
        dwmapi
        d2d1
        dwrite
        d3dcompiler
        winhttp
        shlwapi
        shell32
        RawrXDOrchestration
        RawrXDGit
        RawrXDTerminal
        brutal_gzip
        quant_utils
        Qt6::Core
        Qt6::Network
        Qt6::Widgets
        Qt6::Charts
    )
    
    # Add include dir first to pick up our d3d10effect.h and ggml.h stubs before system SDK
    target_include_directories(RawrXD-Win32IDE BEFORE PRIVATE 
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/utils
        ${CMAKE_SOURCE_DIR}/src/model_loader
        ${CMAKE_SOURCE_DIR}/src/qtapp
        ${CMAKE_SOURCE_DIR}/3rdparty/ggml/include
        ${CMAKE_SOURCE_DIR}/3rdparty/ggml/src
    )
    
    # Link ggml if available
    if(TARGET ggml)
        target_link_libraries(RawrXD-Win32IDE PRIVATE ggml)
        target_compile_definitions(RawrXD-Win32IDE PRIVATE HAVE_GGML=1)
    endif()
    
    # Enable Qt MOC for Q_OBJECT headers in widgets
    set_target_properties(RawrXD-Win32IDE PROPERTIES 
        AUTOMOC ON
        AUTOMOC_MOC_OPTIONS "-I${CMAKE_SOURCE_DIR}/include;-I${CMAKE_CURRENT_SOURCE_DIR}/src/qtapp"
    )
    
    # Compiler options - MSVC only now
    target_compile_options(RawrXD-Win32IDE PRIVATE /EHsc /O2)
    
    # Force MBCS/ANSI build to avoid Unicode string conversion issues
    target_compile_definitions(RawrXD-Win32IDE PRIVATE _MBCS)
    
    set_target_properties(RawrXD-Win32IDE PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        WIN32_EXECUTABLE TRUE
    )
    if(ENABLE_VULKAN)
        target_compile_definitions(RawrXD-Win32IDE PRIVATE ENABLE_VULKAN=1)
    endif()
    if(ENABLE_COPILOT)
        target_compile_definitions(RawrXD-Win32IDE PRIVATE ENABLE_COPILOT=1)
        message(STATUS "Copilot integration enabled for Win32 IDE")
    endif()
endif() # End MSVC-only Win32IDE block

# Terminal test executable (MSVC only, placed in bin-msvc)
if(FALSE AND MSVC AND EXISTS "${CMAKE_SOURCE_DIR}/src/win32app/test_terminal.cpp") # Disabled until headers exist
    add_executable(test_terminal
        src/win32app/test_terminal.cpp
        src/win32app/Win32TerminalManager.cpp
        src/win32app/Win32TerminalManager.h
    )
    set_target_properties(test_terminal PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin-msvc"
    )
endif()

# Chat application (MSVC only, taskbar integration)
if(FALSE AND MSVC AND ENABLE_COPILOT AND EXISTS "${CMAKE_SOURCE_DIR}/src/win32app/main_chat.cpp") # Disabled until headers exist
    add_executable(RawrXD-Chat
        src/win32app/main_chat.cpp
        src/win32app/Win32ChatApp.cpp
        src/win32app/Win32ChatApp.h
        src/win32app/ContextManager.h
    )

    target_link_libraries(RawrXD-Chat PRIVATE
        comctl32
        shell32
        winhttp
    )

    target_include_directories(RawrXD-Chat PRIVATE
        ${CMAKE_SOURCE_DIR}/include
    )

    target_compile_options(RawrXD-Chat PRIVATE /EHsc)

    set_target_properties(RawrXD-Chat PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin-msvc"
        WIN32_EXECUTABLE TRUE
    )
endif()

# Vulkan SDK (optional now; can be disabled if not installed)
option(BUILD_TESTS "Build unit tests (disables Vulkan requirement)" OFF)
# ENABLE_VULKAN defined earlier at line 105 - removed duplicate definition
if(ENABLE_VULKAN)
    if(NOT BUILD_TESTS)
        find_package(Vulkan REQUIRED)
    else()
        find_package(Vulkan QUIET)
    endif()
else()
    message(STATUS "Vulkan disabled via ENABLE_VULKAN=OFF; skipping find_package")
endif()


# Compiler selection: do not force MinGW globally. Let the generator/toolchain decide.
# If you need MinGW, configure the build folder with `-G "MinGW Makefiles"`.

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

if(FALSE) # Disabled until headers exist
# Add main executable
add_executable(RawrXD-ModelLoader
    src/main.cpp
    src/gguf_loader.cpp
    src/vulkan_compute.cpp
    src/hf_downloader.cpp
    src/gui.cpp
    src/api_server.cpp
    src/settings.cpp
    src/telemetry.cpp
    src/telemetry/ai_metrics.cpp
    src/overclock_vendor.cpp
    src/overclock_governor.cpp
    src/baseline_profile.cpp
)

# Console-based IDE (non-Qt) interactive shell
add_executable(RawrXD-CLI
    src/rawrxd_cli.cpp
    src/gguf_loader.cpp
    src/vulkan_compute.cpp
    src/hf_downloader.cpp
    src/api_server.cpp
    src/settings.cpp
    src/telemetry.cpp
    src/overclock_vendor.cpp
    src/overclock_governor.cpp
    src/baseline_profile.cpp
)

target_link_libraries(RawrXD-CLI PRIVATE wbemuuid pdh)
if(Vulkan_FOUND)
    target_link_libraries(RawrXD-CLI PRIVATE Vulkan::Vulkan)
endif()
set_target_properties(RawrXD-CLI PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin-${COMPILER_SUFFIX}")

if(Qt6Widgets_FOUND)
    message(STATUS "Qt6Widgets already configured above")
endif()

# Link libraries
target_link_libraries(RawrXD-ModelLoader PRIVATE
    ws2_32
    winmm
    imm32
    ole32
    oleaut32
    winspool
    advapi32
    shell32
    user32
    gdi32
    wbemuuid
    pdh
)

if(Vulkan_FOUND)
    target_link_libraries(RawrXD-ModelLoader PRIVATE Vulkan::Vulkan)
endif()

# Compiler flags
if(MSVC)
    target_compile_options(RawrXD-ModelLoader PRIVATE /O2 /EHsc)
else()
    target_compile_options(RawrXD-ModelLoader PRIVATE -O2 -fexceptions)
endif()
target_compile_definitions(RawrXD-ModelLoader PRIVATE _CRT_SECURE_NO_WARNINGS _WINDOWS)

# Output directory
set_target_properties(RawrXD-ModelLoader PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin-${COMPILER_SUFFIX}"
)
endif()

if(FALSE) # Disabled until headers exist
# Benchmark target
add_executable(model_loader_bench
    src/bench_main.cpp
    src/gguf_loader.cpp
    src/vulkan_compute.cpp
    src/settings.cpp
    src/telemetry.cpp
)

target_link_libraries(model_loader_bench PRIVATE wbemuuid pdh)
if(Vulkan_FOUND)
    target_link_libraries(model_loader_bench PRIVATE Vulkan::Vulkan)
endif()

# Stress test executable for overclock governor development
add_executable(rawrxd_stress
    src/oc_stress.cpp
    src/telemetry.cpp
)
target_link_libraries(rawrxd_stress PRIVATE wbemuuid pdh)
set_target_properties(rawrxd_stress PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin-${COMPILER_SUFFIX}")
if(MSVC)
    target_compile_options(model_loader_bench PRIVATE /O2 /EHsc)
else()
    target_compile_options(model_loader_bench PRIVATE -O2 -fexceptions)
endif()
set_target_properties(model_loader_bench PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin-${COMPILER_SUFFIX}"
)
endif()

# Unit test targets (separate small executables) - DISABLED - files don't exist
# add_executable(rawrxd_test_baseline
#     tests/test_baseline_main.cpp
#     src/baseline_profile.cpp
#     src/settings.cpp
# )
# target_link_libraries(rawrxd_test_baseline PRIVATE wbemuuid pdh)
# set_target_properties(rawrxd_test_baseline PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# add_executable(rawrxd_test_pid
#     tests/test_pid_main.cpp
#     src/overclock_governor.cpp
#     src/overclock_vendor.cpp
#     src/settings.cpp
# )
# target_link_libraries(rawrxd_test_pid PRIVATE wbemuuid pdh)
# set_target_properties(rawrxd_test_pid PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

if(FALSE) # Disabled until headers exist
# Native Win32 IDE executable (no Qt dependencies)
add_executable(RawrXD-IDE WIN32
    src/ide_main.cpp
    src/ide_window.cpp
)

target_include_directories(RawrXD-IDE PRIVATE include)
target_link_libraries(RawrXD-IDE PRIVATE 
    comctl32 
    comdlg32 
    shell32
)

set_target_properties(RawrXD-IDE PROPERTIES 
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    WIN32_EXECUTABLE TRUE
)
endif()

if(FALSE) # Disabled until headers exist
# Simple C++ IDE with advanced features stubs
add_executable(RawrXD-SimpleIDE
    src/qtapp/main_simple.cpp
    src/qtapp/MainWindowSimple.cpp
    src/qtapp/MainWindowSimple.h
    src/editor_buffer.cpp
    src/syntax_engine.cpp
    src/overclock_vendor.cpp
    src/overclock_governor.cpp
    src/settings.cpp
    src/telemetry.cpp
    src/baseline_profile.cpp
    src/gguf_loader.cpp
    src/telemetry/ai_metrics.cpp
    src/session/ai_session.cpp
    src/backend/ollama_client.cpp
    src/backend/websocket_server.cpp
    src/backend/agentic_tools.cpp
    src/tools/file_ops.cpp
    src/tools/git_client.cpp
    src/context/indexer.cpp
    src/context/semantic_store.cpp
    src/ui/split_layout.cpp
    src/ui/chat_panel.cpp
)

target_include_directories(RawrXD-SimpleIDE PRIVATE include)
target_link_libraries(RawrXD-SimpleIDE PRIVATE 
    user32 
    gdi32 
    wbemuuid 
    pdh
    winhttp
    ws2_32
)

set_target_properties(RawrXD-SimpleIDE PROPERTIES 
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Enable standard exception semantics and optimization for SimpleIDE
if(MSVC)
    target_compile_options(RawrXD-SimpleIDE PRIVATE /EHsc /O2)
else()
    target_compile_options(RawrXD-SimpleIDE PRIVATE -fexceptions -O2)
endif()
target_compile_definitions(RawrXD-SimpleIDE PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

# Chromatic Wave Demo - Chameleon/Neon effects with D3D11
# Target: 540Hz @ 3840x2160 (4K UHD)
add_executable(RawrXD-Chromatic WIN32
    src/chromatic_main.cpp
    src/ui/chromatic_window.cpp
)

target_include_directories(RawrXD-Chromatic PRIVATE 
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(RawrXD-Chromatic PRIVATE 
    user32 
    gdi32 
    d3d11
    dxgi
    d3dcompiler
)

set_target_properties(RawrXD-Chromatic PROPERTIES 
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    WIN32_EXECUTABLE TRUE
)

if(MSVC)
    target_compile_options(RawrXD-Chromatic PRIVATE /EHsc /O2)
else()
    target_compile_options(RawrXD-Chromatic PRIVATE -fexceptions -O2)
endif()
target_compile_definitions(RawrXD-Chromatic PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)

# Agentic IDE - Advanced AI-powered IDE with terminal integration (Qt-based GUI app)
# Enabled with Qt6 6.7.2+ support
if(Qt6_FOUND AND Qt6_VERSION VERSION_GREATER_EQUAL 6.7.2)
    message(STATUS "Building RawrXD-AgenticIDE with Qt ${Qt6_VERSION}")
    
    # Check for required source files - v5.0 Clean Implementation
    set(AGENTICIDE_SOURCES
        src/qtapp/main_v5.cpp
        src/qtapp/MainWindow_v5.cpp
        src/qtapp/qt_masm_bridge.cpp
    )
    
    # Check if additional source files exist
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/agentic_ide.cpp")
        list(APPEND AGENTICIDE_SOURCES src/agentic_ide.cpp)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/scalar_server.cpp")
        list(APPEND AGENTICIDE_SOURCES src/scalar_server.cpp)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/agentic_chat.cpp")
        list(APPEND AGENTICIDE_SOURCES src/agentic_chat.cpp)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/chat_workspace.cpp")
        list(APPEND AGENTICIDE_SOURCES src/chat_workspace.cpp)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/multi_tab_editor.cpp")
        list(APPEND AGENTICIDE_SOURCES src/multi_tab_editor.cpp)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/terminal_pool.cpp")
        list(APPEND AGENTICIDE_SOURCES src/terminal_pool.cpp)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/agentic_engine.cpp")
        list(APPEND AGENTICIDE_SOURCES src/agentic_engine.cpp)
        list(APPEND AGENTICIDE_SOURCES src/zero_day_agentic_engine.cpp)
        if(EXISTS "${CMAKE_SOURCE_DIR}/include/zero_day_agentic_engine.hpp")
            list(APPEND AGENTICIDE_SOURCES include/zero_day_agentic_engine.hpp)
        endif()
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/chat_interface.cpp")
        list(APPEND AGENTICIDE_SOURCES src/chat_interface.cpp)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/agentic_copilot_bridge.cpp")
        list(APPEND AGENTICIDE_SOURCES src/agentic_copilot_bridge.cpp)
    endif()
    # Add agentic failure detector and puppeteer for hotpatching
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/agent/agentic_failure_detector.cpp")
        list(APPEND AGENTICIDE_SOURCES src/agent/agentic_failure_detector.cpp)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/agent/agentic_failure_detector.hpp")
        list(APPEND AGENTICIDE_SOURCES src/agent/agentic_failure_detector.hpp)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/agent/agentic_puppeteer.cpp")
        list(APPEND AGENTICIDE_SOURCES src/agent/agentic_puppeteer.cpp)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/agent/agentic_puppeteer.hpp")
        list(APPEND AGENTICIDE_SOURCES src/agent/agentic_puppeteer.hpp)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/qtapp/ai_code_assistant.cpp")
        list(APPEND AGENTICIDE_SOURCES src/qtapp/ai_code_assistant.cpp)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/qtapp/ai_code_assistant_panel.cpp")
        list(APPEND AGENTICIDE_SOURCES src/qtapp/ai_code_assistant_panel.cpp)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/qtapp/settings_dialog.cpp")
        list(APPEND AGENTICIDE_SOURCES src/qtapp/settings_dialog.cpp)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/qtapp/settings_manager.cpp")
        list(APPEND AGENTICIDE_SOURCES src/qtapp/settings_manager.cpp)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/gui/ModelConversionDialog.cpp")
        list(APPEND AGENTICIDE_SOURCES src/gui/ModelConversionDialog.cpp)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/qtapp/TerminalManager.cpp")
        list(APPEND AGENTICIDE_SOURCES src/qtapp/TerminalManager.cpp)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/utils/InferenceSettingsManager.cpp")
        list(APPEND AGENTICIDE_SOURCES src/utils/InferenceSettingsManager.cpp)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/ui/InferenceSettingsDialog.cpp")
        list(APPEND AGENTICIDE_SOURCES src/ui/InferenceSettingsDialog.cpp)
    endif()
    # if(EXISTS "${CMAKE_SOURCE_DIR}/src/qtapp/distributed_trainer.cpp")  # Temporarily disabled
    #     list(APPEND AGENTICIDE_SOURCES src/qtapp/distributed_trainer.cpp)
    # endif()
    # if(EXISTS "${CMAKE_SOURCE_DIR}/src/qtapp/interpretability_panel.cpp")  # Temporarily disabled
    #     list(APPEND AGENTICIDE_SOURCES src/qtapp/interpretability_panel.cpp)
    # endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/qtapp/tokenizer_selector.cpp")
        list(APPEND AGENTICIDE_SOURCES src/qtapp/tokenizer_selector.cpp)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/real_time_completion_engine.cpp")
        list(APPEND AGENTICIDE_SOURCES src/real_time_completion_engine.cpp)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/include/real_time_completion_engine.h")
        list(APPEND AGENTICIDE_SOURCES include/real_time_completion_engine.h)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/include/tokenizer_selector.h")
        list(APPEND AGENTICIDE_SOURCES include/tokenizer_selector.h)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/qtapp/ci_cd_settings.cpp")
        list(APPEND AGENTICIDE_SOURCES src/qtapp/ci_cd_settings.cpp)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/qtapp/ci_cd_settings.h")
        list(APPEND AGENTICIDE_SOURCES src/qtapp/ci_cd_settings.h)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/include/ci_cd_settings.h")
        list(APPEND AGENTICIDE_SOURCES include/ci_cd_settings.h)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/qtapp/checkpoint_manager.cpp")
        list(APPEND AGENTICIDE_SOURCES src/qtapp/checkpoint_manager.cpp)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/qtapp/checkpoint_manager.h")
        list(APPEND AGENTICIDE_SOURCES src/qtapp/checkpoint_manager.h)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/include/checkpoint_manager.h")
        list(APPEND AGENTICIDE_SOURCES include/checkpoint_manager.h)
    endif()

    if(EXISTS "${CMAKE_SOURCE_DIR}/src/qtapp/inflate_deflate_cpp.cpp")
        list(APPEND AGENTICIDE_SOURCES src/qtapp/inflate_deflate_cpp.cpp)
    endif()
    # Ensure MASM wrapper is linked so deflate_brutal_masm resolves
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/qtapp/brutal_gzip_wrapper.cpp")
        list(APPEND AGENTICIDE_SOURCES src/qtapp/brutal_gzip_wrapper.cpp)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/file_browser.cpp")
        list(APPEND AGENTICIDE_SOURCES src/file_browser.cpp)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/inference_engine.cpp")
        list(APPEND AGENTICIDE_SOURCES src/inference_engine.cpp)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/qtapp/inference_engine.cpp")
        list(APPEND AGENTICIDE_SOURCES src/qtapp/inference_engine.cpp)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/qtapp/sentencepiece_tokenizer.cpp")
        list(APPEND AGENTICIDE_SOURCES src/qtapp/sentencepiece_tokenizer.cpp)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/qtapp/vocabulary_loader.cpp")
        list(APPEND AGENTICIDE_SOURCES src/qtapp/vocabulary_loader.cpp)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/qtapp/bpe_tokenizer.cpp")
        list(APPEND AGENTICIDE_SOURCES src/qtapp/bpe_tokenizer.cpp)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/qtapp/transformer_inference.cpp")
        list(APPEND AGENTICIDE_SOURCES src/qtapp/transformer_inference.cpp)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/qtapp/gguf_loader.hpp")
        list(APPEND AGENTICIDE_SOURCES src/qtapp/gguf_loader.hpp)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/qtapp/gguf_loader.cpp")
        list(APPEND AGENTICIDE_SOURCES src/qtapp/gguf_loader.cpp)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/gguf_loader.cpp")
        list(APPEND AGENTICIDE_SOURCES src/gguf_loader.cpp)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/gguf_vocab_resolver.cpp")
        list(APPEND AGENTICIDE_SOURCES src/gguf_vocab_resolver.cpp)
    endif()
    
    # Model Loader with Brutal MASM Compression
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/qtapp/model_loader_with_compression.cpp")
        list(APPEND AGENTICIDE_SOURCES src/qtapp/model_loader_with_compression.cpp)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/qtapp/model_loader_with_compression.hpp")
        list(APPEND AGENTICIDE_SOURCES src/qtapp/model_loader_with_compression.hpp)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/qtapp/model_loader_widget.cpp")
        list(APPEND AGENTICIDE_SOURCES src/qtapp/model_loader_widget.cpp)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/qtapp/model_loader_widget.hpp")
        list(APPEND AGENTICIDE_SOURCES src/qtapp/model_loader_widget.hpp)
    endif()
    
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/vulkan_compute.cpp")
        list(APPEND AGENTICIDE_SOURCES src/vulkan_compute.cpp)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/transformer_block_scalar.cpp")
        list(APPEND AGENTICIDE_SOURCES src/transformer_block_scalar.cpp)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/settings.cpp")
        list(APPEND AGENTICIDE_SOURCES src/settings.cpp)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/planning_agent.cpp")
        list(APPEND AGENTICIDE_SOURCES src/planning_agent.cpp)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/plan_orchestrator.cpp")
        list(APPEND AGENTICIDE_SOURCES src/plan_orchestrator.cpp)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/lsp_client.cpp")
        list(APPEND AGENTICIDE_SOURCES src/lsp_client.cpp)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/agentic_text_edit.cpp")
        list(APPEND AGENTICIDE_SOURCES src/agentic_text_edit.cpp)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/ghost_text_renderer.cpp")
        list(APPEND AGENTICIDE_SOURCES src/ghost_text_renderer.cpp)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/universal_model_router.cpp")
        list(APPEND AGENTICIDE_SOURCES src/universal_model_router.cpp)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/cloud_api_client.cpp")
        list(APPEND AGENTICIDE_SOURCES src/cloud_api_client.cpp)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/config_manager.cpp")
        list(APPEND AGENTICIDE_SOURCES src/config_manager.cpp)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/feature_toggle.cpp")
        list(APPEND AGENTICIDE_SOURCES src/feature_toggle.cpp)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/logging/logging.cpp")
        list(APPEND AGENTICIDE_SOURCES src/logging/logging.cpp)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/cloud_api_client.h")
        list(APPEND AGENTICIDE_SOURCES src/cloud_api_client.h)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/include/config_manager.h")
        list(APPEND AGENTICIDE_SOURCES include/config_manager.h)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/include/feature_toggle.h")
        list(APPEND AGENTICIDE_SOURCES include/feature_toggle.h)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/include/logging/logging.h")
        list(APPEND AGENTICIDE_SOURCES include/logging/logging.h)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/tool_registry.cpp")
        list(APPEND AGENTICIDE_SOURCES src/tool_registry.cpp)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/qtapp/ai_completion_provider.cpp")
        list(APPEND AGENTICIDE_SOURCES src/qtapp/ai_completion_provider.cpp)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/qtapp/model_loader_thread.cpp")
        list(APPEND AGENTICIDE_SOURCES src/qtapp/model_loader_thread.cpp)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/telemetry.cpp")
        list(APPEND AGENTICIDE_SOURCES src/telemetry.cpp)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/telemetry_singleton.cpp")
        list(APPEND AGENTICIDE_SOURCES src/telemetry_singleton.cpp)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/todo_manager.cpp")
        list(APPEND AGENTICIDE_SOURCES src/todo_manager.cpp)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/todo_dock.cpp")
        list(APPEND AGENTICIDE_SOURCES src/todo_dock.cpp)
    endif()
    # NOTE: inference_engine_stub.cpp is excluded because src/qtapp/inference_engine.cpp is the primary implementation
    # if(EXISTS "${CMAKE_SOURCE_DIR}/src/inference_engine_stub.cpp")
    #     list(APPEND AGENTICIDE_SOURCES src/inference_engine_stub.cpp)
    # endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/agentic_executor.cpp")
        list(APPEND AGENTICIDE_SOURCES src/agentic_executor.cpp)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/autonomous_model_manager.cpp")
        list(APPEND AGENTICIDE_SOURCES src/autonomous_model_manager.cpp)
        list(APPEND AGENTICIDE_SOURCES src/autonomous_model_manager.h)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/autonomous_resource_manager.cpp")
        list(APPEND AGENTICIDE_SOURCES src/autonomous_resource_manager.cpp)
        list(APPEND AGENTICIDE_SOURCES include/autonomous_resource_manager.h)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/agentic_learning_system.cpp")
        list(APPEND AGENTICIDE_SOURCES src/agentic_learning_system.cpp)
        list(APPEND AGENTICIDE_SOURCES include/agentic_learning_system.h)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/compression_interface.cpp")
        list(APPEND AGENTICIDE_SOURCES src/compression_interface.cpp)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/model_trainer.cpp")
        list(APPEND AGENTICIDE_SOURCES src/model_trainer.cpp)
    endif()
    
    # Phase 1: ModelTrainer UI Components
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/training_dialog.cpp")
        list(APPEND AGENTICIDE_SOURCES src/training_dialog.cpp)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/training_progress_dock.cpp")
        list(APPEND AGENTICIDE_SOURCES src/training_progress_dock.cpp)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/model_registry.cpp")
        list(APPEND AGENTICIDE_SOURCES src/model_registry.cpp)
    endif()
    
    # Phase 2: Advanced Monitoring Components
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/profiler.cpp")
        list(APPEND AGENTICIDE_SOURCES src/profiler.cpp)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/observability_dashboard.cpp")
        list(APPEND AGENTICIDE_SOURCES src/observability_dashboard.cpp)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/hardware_backend_selector.cpp")
        list(APPEND AGENTICIDE_SOURCES src/hardware_backend_selector.cpp)
    endif()
    
    # Phase 2 Polish: UI Enhancement Components (Investor Demo Features)
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/ui/diff_dock.cpp")
        list(APPEND AGENTICIDE_SOURCES src/ui/diff_dock.cpp)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/ui/gpu_backend_selector.cpp")
        list(APPEND AGENTICIDE_SOURCES src/ui/gpu_backend_selector.cpp)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/ui/auto_model_downloader.cpp")
        list(APPEND AGENTICIDE_SOURCES src/ui/auto_model_downloader.cpp)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/ui/model_download_dialog_new.cpp")
        list(APPEND AGENTICIDE_SOURCES src/ui/model_download_dialog_new.cpp)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/ui/telemetry_optin_dialog.cpp")
        list(APPEND AGENTICIDE_SOURCES src/ui/telemetry_optin_dialog.cpp)
    endif()
    
    # Phase 3: Production Enhancements
    # Security, checkpoint, and CI/CD management components
    # TEMPORARILY DISABLED - not needed for v5.0 clean build
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/qtapp/security_manager.cpp")
        list(APPEND AGENTICIDE_SOURCES src/qtapp/security_manager.cpp)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/qtapp/checkpoint_manager.cpp")
        list(APPEND AGENTICIDE_SOURCES src/qtapp/checkpoint_manager.cpp)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/qtapp/ci_cd_settings.cpp")
        list(APPEND AGENTICIDE_SOURCES src/qtapp/ci_cd_settings.cpp)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/qtapp/tokenizer_selector.cpp")
        list(APPEND AGENTICIDE_SOURCES src/qtapp/tokenizer_selector.cpp)
    endif()
    
    # ============================================================
    # IDE WIDGETS - MASM Editor, Hotpatch Panel, Project Explorer, etc.
    # All IDE functions accessible via View menu
    # ============================================================
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/qtapp/widgets/masm_editor_widget.cpp")
        list(APPEND AGENTICIDE_SOURCES src/qtapp/widgets/masm_editor_widget.cpp)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/qtapp/widgets/masm_editor_widget.h")
        list(APPEND AGENTICIDE_SOURCES src/qtapp/widgets/masm_editor_widget.h)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/qtapp/widgets/hotpatch_panel.cpp")
        list(APPEND AGENTICIDE_SOURCES src/qtapp/widgets/hotpatch_panel.cpp)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/qtapp/widgets/hotpatch_panel.h")
        list(APPEND AGENTICIDE_SOURCES src/qtapp/widgets/hotpatch_panel.h)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/qtapp/widgets/project_explorer.cpp")
        list(APPEND AGENTICIDE_SOURCES src/qtapp/widgets/project_explorer.cpp)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/qtapp/widgets/project_explorer.h")
        list(APPEND AGENTICIDE_SOURCES src/qtapp/widgets/project_explorer.h)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/qtapp/widgets/find_widget.cpp")
        list(APPEND AGENTICIDE_SOURCES src/qtapp/widgets/find_widget.cpp)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/qtapp/widgets/find_widget.h")
        list(APPEND AGENTICIDE_SOURCES src/qtapp/widgets/find_widget.h)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/qtapp/widgets/multi_file_search.cpp")
        list(APPEND AGENTICIDE_SOURCES src/qtapp/widgets/multi_file_search.cpp)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/qtapp/widgets/multi_file_search.h")
        list(APPEND AGENTICIDE_SOURCES src/qtapp/widgets/multi_file_search.h)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/qtapp/widgets/breadcrumb_navigation.cpp")
        list(APPEND AGENTICIDE_SOURCES src/qtapp/widgets/breadcrumb_navigation.cpp)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/qtapp/widgets/breadcrumb_navigation.hpp")
        list(APPEND AGENTICIDE_SOURCES src/qtapp/widgets/breadcrumb_navigation.hpp)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/qtapp/widgets/layer_quant_widget.cpp")
        list(APPEND AGENTICIDE_SOURCES src/qtapp/widgets/layer_quant_widget.cpp)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/qtapp/widgets/layer_quant_widget.hpp")
        list(APPEND AGENTICIDE_SOURCES src/qtapp/widgets/layer_quant_widget.hpp)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/qtapp/widgets/OverlayWidget.cpp")
        list(APPEND AGENTICIDE_SOURCES src/qtapp/widgets/OverlayWidget.cpp)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/qtapp/widgets/OverlayWidget.hpp")
        list(APPEND AGENTICIDE_SOURCES src/qtapp/widgets/OverlayWidget.hpp)
    endif()
    
    # Interpretability Panel (Production)
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/qtapp/interpretability_panel_enhanced.cpp")
        list(APPEND AGENTICIDE_SOURCES src/qtapp/interpretability_panel_enhanced.cpp)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/qtapp/interpretability_panel_enhanced.hpp")
        list(APPEND AGENTICIDE_SOURCES src/qtapp/interpretability_panel_enhanced.hpp)
    endif()
    
    # ============================================================
    # THEME SYSTEM COMPONENTS (TEMPORARILY DISABLED - MOC issues)
    # ============================================================
    # Will be re-enabled after fixing theme header dependencies
    
    # ============================================================
    # AI Chat Panel and Utils (required for MainWindow chat features)
    # ============================================================
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/qtapp/ai_chat_panel.cpp")
        list(APPEND AGENTICIDE_SOURCES src/qtapp/ai_chat_panel.cpp)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/qtapp/ai_chat_panel.hpp")
        list(APPEND AGENTICIDE_SOURCES src/qtapp/ai_chat_panel.hpp)
    endif()
    # Agent chat breadcrumb (mode + model selector)
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/qtapp/agent_chat_breadcrumb.cpp")
        list(APPEND AGENTICIDE_SOURCES src/qtapp/agent_chat_breadcrumb.cpp)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/qtapp/agent_chat_breadcrumb.hpp")
        list(APPEND AGENTICIDE_SOURCES src/qtapp/agent_chat_breadcrumb.hpp)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/qtapp/utils/file_operations.cpp")
        list(APPEND AGENTICIDE_SOURCES src/qtapp/utils/file_operations.cpp)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/qtapp/utils/project_detector.cpp")
        list(APPEND AGENTICIDE_SOURCES src/qtapp/utils/project_detector.cpp)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/qtapp/utils/qt_file_writer.cpp")
        list(APPEND AGENTICIDE_SOURCES src/qtapp/utils/qt_file_writer.cpp)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/qtapp/utils/qt_directory_manager.cpp")
        list(APPEND AGENTICIDE_SOURCES src/qtapp/utils/qt_directory_manager.cpp)
    endif()
    
    # Add moc_includes.cpp to force AUTOMOC to process all Q_OBJECT headers
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/qtapp/moc_includes.cpp")
        list(APPEND AGENTICIDE_SOURCES src/qtapp/moc_includes.cpp)
    endif()
    # Explicit trigger: ensure AUTOMOC sees ZeroDayAgenticEngine Q_OBJECT header
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/qtapp/zero_day_agentic_engine_moc_trigger.cpp")
        list(APPEND AGENTICIDE_SOURCES src/qtapp/zero_day_agentic_engine_moc_trigger.cpp)
    endif()
    
    # Production API Server, Auth, Crypto, and Infrastructure Components
    # These files have internal guards for QT_HTTPSERVER_LIB
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/production_api_stub.cpp")
        list(APPEND AGENTICIDE_SOURCES src/production_api_stub.cpp)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/production_api_server.cpp")
        list(APPEND AGENTICIDE_SOURCES src/production_api_server.cpp)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/production_api_example.cpp")
        list(APPEND AGENTICIDE_SOURCES src/production_api_example.cpp)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/api/rest_api_server.cpp")
        list(APPEND AGENTICIDE_SOURCES src/api/rest_api_server.cpp)
    endif()
    
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/auth/oauth2_manager.cpp")
        list(APPEND AGENTICIDE_SOURCES src/auth/oauth2_manager.cpp)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/jwt_validator.cpp")
        list(APPEND AGENTICIDE_SOURCES src/jwt_validator.cpp)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/config_manager.cpp")
        list(APPEND AGENTICIDE_SOURCES src/config_manager.cpp)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/crypto/pqc_crypto.cpp")
        list(APPEND AGENTICIDE_SOURCES src/crypto/pqc_crypto.cpp)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/db/database_manager.cpp")
        list(APPEND AGENTICIDE_SOURCES src/db/database_manager.cpp)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/monitoring/metrics_emitter.cpp")
        list(APPEND AGENTICIDE_SOURCES src/monitoring/metrics_emitter.cpp)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/semantic-analysis/ast_analyzer.cpp")
        list(APPEND AGENTICIDE_SOURCES src/semantic-analysis/ast_analyzer.cpp)
    endif()
    # NOTE: agentic_textedit.cpp excluded - conflicts with src/agentic_text_edit.cpp (same class name)
    # if(EXISTS "${CMAKE_SOURCE_DIR}/src/qtapp/agentic_textedit.cpp")
    #     list(APPEND AGENTICIDE_SOURCES src/qtapp/agentic_textedit.cpp)
    # endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/qtapp/agentic_tools.cpp")
        list(APPEND AGENTICIDE_SOURCES src/qtapp/agentic_tools.cpp)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/qtapp/quant_impl.cpp")
        list(APPEND AGENTICIDE_SOURCES src/qtapp/quant_impl.cpp)
    endif()
    
    # ============================================================
    # Crypto Library (RSA/ECDSA for JWT validation)
    # ============================================================
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/crypto/rawrxd_crypto_base.cpp")
        list(APPEND AGENTICIDE_SOURCES src/crypto/rawrxd_crypto_base.cpp)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/crypto/rawrxd_crypto_bigint.cpp")
        list(APPEND AGENTICIDE_SOURCES src/crypto/rawrxd_crypto_bigint.cpp)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/crypto/rawrxd_crypto_rsa_ecc.cpp")
        list(APPEND AGENTICIDE_SOURCES src/crypto/rawrxd_crypto_rsa_ecc.cpp)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/crypto/rawrxd_crypto_aes.cpp")
        list(APPEND AGENTICIDE_SOURCES src/crypto/rawrxd_crypto_aes.cpp)
    endif()
    
    # Production API and Infrastructure Headers
    # Only add if Qt HttpServer is available
    if(HAVE_QT_HTTPSERVER)
        if(EXISTS "${CMAKE_SOURCE_DIR}/include/production_api_server.h")
            list(APPEND AGENTICIDE_SOURCES include/production_api_server.h)
        endif()
        if(EXISTS "${CMAKE_SOURCE_DIR}/include/production_api_configuration.h")
            list(APPEND AGENTICIDE_SOURCES include/production_api_configuration.h)
        endif()
        if(EXISTS "${CMAKE_SOURCE_DIR}/include/rest_api_server.h")
            list(APPEND AGENTICIDE_SOURCES include/rest_api_server.h)
        endif()
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/include/oauth2_manager.h")
        list(APPEND AGENTICIDE_SOURCES include/oauth2_manager.h)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/include/jwt_validator.h")
        list(APPEND AGENTICIDE_SOURCES include/jwt_validator.h)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/include/config_manager.h")
        list(APPEND AGENTICIDE_SOURCES include/config_manager.h)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/include/pqc_crypto.h")
        list(APPEND AGENTICIDE_SOURCES include/pqc_crypto.h)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/include/database_manager.h")
        list(APPEND AGENTICIDE_SOURCES include/database_manager.h)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/include/metrics_emitter.h")
        list(APPEND AGENTICIDE_SOURCES include/metrics_emitter.h)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/include/ast_analyzer.h")
        list(APPEND AGENTICIDE_SOURCES include/ast_analyzer.h)
    endif()
    
    # Enterprise Telemetry Window - must be included for MOC processing
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/qtapp/TelemetryWindow.h")
        list(APPEND AGENTICIDE_SOURCES src/qtapp/TelemetryWindow.h)
    endif()
    
    # Phase 1 & 2: Response Parsing, Model Testing, and Library Integration
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/response_parser.cpp")
        list(APPEND AGENTICIDE_SOURCES src/response_parser.cpp)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/model_tester.cpp")
        list(APPEND AGENTICIDE_SOURCES src/model_tester.cpp)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/library_integration.cpp")
        list(APPEND AGENTICIDE_SOURCES src/library_integration.cpp)
    endif()
    
    # Phase 3: Real AI Implementation with Multi-Backend LLM Support
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/ai_implementation.cpp")
        list(APPEND AGENTICIDE_SOURCES src/ai_implementation.cpp)
    endif()
    
    # Phase 3: Production Streaming Support
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/streaming_engine.cpp")
        list(APPEND AGENTICIDE_SOURCES src/streaming_engine.cpp)
    endif()
    
    # Enterprise Autonomous Agent System (Zero-Day Agentic Engine)
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/zero_day_agentic_engine.cpp")
        list(APPEND AGENTICIDE_SOURCES src/zero_day_agentic_engine.cpp)
    endif()
    
    # Add zero_day header to sources for tracking (MOC handled globally below)
    if(EXISTS "${CMAKE_SOURCE_DIR}/include/zero_day_agentic_engine.hpp")
        list(APPEND AGENTICIDE_SOURCES include/zero_day_agentic_engine.hpp)
    endif()

    # Enterprise Streaming GGUF System
    list(APPEND AGENTICIDE_SOURCES 
        src/Memory/streaming_gguf_memory_manager.hpp
        src/Memory/streaming_gguf_memory_manager.cpp
        src/Memory/lazy_model_loader.hpp
        src/Memory/lazy_model_loader.cpp
        src/Memory/large_model_optimizer.hpp
        src/Memory/large_model_optimizer.cpp
        src/Memory/enterprise_streaming_controller.hpp
        src/Memory/enterprise_streaming_controller.cpp
        src/monitoring/enterprise_metrics_collector.hpp
        src/monitoring/enterprise_metrics_collector.cpp
        src/Memory/fault_tolerance_manager.hpp
        src/Memory/fault_tolerance_manager.cpp
    )
    
    # Theme System Sources - MUST be before add_executable
    list(APPEND AGENTICIDE_SOURCES src/qtapp/ThemeManager.cpp)
    list(APPEND AGENTICIDE_SOURCES src/qtapp/ThemeConfigurationPanel.cpp)
    list(APPEND AGENTICIDE_SOURCES src/qtapp/TransparencyControlPanel.cpp)
    list(APPEND AGENTICIDE_SOURCES src/qtapp/ThemedCodeEditor.cpp)
    
    add_executable(RawrXD-AgenticIDE WIN32 ${AGENTICIDE_SOURCES})
    
    # Enable Qt MOC for QObject-derived classes - must be set BEFORE any other properties
    set_target_properties(RawrXD-AgenticIDE PROPERTIES 
        AUTOMOC ON
        AUTOMOC_MOC_OPTIONS "-I${CMAKE_SOURCE_DIR}/include;-I${CMAKE_CURRENT_SOURCE_DIR}/src/qtapp;-I${CMAKE_CURRENT_SOURCE_DIR}/src/ui;-I${CMAKE_CURRENT_SOURCE_DIR}/src/utils;-I${CMAKE_CURRENT_SOURCE_DIR}/src/gui"
    )

    # MOC wrapping for checkpoint_manager, ci_cd_settings, tokenizer_selector is handled
    # by RawrXD-QtShell target via qt_wrap_cpp to avoid duplicate generation

    # Explicitly set MOC include paths
    target_include_directories(RawrXD-AgenticIDE BEFORE PRIVATE 
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/qtapp
        ${CMAKE_SOURCE_DIR}/src/agent
        ${CMAKE_SOURCE_DIR}/src/ui
        ${CMAKE_SOURCE_DIR}/src/utils
        ${CMAKE_SOURCE_DIR}/src/gui
        ${CMAKE_SOURCE_DIR}/3rdparty/ggml/include
        ${CMAKE_SOURCE_DIR}/3rdparty/ggml/src
    )
    target_link_libraries(RawrXD-AgenticIDE PRIVATE 
        user32 
        gdi32 
        wbemuuid 
        pdh
        winhttp
        ws2_32
        comctl32
        d3d11
        dxgi
        d3dcompiler
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Network
        Qt6::Sql
        brutal_gzip
        quant_utils
        ggml_interface
    )

    # Enable Brutal compression test harness when requested
    if(TEST_BRUTAL)
        target_compile_definitions(RawrXD-AgenticIDE PRIVATE TEST_BRUTAL=1)
        message(STATUS "TEST_BRUTAL enabled: ModelLoaderWidget will run codec round-trip test")
    endif()

    if(CURL_FOUND)
        target_link_libraries(RawrXD-AgenticIDE PRIVATE CURL::libcurl)
        target_compile_definitions(RawrXD-AgenticIDE PRIVATE HAVE_CURL=1)
    else()
        target_compile_definitions(RawrXD-AgenticIDE PRIVATE HAVE_CURL=0)
    endif()

    if(ZSTD_FOUND)
        target_link_libraries(RawrXD-AgenticIDE PRIVATE zstd::libzstd_shared)
        target_compile_definitions(RawrXD-AgenticIDE PRIVATE HAVE_ZSTD=1)
    else()
        target_compile_definitions(RawrXD-AgenticIDE PRIVATE HAVE_ZSTD=0)
    endif()
    
    # Add Vulkan linkage if available
    if(Vulkan_FOUND)
        target_link_libraries(RawrXD-AgenticIDE PRIVATE Vulkan::Vulkan)
        target_compile_definitions(RawrXD-AgenticIDE PRIVATE HAVE_VULKAN=1)
    endif()

    set_target_properties(RawrXD-AgenticIDE PROPERTIES 
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        WIN32_EXECUTABLE TRUE
    )

    if(MSVC)
        target_compile_options(RawrXD-AgenticIDE PRIVATE /EHsc /O2)
    else()
        target_compile_options(RawrXD-AgenticIDE PRIVATE -fexceptions -O2)
    endif()
    target_compile_definitions(RawrXD-AgenticIDE PRIVATE _CRT_SECURE_NO_WARNINGS)
    
    # Automatic DLL deployment for RawrXD-AgenticIDE
    deploy_runtime_dependencies(RawrXD-AgenticIDE)
    
    # Include smoke tests
    if(EXISTS "${CMAKE_SOURCE_DIR}/smoke_tests.cmake")
        include(${CMAKE_SOURCE_DIR}/smoke_tests.cmake)
    endif()
    
    message(STATUS "RawrXD-AgenticIDE: Advanced AI-powered IDE enabled with Qt ${Qt6_VERSION}")
    message(STATUS "AgenticIDE sources: ${AGENTICIDE_SOURCES}")
    
else()
    message(STATUS "Qt6 not found or version < 6.7.2; skipping RawrXD-AgenticIDE target")
endif()

message(STATUS "RawrXD-ModelLoader configured successfully with Clang C++20")

# ============================================================
# SHARED MOC GENERATION - Used by both RawrXD-AgenticIDE and test_ide_main
# ============================================================
if(EXISTS "${CMAKE_SOURCE_DIR}/include/zero_day_agentic_engine.hpp")
    qt_wrap_cpp(ZERO_DAY_MOC_GENERATED "${CMAKE_SOURCE_DIR}/include/zero_day_agentic_engine.hpp")
    message(STATUS "Shared MOC generated for ZeroDayAgenticEngine")
    # Add to RawrXD-AgenticIDE if the target was created
    if(TARGET RawrXD-AgenticIDE)
        target_sources(RawrXD-AgenticIDE PRIVATE ${ZERO_DAY_MOC_GENERATED})
    endif()
endif()

# Stub test to verify WinMain can be reached at all
add_executable(stub_test WIN32 src/stub_main.cpp)

# Test IDE main - test if Qt can initialize
add_executable(test_ide_main WIN32 
    src/test_ide_main.cpp 
    src/agentic_ide.cpp
    src/telemetry.cpp
    src/settings.cpp
    src/multi_tab_editor.cpp
    src/chat_interface.cpp
    src/agentic_engine.cpp
    src/terminal_pool.cpp
    src/plan_orchestrator.cpp
    src/lsp_client.cpp
    src/universal_model_router.cpp
    src/cloud_api_client.cpp
    src/model_interface.cpp
    src/qtapp/inference_engine.cpp
    src/qtapp/bpe_tokenizer.cpp
    src/qtapp/sentencepiece_tokenizer.cpp
    src/qtapp/vocabulary_loader.cpp
    src/qtapp/transformer_inference.cpp
    src/qtapp/gguf_loader.cpp
    src/gguf_loader.cpp
    src/gguf_vocab_resolver.cpp
    src/qtapp/agent_mode_handler.cpp
    src/qtapp/agentic_tools.cpp
    src/qtapp/agentic_textedit.cpp
    src/qtapp/quant_impl.cpp
    src/zero_day_agentic_engine.cpp
    src/tool_registry.cpp
)

# Reuse the shared MOC file generated globally for zero_day_agentic_engine
target_sources(test_ide_main PRIVATE ${ZERO_DAY_MOC_GENERATED})

set_target_properties(test_ide_main PROPERTIES AUTOMOC ON)
target_link_libraries(test_ide_main PRIVATE 
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network Qt6::Sql Qt6::Concurrent Qt6::Test
    user32 gdi32 wbemuuid pdh winhttp ws2_32 comctl32 d3d11 dxgi d3dcompiler
    RawrXDOrchestration RawrXDGit RawrXDTerminal brutal_gzip
)

# Link ggml if available
if(TARGET ggml)
    target_link_libraries(test_ide_main PRIVATE ggml)
    target_compile_definitions(test_ide_main PRIVATE HAVE_GGML=1)
endif()

target_include_directories(test_ide_main PRIVATE 
    ${CMAKE_SOURCE_DIR}/include 
    ${CMAKE_SOURCE_DIR}/src 
    ${CMAKE_SOURCE_DIR}/src/qtapp
    ${CMAKE_SOURCE_DIR}/src/orchestration
    ${CMAKE_SOURCE_DIR}/src/git
    ${CMAKE_SOURCE_DIR}/src/terminal
    ${CMAKE_SOURCE_DIR}/src/agent
    ${CMAKE_SOURCE_DIR}/3rdparty/ggml/include
    ${CMAKE_SOURCE_DIR}/3rdparty/ggml/src
)

# Copy Qt DLLs for test_ide_main
add_custom_command(TARGET test_ide_main POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:Qt6::Core>"
        "$<TARGET_FILE:Qt6::Gui>"
        "$<TARGET_FILE:Qt6::Widgets>"
        "$<TARGET_FILE_DIR:test_ide_main>"
    COMMENT "Copying Qt DLLs for test_ide_main"
)

# Copy platform plugins for test_ide_main
add_custom_command(TARGET test_ide_main POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:test_ide_main>/platforms"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${Qt6_DIR}/../../../plugins/platforms/qwindows.dll"
        "$<TARGET_FILE_DIR:test_ide_main>/platforms/"
    COMMENT "Copying Qt platform plugins for test_ide_main"
)

# Test QMainWindow - simplest Qt GUI application
add_executable(test_qmainwindow WIN32 src/test_qmainwindow.cpp)
set_target_properties(test_qmainwindow PROPERTIES AUTOMOC ON)
target_link_libraries(test_qmainwindow PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets)
target_include_directories(test_qmainwindow PRIVATE ${CMAKE_SOURCE_DIR}/include)

# Copy Qt DLLs for test_qmainwindow
add_custom_command(TARGET test_qmainwindow POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:Qt6::Core>"
        "$<TARGET_FILE:Qt6::Gui>"
        "$<TARGET_FILE:Qt6::Widgets>"
        "$<TARGET_FILE_DIR:test_qmainwindow>"
    COMMENT "Copying Qt DLLs for test_qmainwindow"
)

# Copy platform plugins for test_qmainwindow
add_custom_command(TARGET test_qmainwindow POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:test_qmainwindow>/platforms"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${Qt6_DIR}/../../../plugins/platforms/qwindows.dll"
        "$<TARGET_FILE_DIR:test_qmainwindow>/platforms/"
    COMMENT "Copying Qt platform plugins for test_qmainwindow"
)

# Test if agentic_ide.h header itself causes a crash
add_executable(test_header src/agentic_ide_test.cpp)
set_target_properties(test_header PROPERTIES AUTOMOC ON)
target_link_libraries(test_header PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets)
target_include_directories(test_header PRIVATE ${CMAKE_SOURCE_DIR}/include)

if(FALSE) # Disabled until headers exist
# IDE Test Runner - Comprehensive function testing
add_executable(RawrXD-TestRunner
    src/win32app/test_runner.cpp
    src/win32app/Win32IDE.cpp
    src/win32app/Win32IDE_Commands.cpp
    src/win32app/Win32IDE_Debugger.cpp
    src/win32app/Win32IDE_FileOps.cpp
    src/win32app/Win32IDE_PowerShell.cpp
    src/win32app/Win32IDE_PowerShellPanel.cpp
    src/win32app/Win32IDE_Sidebar.cpp
    src/win32app/Win32IDE_VSCodeUI.cpp
    src/win32app/Win32IDE_AgenticBridge.cpp
    src/win32app/Win32IDE_AgentCommands.cpp
    src/win32app/Win32TerminalManager.cpp
    src/win32app/TransparentRenderer.cpp
    src/win32app/VulkanRenderer.cpp
    src/gguf_loader.cpp
    src/streaming_gguf_loader.cpp
)

target_include_directories(RawrXD-TestRunner PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/win32app
)

target_link_libraries(RawrXD-TestRunner PRIVATE
    user32 gdi32 comctl32 comdlg32 shell32 shlwapi
    d3d11 dxgi d3dcompiler
    d2d1 dwrite dcomp dwmapi
    winhttp
)

set_target_properties(RawrXD-TestRunner PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    WIN32_EXECUTABLE TRUE
)

target_compile_options(RawrXD-TestRunner PRIVATE -O2)
target_compile_definitions(RawrXD-TestRunner PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
endif()

# Q8_0 AVX2 end-to-end bench
if(EXISTS "${CMAKE_SOURCE_DIR}/tests/bench_q8_0_end2end.cpp")
    add_executable(bench_q8_0_end2end
        tests/bench_q8_0_end2end.cpp
        kernels/q8_0_avx2.cc
        kernels/matmul_kernel_avx2.cc
    )
    target_include_directories(bench_q8_0_end2end PRIVATE ${CMAKE_SOURCE_DIR})
    target_compile_options(bench_q8_0_end2end PRIVATE /arch:AVX2 /O2)
    target_compile_definitions(bench_q8_0_end2end PRIVATE __AVX2__=1)
    set_target_properties(bench_q8_0_end2end PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
    )
endif()

# Flash-Attention AVX2 bench
if(EXISTS "${CMAKE_SOURCE_DIR}/tests/bench_flash_attn.cpp")
    add_executable(bench_flash_attn
        tests/bench_flash_attn.cpp
        kernels/flash_attn_avx2.cc
        kernels/matmul_kernel_avx2.cc
    )
    target_include_directories(bench_flash_attn PRIVATE ${CMAKE_SOURCE_DIR})
    target_compile_options(bench_flash_attn PRIVATE /arch:AVX2 /O2)
    target_compile_definitions(bench_flash_attn PRIVATE __AVX2__=1)
    set_target_properties(bench_flash_attn PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
    )
endif()
# Flash-Attention All-Quant bench
if(EXISTS "${CMAKE_SOURCE_DIR}/tests/bench_flash_all_quant.cpp")
    add_executable(bench_flash_all_quant
        tests/bench_flash_all_quant.cpp
        kernels/flash_attn_avx2.cc
        kernels/matmul_kernel_avx2.cc
    )
    target_include_directories(bench_flash_all_quant PRIVATE ${CMAKE_SOURCE_DIR})
    target_compile_options(bench_flash_all_quant PRIVATE /arch:AVX2 /O2)
    target_compile_definitions(bench_flash_all_quant PRIVATE __AVX2__=1)
    set_target_properties(bench_flash_all_quant PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
    )
endif()

# Flash-Attention ASM Puppeteer (Phase 4 Final Form)
if(EXISTS "${CMAKE_SOURCE_DIR}/tests/bench_flash_asm_puppeteer.cpp")
    if(MSVC)
        # Assemble NASM file via custom command to avoid MSBuild flags leakage
        set(FLASH_ASM_SRC ${CMAKE_SOURCE_DIR}/kernels/flash_attn_asm_avx2.asm)
        set(FLASH_ASM_OBJ ${CMAKE_CURRENT_BINARY_DIR}/flash_attn_asm_avx2.obj)
        add_custom_command(
            OUTPUT ${FLASH_ASM_OBJ}
            COMMAND nasm -f win64 -o ${FLASH_ASM_OBJ} ${FLASH_ASM_SRC}
            DEPENDS ${FLASH_ASM_SRC}
            COMMENT "Assembling flash_attn_asm_avx2.asm with nasm"
        )
        add_custom_target(flash_attn_asm_obj DEPENDS ${FLASH_ASM_OBJ})
        
        add_executable(bench_flash_asm_puppeteer
            tests/bench_flash_asm_puppeteer.cpp
            kernels/flash_attn_avx2.cc
            kernels/matmul_kernel_avx2.cc
        )
        add_dependencies(bench_flash_asm_puppeteer flash_attn_asm_obj)
        target_sources(bench_flash_asm_puppeteer PRIVATE ${FLASH_ASM_OBJ})
        target_include_directories(bench_flash_asm_puppeteer PRIVATE ${CMAKE_SOURCE_DIR})
        target_compile_options(bench_flash_asm_puppeteer PRIVATE /arch:AVX2 /O2)
        target_compile_definitions(bench_flash_asm_puppeteer PRIVATE __AVX2__=1)
        set_target_properties(bench_flash_asm_puppeteer PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
        )
    endif()
endif()

# MASM God-Mode deflate benchmark
if(MSVC)
    enable_language(ASM_MASM)
    
    if(EXISTS "${CMAKE_SOURCE_DIR}/tests/bench_deflate_masm.cpp")
        # Disabled: deflate_godmode_masm.asm has broken syntax - bench only, not required for IDE
        # add_executable(bench_deflate_godmode tests/bench_deflate_masm.cpp kernels/deflate_godmode_masm.asm)
        # target_compile_definitions(bench_deflate_godmode PRIVATE NO_ZLIB_REF=1 DEFLATE_GODMODE=1)
        # set_target_properties(bench_deflate_godmode PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    endif()

    # Brutal MASM benchmark
    if(EXISTS "${CMAKE_SOURCE_DIR}/tests/bench_deflate_brutal.cpp")
        add_executable(bench_deflate_brutal tests/bench_deflate_brutal.cpp kernels/deflate_brutal_masm.asm)
        set_target_properties(bench_deflate_brutal PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    endif()

endif()

# ARM-NEON benchmark (cross-compile or native ARM64)
if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|ARM64")
    enable_language(ASM)
    # Existing generic NEON bench (if present)
    if(EXISTS "${CMAKE_SOURCE_DIR}/tests/bench_deflate_neon.cpp" AND EXISTS "${CMAKE_SOURCE_DIR}/kernels/deflate_neon.S")
        add_executable(bench_deflate_neon tests/bench_deflate_neon.cpp kernels/deflate_neon.S)
        set_target_properties(bench_deflate_neon PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    endif()

    # Brutal stored-block ARM64 bench
    if(EXISTS "${CMAKE_SOURCE_DIR}/tests/bench_deflate_brutal_arm64.cpp" AND EXISTS "${CMAKE_SOURCE_DIR}/kernels/deflate_brutal_neon.S")
        add_executable(bench_deflate_brutal_arm64
            tests/bench_deflate_brutal_arm64.cpp
            kernels/deflate_brutal_neon.S
        )
        set_target_properties(bench_deflate_brutal_arm64 PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    endif()
endif()

# Add benchmark for 50MB payload
if(EXISTS "${CMAKE_SOURCE_DIR}/tests/bench_deflate_50mb.cpp")
    add_executable(bench_deflate_50mb tests/bench_deflate_50mb.cpp)
    target_link_libraries(bench_deflate_50mb PRIVATE brutal_gzip)
    set_target_properties(bench_deflate_50mb PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/tests/quant_engine_smoke.cpp")
    add_executable(quant_engine_test
        tests/quant_engine_smoke.cpp
    )
    target_link_libraries(quant_engine_test PRIVATE Qt6::Core quant_utils)
    set_target_properties(quant_engine_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
endif()





 
 # Simple scalar quantization smoke test (Q5/Q6 bit-packers)
 add_executable(quant_scalar_smoke tests/quant_scalar_smoke.cpp)
 set_target_properties(quant_scalar_smoke PROPERTIES
     RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
    AUTOMOC OFF
 )

# Comprehensive quantization correctness tests (pack/unpack, error bounds)
add_executable(quant_correctness_tests tests/quant_correctness_tests.cpp)
target_link_libraries(quant_correctness_tests PRIVATE Qt6::Core quant_utils)
set_target_properties(quant_correctness_tests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
    AUTOMOC OFF
)

# Agent Coordinator comprehensive tests (scheduling, cancellation, failure paths)
add_executable(test_agent_coordinator tests/test_agent_coordinator.cpp)
target_link_libraries(test_agent_coordinator PRIVATE 
    Qt6::Core 
    Qt6::Test 
    RawrXDOrchestration
)
target_include_directories(test_agent_coordinator PRIVATE 
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/orchestration
)
set_target_properties(test_agent_coordinator PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
    AUTOMOC ON
)

# Agent Coordinator integration tests (real GGUF model loading + inference)
add_executable(test_agent_coordinator_integration
    tests/test_agent_coordinator_integration.cpp
    src/model_loader/model_loader.cpp
    src/inference_engine_stub.cpp
    src/qtapp/gguf_server.cpp
    src/gguf_loader.cpp
    src/qtapp/inflate_deflate_cpp.cpp
)
target_link_libraries(test_agent_coordinator_integration PRIVATE
    Qt6::Core
    Qt6::Test
    Qt6::Network
    RawrXDOrchestration
    brutal_gzip
)
target_include_directories(test_agent_coordinator_integration PRIVATE 
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/orchestration
    ${CMAKE_SOURCE_DIR}/src/model_loader
    ${CMAKE_SOURCE_DIR}/src/qtapp
    ${CMAKE_SOURCE_DIR}/include
)
set_target_properties(test_agent_coordinator_integration PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
    AUTOMOC ON
)

# KV Cache Infrastructure Test
if(ENABLE_VULKAN)
add_executable(test_kv_cache 
    src/test_kv_cache.cpp
    src/vulkan_compute.cpp
)
target_link_libraries(test_kv_cache PRIVATE Vulkan::Vulkan)
target_include_directories(test_kv_cache PRIVATE 
    ${CMAKE_SOURCE_DIR}/include
    ${Vulkan_INCLUDE_DIRS}
)
set_target_properties(test_kv_cache PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
)
endif()

# GGUF Loader Improvements Test
add_executable(test_gguf_loader
    tests/test_gguf_loader.cpp
    src/gguf_loader.cpp
    src/gguf_vocab_resolver.cpp
)
target_link_libraries(test_gguf_loader PRIVATE 
    Qt6::Core
    brutal_gzip  # Supplies codec::deflate, codec::inflate, deflate_brutal_masm
)
target_include_directories(test_gguf_loader PRIVATE 
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)
set_target_properties(test_gguf_loader PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
    AUTOMOC ON
)

# GGUF Loader Simple Test
add_executable(test_gguf_loader_simple
    tests/test_gguf_loader_simple.cpp
    src/gguf_loader.cpp
    src/gguf_vocab_resolver.cpp
)
target_link_libraries(test_gguf_loader_simple PRIVATE 
    Qt6::Core
    brutal_gzip  # Supplies codec::deflate, codec::inflate, deflate_brutal_masm
)
target_include_directories(test_gguf_loader_simple PRIVATE 
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)
set_target_properties(test_gguf_loader_simple PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
    AUTOMOC ON
)

# Brutal Gzip Library auto-selects MASM on x64 and NEON on ARM64
add_library(brutal_gzip STATIC)
target_include_directories(brutal_gzip PUBLIC include)
# Architecture-specific kernels get added below (MASM on x64, NEON on ARM64).

if(MSVC AND CMAKE_SYSTEM_PROCESSOR MATCHES "AMD64")
    enable_language(ASM_MASM)
    # Use inflate_deflate_asm.asm which exports AsmDeflate and AsmInflate
    set(BRUTAL_MASM_SRC "${CMAKE_SOURCE_DIR}/src/qtapp/inflate_deflate_asm.asm")
    if(EXISTS "${BRUTAL_MASM_SRC}")
        # Ensure CMake treats this file as MASM source explicitly
        set_source_files_properties("${BRUTAL_MASM_SRC}" PROPERTIES LANGUAGE ASM_MASM)
        target_sources(brutal_gzip PRIVATE "${BRUTAL_MASM_SRC}")
        # Propagate definition so headers select MASM path in consumers
        target_compile_definitions(brutal_gzip PUBLIC HAS_BRUTAL_GZIP_MASM=1)
        message(STATUS "BrutalGzip: Including MASM source ${BRUTAL_MASM_SRC}")
    else()
        message(FATAL_ERROR "BrutalGzip: MASM source not found at ${BRUTAL_MASM_SRC}")
    endif()
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|ARM64")
    enable_language(ASM)
    target_sources(brutal_gzip PRIVATE kernels/deflate_neon.S)
    target_compile_definitions(brutal_gzip PUBLIC HAS_BRUTAL_GZIP_NEON=1)
endif()

# brutal-gzip implementation file (tiny C++ wrapper)
target_sources(brutal_gzip PRIVATE src/qtapp/inflate_deflate_cpp.cpp)

# Production-ready hotpatch libraries (using only existing files)
# Memory hotpatch - header only for now until full implementation
# add_library(memory_hotpatch STATIC src/qtapp/model_memory_hotpatch.cpp)
# Compiled into main target for now

option(BUILD_SHARED_LIBS           "ggml: build shared libraries" ${BUILD_SHARED_LIBS_DEFAULT})
option(GGML_BACKEND_DL             "ggml: build backends as dynamic libraries (requires BUILD_SHARED_LIBS)" OFF)
set(GGML_BACKEND_DIR "" CACHE PATH "ggml: directory to load dynamic backends from (requires GGML_BACKEND_DL")

#
# option list
#

# TODO: mark all options as advanced when not GGML_STANDALONE

if (APPLE)
    set(GGML_METAL_DEFAULT ON)
    set(GGML_BLAS_DEFAULT ON)
    set(GGML_BLAS_VENDOR_DEFAULT "Apple")
else()
    set(GGML_METAL_DEFAULT OFF)
    set(GGML_BLAS_DEFAULT OFF)
    set(GGML_BLAS_VENDOR_DEFAULT "Generic")
endif()

if (CMAKE_CROSSCOMPILING OR DEFINED ENV{SOURCE_DATE_EPOCH})
    message(STATUS "Setting GGML_NATIVE_DEFAULT to OFF")
    set(GGML_NATIVE_DEFAULT OFF)
else()
    set(GGML_NATIVE_DEFAULT ON)
endif()

# defaults
if (NOT GGML_LLAMAFILE_DEFAULT)
    set(GGML_LLAMAFILE_DEFAULT OFF)
endif()

if (NOT GGML_CUDA_GRAPHS_DEFAULT)
    set(GGML_CUDA_GRAPHS_DEFAULT OFF)
endif()

# general
option(GGML_STATIC "ggml: static link libraries"                     OFF)
option(GGML_NATIVE "ggml: optimize the build for the current system" ${GGML_NATIVE_DEFAULT})
option(GGML_LTO    "ggml: enable link time optimization"             OFF)
option(GGML_CCACHE "ggml: use ccache if available"                   ON)

# debug
option(GGML_ALL_WARNINGS           "ggml: enable all compiler warnings"                   ON)
option(GGML_ALL_WARNINGS_3RD_PARTY "ggml: enable all compiler warnings in 3rd party libs" OFF)
option(GGML_GPROF                  "ggml: enable gprof"                                   OFF)

# build
option(GGML_FATAL_WARNINGS    "ggml: enable -Werror flag"    OFF)

# sanitizers
option(GGML_SANITIZE_THREAD    "ggml: enable thread sanitizer"    OFF)
option(GGML_SANITIZE_ADDRESS   "ggml: enable address sanitizer"   OFF)
option(GGML_SANITIZE_UNDEFINED "ggml: enable undefined sanitizer" OFF)

# instruction set specific
if (GGML_NATIVE OR NOT GGML_NATIVE_DEFAULT)
    set(INS_ENB OFF)
else()
    set(INS_ENB ON)
endif()

message(DEBUG "GGML_NATIVE         : ${GGML_NATIVE}")
message(DEBUG "GGML_NATIVE_DEFAULT : ${GGML_NATIVE_DEFAULT}")
message(DEBUG "INS_ENB             : ${INS_ENB}")

option(GGML_CPU_HBM          "ggml: use memkind for CPU HBM" OFF)
option(GGML_CPU_REPACK       "ggml: use runtime weight conversion of Q4_0 to Q4_X_X" ON)
option(GGML_CPU_KLEIDIAI     "ggml: use KleidiAI optimized kernels if applicable" OFF)
option(GGML_SSE42            "ggml: enable SSE 4.2"          ${INS_ENB})
option(GGML_AVX              "ggml: enable AVX"              ${INS_ENB})
option(GGML_AVX_VNNI         "ggml: enable AVX-VNNI"         OFF)
option(GGML_AVX2             "ggml: enable AVX2"             ${INS_ENB})
option(GGML_BMI2             "ggml: enable BMI2"             ${INS_ENB})
option(GGML_AVX512           "ggml: enable AVX512F"          OFF)
option(GGML_AVX512_VBMI      "ggml: enable AVX512-VBMI"      OFF)
option(GGML_AVX512_VNNI      "ggml: enable AVX512-VNNI"      OFF)
option(GGML_AVX512_BF16      "ggml: enable AVX512-BF16"      OFF)
if (NOT MSVC)
    # in MSVC F16C and FMA is implied with AVX2/AVX512
    option(GGML_FMA          "ggml: enable FMA"              ${INS_ENB})
    option(GGML_F16C         "ggml: enable F16C"             ${INS_ENB})
    # MSVC does not seem to support AMX
    option(GGML_AMX_TILE     "ggml: enable AMX-TILE"         OFF)
    option(GGML_AMX_INT8     "ggml: enable AMX-INT8"         OFF)
    option(GGML_AMX_BF16     "ggml: enable AMX-BF16"         OFF)
endif()
option(GGML_LASX             "ggml: enable lasx"             ON)
option(GGML_LSX              "ggml: enable lsx"              ON)
option(GGML_RVV              "ggml: enable rvv"              ON)
option(GGML_RV_ZFH           "ggml: enable riscv zfh"        ON)
option(GGML_RV_ZVFH          "ggml: enable riscv zvfh"       ON)
option(GGML_RV_ZICBOP        "ggml: enable riscv zicbop"     ON)
option(GGML_XTHEADVECTOR     "ggml: enable xtheadvector"     OFF)
option(GGML_VXE              "ggml: enable vxe"              ${GGML_NATIVE})

option(GGML_CPU_ALL_VARIANTS "ggml: build all variants of the CPU backend (requires GGML_BACKEND_DL)" OFF)
set(GGML_CPU_ARM_ARCH        "" CACHE STRING "ggml: CPU architecture for ARM")
set(GGML_CPU_POWERPC_CPUTYPE "" CACHE STRING "ggml: CPU type for PowerPC")


if (MINGW)
    set(GGML_WIN_VER "0xA00" CACHE STRING   "ggml: Windows version")
endif()

# ggml core
set(GGML_SCHED_MAX_COPIES  "4" CACHE STRING "ggml: max input copies for pipeline parallelism")
option(GGML_CPU                             "ggml: enable CPU backend"                        ON)

# 3rd party libs / backends
option(GGML_ACCELERATE                      "ggml: enable Accelerate framework"               ON)
option(GGML_BLAS                            "ggml: use BLAS"                                  ${GGML_BLAS_DEFAULT})
set(GGML_BLAS_VENDOR ${GGML_BLAS_VENDOR_DEFAULT} CACHE STRING
                                            "ggml: BLAS library vendor")
option(GGML_LLAMAFILE                       "ggml: use LLAMAFILE"                             ${GGML_LLAMAFILE_DEFAULT})

option(GGML_CUDA                            "ggml: use CUDA"                                  OFF)
option(GGML_MUSA                            "ggml: use MUSA"                                  OFF)
option(GGML_CUDA_FORCE_MMQ                  "ggml: use mmq kernels instead of cuBLAS"         OFF)
option(GGML_CUDA_FORCE_CUBLAS               "ggml: always use cuBLAS instead of mmq kernels"  OFF)
set   (GGML_CUDA_PEER_MAX_BATCH_SIZE "128" CACHE STRING
                                            "ggml: max. batch size for using peer access")
option(GGML_CUDA_NO_PEER_COPY               "ggml: do not use peer to peer copies"            OFF)
option(GGML_CUDA_NO_VMM                     "ggml: do not try to use CUDA VMM"                OFF)
option(GGML_CUDA_FA                         "ggml: compile ggml FlashAttention CUDA kernels"  ON)
option(GGML_CUDA_FA_ALL_QUANTS              "ggml: compile all quants for FlashAttention"     OFF)
option(GGML_CUDA_GRAPHS                     "ggml: use CUDA graphs (llama.cpp only)"          ${GGML_CUDA_GRAPHS_DEFAULT})
set   (GGML_CUDA_COMPRESSION_MODE "size" CACHE STRING
                                            "ggml: cuda link binary compression mode; requires cuda 12.8+")
set_property(CACHE GGML_CUDA_COMPRESSION_MODE PROPERTY STRINGS "none;speed;balance;size")

option(GGML_HIP                             "ggml: use HIP"                                   OFF)
option(GGML_HIP_GRAPHS                      "ggml: use HIP graph, experimental, slow"         OFF)
option(GGML_HIP_NO_VMM                      "ggml: do not try to use HIP VMM"                 ON)
option(GGML_HIP_ROCWMMA_FATTN               "ggml: enable rocWMMA for FlashAttention"         OFF)
option(GGML_HIP_MMQ_MFMA                    "ggml: enable MFMA MMA for CDNA in MMQ"           ON)
option(GGML_HIP_EXPORT_METRICS              "ggml: enable kernel perf metrics output"         OFF)
option(GGML_MUSA_GRAPHS                     "ggml: use MUSA graph, experimental, unstable"    OFF)
option(GGML_MUSA_MUDNN_COPY                 "ggml: enable muDNN for accelerated copy"         OFF)
option(GGML_VULKAN                          "ggml: use Vulkan"                                OFF)
option(GGML_VULKAN_CHECK_RESULTS            "ggml: run Vulkan op checks"                      OFF)
option(GGML_VULKAN_DEBUG                    "ggml: enable Vulkan debug output"                OFF)
option(GGML_VULKAN_MEMORY_DEBUG             "ggml: enable Vulkan memory debug output"         OFF)
option(GGML_VULKAN_SHADER_DEBUG_INFO        "ggml: enable Vulkan shader debug info"           OFF)
option(GGML_VULKAN_VALIDATE                 "ggml: enable Vulkan validation"                  OFF)
option(GGML_VULKAN_RUN_TESTS                "ggml: run Vulkan tests"                          OFF)
option(GGML_WEBGPU                          "ggml: use WebGPU"                                OFF)
option(GGML_WEBGPU_DEBUG                    "ggml: enable WebGPU debug output"                OFF)
option(GGML_WEBGPU_CPU_PROFILE              "ggml: enable WebGPU profiling (CPU)"             OFF)
option(GGML_WEBGPU_GPU_PROFILE              "ggml: enable WebGPU profiling (GPU)"             OFF)

option(GGML_ZDNN                            "ggml: use zDNN"                                  OFF)
option(GGML_METAL                           "ggml: use Metal"                                 ${GGML_METAL_DEFAULT})
option(GGML_METAL_NDEBUG                    "ggml: disable Metal debugging"                   OFF)
option(GGML_METAL_SHADER_DEBUG              "ggml: compile Metal with -fno-fast-math"         OFF)
option(GGML_METAL_EMBED_LIBRARY             "ggml: embed Metal library"                       ${GGML_METAL})
set   (GGML_METAL_MACOSX_VERSION_MIN "" CACHE STRING
                                            "ggml: metal minimum macOS version")
set   (GGML_METAL_STD "" CACHE STRING       "ggml: metal standard version (-std flag)")
option(GGML_OPENMP                          "ggml: use OpenMP"                                ON)
option(GGML_RPC                             "ggml: use RPC"                                   OFF)
option(GGML_SYCL                            "ggml: use SYCL"                                  OFF)
option(GGML_SYCL_F16                        "ggml: use 16 bit floats for sycl calculations"   OFF)
option(GGML_SYCL_GRAPH                      "ggml: enable graphs in the SYCL backend"         ON)
option(GGML_SYCL_DNN                        "ggml: enable oneDNN in the SYCL backend"         ON)
set   (GGML_SYCL_TARGET "INTEL" CACHE STRING
                                            "ggml: sycl target device")
set   (GGML_SYCL_DEVICE_ARCH "" CACHE STRING
                                            "ggml: sycl device architecture")

option(GGML_OPENCL                          "ggml: use OpenCL"                                OFF)
option(GGML_OPENCL_PROFILING                "ggml: use OpenCL profiling (increases overhead)" OFF)
option(GGML_OPENCL_EMBED_KERNELS            "ggml: embed kernels"                             ON)
option(GGML_OPENCL_USE_ADRENO_KERNELS       "ggml: use optimized kernels for Adreno"          ON)
set   (GGML_OPENCL_TARGET_VERSION "300" CACHE STRING
                                            "gmml: OpenCL API version to target")

option(GGML_HEXAGON                         "ggml: enable Hexagon backend"                    OFF)

# toolchain for vulkan-shaders-gen
set   (GGML_VULKAN_SHADERS_GEN_TOOLCHAIN "" CACHE FILEPATH "ggml: toolchain file for vulkan-shaders-gen")

# extra artifacts
option(GGML_BUILD_TESTS    "ggml: build tests"    ${GGML_STANDALONE})
option(GGML_BUILD_EXAMPLES "ggml: build examples" ${GGML_STANDALONE})

#
# dependencies
#

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED true)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED true)

set(THREADS_PREFER_PTHREAD_FLAG ON)

find_package(Threads REQUIRED)

include(GNUInstallDirs)

#
# build the library
#

add_subdirectory(src)

#
# Benchmark executables
#

# AI Completion Benchmark - FULL PRODUCTION VERSION (with comprehensive tests)
if(EXISTS "${CMAKE_SOURCE_DIR}/tests/benchmark_completions_full.cpp")
    add_executable(benchmark_completions
        tests/benchmark_completions_full.cpp
        src/real_time_completion_engine.cpp
        src/qtapp/inference_engine.cpp
        src/qtapp/gguf_loader.cpp
        src/qtapp/quant_utils.cpp
        src/qtapp/transformer_inference.cpp
        src/qtapp/bpe_tokenizer.cpp
        src/qtapp/sentencepiece_tokenizer.cpp
        src/qtapp/vocabulary_loader.cpp
        src/qtapp/inflate_deflate_cpp.cpp
        src/gguf_loader.cpp
        src/gguf_vocab_resolver.cpp  # Universal vocab size detection for all GGUF models
        # Memory-mapped streaming GGUF loader for 3-70GB models
        src/ai/memory_mapped_file.cpp
        src/ai/streaming_gguf_loader_qt.cpp
    )
    
    target_link_libraries(benchmark_completions PRIVATE
        ${ALL_LIBS}
        brutal_gzip
        ggml
        ggml-cpu
        ggml-vulkan
    )
    
    target_include_directories(benchmark_completions PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/ai
        ${CMAKE_SOURCE_DIR}/3rdparty/ggml/include
        ${CMAKE_SOURCE_DIR}/3rdparty/ggml/src
        ${Qt6_INCLUDE_DIRS}
    )
    
    target_compile_options(benchmark_completions PRIVATE /EHsc /std:c++latest)
    
    set_target_properties(benchmark_completions PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/Release"
    )
    
    # Automatic DLL deployment for benchmark_completions
    deploy_runtime_dependencies(benchmark_completions)
    
    message(STATUS "✓ Building benchmark_completions (FULL PRODUCTION VERSION)")
endif()

#
# tests and examples
#

if (GGML_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif ()

if (GGML_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif ()

#
# install
#

include(CMakePackageConfigHelpers)

# all public headers
set(GGML_PUBLIC_HEADERS
    include/ggml.h
    include/ggml-cpu.h
    include/ggml-alloc.h
    include/ggml-backend.h
    include/ggml-blas.h
    include/ggml-cann.h
    include/ggml-cpp.h
    include/ggml-cuda.h
    include/ggml-opt.h
    include/ggml-metal.h
    include/ggml-rpc.h
    include/ggml-sycl.h
    include/ggml-vulkan.h
    include/ggml-webgpu.h
    include/gguf.h)

set_target_properties(ggml PROPERTIES PUBLIC_HEADER "${GGML_PUBLIC_HEADERS}")
#if (GGML_METAL)
#    set_target_properties(ggml PROPERTIES RESOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/ggml-metal.metal")
#endif()
install(TARGETS ggml LIBRARY PUBLIC_HEADER)
install(TARGETS ggml-base LIBRARY)

if (GGML_STANDALONE)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/ggml.pc.in
        ${CMAKE_CURRENT_BINARY_DIR}/ggml.pc
        @ONLY)

    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ggml.pc
        DESTINATION share/pkgconfig)
endif()

#
# Create CMake package
#



# Capture variables prefixed with GGML_.

set(variable_set_statements
"
####### Expanded from @GGML_VARIABLES_EXPANED@ by configure_package_config_file() #######
####### Any changes to this file will be overwritten by the next CMake run        #######

")

set(GGML_SHARED_LIB ${BUILD_SHARED_LIBS})

get_cmake_property(all_variables VARIABLES)
foreach(variable_name IN LISTS all_variables)
    if(variable_name MATCHES "^GGML_")
        string(REPLACE ";" "\\;"
               variable_value "${${variable_name}}")

        set(variable_set_statements
            "${variable_set_statements}set(${variable_name} \"${variable_value}\")\n")
    endif()
endforeach()

set(GGML_VARIABLES_EXPANDED ${variable_set_statements})

# Create the CMake package and set install location.

set(GGML_INSTALL_VERSION ${GGML_VERSION})
set(GGML_INCLUDE_INSTALL_DIR ${CMAKE_INSTALL_INCLUDEDIR} CACHE PATH "Location of header  files")
set(GGML_LIB_INSTALL_DIR     ${CMAKE_INSTALL_LIBDIR}     CACHE PATH "Location of library files")
set(GGML_BIN_INSTALL_DIR     ${CMAKE_INSTALL_BINDIR}     CACHE PATH "Location of binary  files")

configure_package_config_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/cmake/ggml-config.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/ggml-config.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ggml
    PATH_VARS GGML_INCLUDE_INSTALL_DIR
              GGML_LIB_INSTALL_DIR
              GGML_BIN_INSTALL_DIR)

write_basic_package_version_file(
        ${CMAKE_CURRENT_BINARY_DIR}/ggml-version.cmake
    VERSION ${GGML_INSTALL_VERSION}
    COMPATIBILITY SameMajorVersion)

target_compile_definitions(ggml-base PRIVATE
    GGML_VERSION="${GGML_INSTALL_VERSION}"
    GGML_COMMIT="${GGML_BUILD_COMMIT}"
)
message(STATUS "ggml version: ${GGML_INSTALL_VERSION}")
message(STATUS "ggml commit:  ${GGML_BUILD_COMMIT}")

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ggml-config.cmake
              ${CMAKE_CURRENT_BINARY_DIR}/ggml-version.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ggml)

if (MSVC)
    set(MSVC_WARNING_FLAGS
        /wd4005  # Macro redefinition
        /wd4244  # Conversion from one type to another type, possible loss of data
        /wd4267  # Conversion from 'size_t' to a smaller type, possible loss of data
        /wd4305  # Conversion from 'type1' to 'type2', possible loss of data
        /wd4566  # Conversion from 'char' to 'wchar_t', possible loss of data
        /wd4996  # Disable POSIX deprecation warnings
        /wd4702  # Unreachable code warnings
    )
    function(disable_msvc_warnings target_name)
        if(TARGET ${target_name})
            target_compile_options(${target_name} PRIVATE ${MSVC_WARNING_FLAGS})
        endif()
    endfunction()

    disable_msvc_warnings(ggml-base)
    disable_msvc_warnings(ggml)
    disable_msvc_warnings(ggml-cpu)
    disable_msvc_warnings(ggml-cpu-x64)
    disable_msvc_warnings(ggml-cpu-sse42)
    disable_msvc_warnings(ggml-cpu-sandybridge)
    disable_msvc_warnings(ggml-cpu-haswell)
    disable_msvc_warnings(ggml-cpu-skylakex)
    disable_msvc_warnings(ggml-cpu-icelake)
    disable_msvc_warnings(ggml-cpu-alderlake)

    if (GGML_BUILD_EXAMPLES)
        disable_msvc_warnings(common-ggml)
        disable_msvc_warnings(common)

        disable_msvc_warnings(mnist-common)
        disable_msvc_warnings(mnist-eval)
        disable_msvc_warnings(mnist-train)

        disable_msvc_warnings(gpt-2-ctx)
        disable_msvc_warnings(gpt-2-alloc)
        disable_msvc_warnings(gpt-2-backend)
        disable_msvc_warnings(gpt-2-sched)
        disable_msvc_warnings(gpt-2-quantize)
        disable_msvc_warnings(gpt-2-batched)

        disable_msvc_warnings(gpt-j)
        disable_msvc_warnings(gpt-j-quantize)

        disable_msvc_warnings(magika)
        disable_msvc_warnings(yolov3-tiny)
        disable_msvc_warnings(sam)

        disable_msvc_warnings(simple-ctx)
        disable_msvc_warnings(simple-backend)
    endif()

    if (GGML_BUILD_TESTS)
        disable_msvc_warnings(test-mul-mat)
        disable_msvc_warnings(test-arange)
        disable_msvc_warnings(test-backend-ops)
        disable_msvc_warnings(test-cont)
        disable_msvc_warnings(test-conv-transpose)
        disable_msvc_warnings(test-conv-transpose-1d)
        disable_msvc_warnings(test-conv1d)
        disable_msvc_warnings(test-conv2d)
        disable_msvc_warnings(test-conv2d-dw)
        disable_msvc_warnings(test-customop)
        disable_msvc_warnings(test-dup)
        disable_msvc_warnings(test-opt)
        disable_msvc_warnings(test-pool)
    endif ()
endif()

# ============================================================
# MASM IDE Alternative Build (Pure Assembly)
# ============================================================
if(EXISTS "${CMAKE_SOURCE_DIR}/src/masm/final-ide/build_masm_ide.bat")
    message(STATUS "MASM IDE build script found - pure assembly alternative available")
    
    # Add custom target for MASM IDE build
    add_custom_target(RawrXD-MASM-IDE
        COMMAND "${CMAKE_SOURCE_DIR}/src/masm/final-ide/build_masm_ide.bat"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/src/masm/final-ide"
        COMMENT "Building pure MASM RawrXD IDE"
    )
    
    # Add dependency check for MASM compiler
    find_program(ML64_COMPILER ml64)
    if(ML64_COMPILER)
        message(STATUS "MASM compiler found: ${ML64_COMPILER}")
        set(HAVE_MASM_COMPILER TRUE)
    else()
        message(WARNING "MASM compiler not found - RawrXD-MASM-IDE target requires ml64.exe")
        set(HAVE_MASM_COMPILER FALSE)
    endif()
endif()
