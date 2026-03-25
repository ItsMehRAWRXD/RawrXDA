# Tools - Command-line utilities for benchmarking and testing

<<<<<<< HEAD
set(_RAWRXD_OPTIONAL_GGML_LINK_LIBS)
if(TARGET ggml)
    list(APPEND _RAWRXD_OPTIONAL_GGML_LINK_LIBS ggml)
endif()

=======
>>>>>>> origin/main
# RawrXD KeyGen — RSA-4096 Key Authority & License Signer (unlock 800B kernel / enterprise)
# Build: standalone Win32 CLI; no Qt/engine deps. Use for --genkey, --hwid, --issue, --sign.
add_executable(RawrXD_KeyGen WIN32
    RawrXD_KeyGen.cpp
)
target_link_libraries(RawrXD_KeyGen PRIVATE advapi32 crypt32)
if(MSVC)
  target_compile_options(RawrXD_KeyGen PRIVATE /W4)
endif()

# Simple GPU Test - For hotpatch testing framework
if(TARGET InferenceEngine)
  add_executable(simple_gpu_test
      simple_gpu_test.cpp
  )

<<<<<<< HEAD
  # Qt-free: pure C++ with STL
  target_link_libraries(simple_gpu_test PRIVATE
      InferenceEngine
      ${_RAWRXD_OPTIONAL_GGML_LINK_LIBS}
  )
=======
# Qt-free: pure C++ with STL
target_link_libraries(simple_gpu_test PRIVATE
    InferenceEngine
    ggml
)
>>>>>>> origin/main

  # Real Multi-Model Benchmark - For comprehensive model testing
  add_executable(real_multi_model_benchmark
      real_multi_model_benchmark.cpp
  )

<<<<<<< HEAD
  target_link_libraries(real_multi_model_benchmark PRIVATE
      InferenceEngine
      ${_RAWRXD_OPTIONAL_GGML_LINK_LIBS}
  )
=======
target_link_libraries(real_multi_model_benchmark PRIVATE
    InferenceEngine
    ggml
)
>>>>>>> origin/main

  add_executable(bench_cpu
      bench_cpu.cpp
  )

<<<<<<< HEAD
  target_include_directories(bench_cpu PRIVATE
      ${CMAKE_SOURCE_DIR}/src
      ${CMAKE_SOURCE_DIR}/include
  )
=======
target_link_libraries(gguf_api_server PRIVATE
    InferenceEngine
    ggml
    ws2_32
)
>>>>>>> origin/main

  target_link_libraries(bench_cpu PRIVATE
      InferenceEngine
      ${_RAWRXD_OPTIONAL_GGML_LINK_LIBS}
  )

  add_executable(comprehensive_benchmark
      comprehensive_benchmark.cpp
      ../backend_selector.cpp
      ../inference_benchmark.cpp
  )

  target_include_directories(comprehensive_benchmark PRIVATE
      ${CMAKE_SOURCE_DIR}/src
      ${CMAKE_SOURCE_DIR}/include
  )

  target_link_libraries(comprehensive_benchmark PRIVATE
      InferenceEngine
      ${_RAWRXD_OPTIONAL_GGML_LINK_LIBS}
  )

  # GGUF API Server - Real HTTP API with GPU inference
  add_executable(gguf_api_server
      ../gguf_api_server.cpp
  )

  target_link_libraries(gguf_api_server PRIVATE
      InferenceEngine
      ${_RAWRXD_OPTIONAL_GGML_LINK_LIBS}
      ws2_32
  )

  target_include_directories(gguf_api_server PRIVATE
      ${CMAKE_SOURCE_DIR}/src
      ${CMAKE_SOURCE_DIR}/include
  )
else()
  message(STATUS "[Tools] InferenceEngine library target not available; skipping inference-dependent CLI tools")
endif()

# Install targets (RawrXD_KeyGen ships with IDE for License Creator > Launch KeyGen)
<<<<<<< HEAD
set(_RAWRXD_TOOL_INSTALL_TARGETS RawrXD_KeyGen)

foreach(_rawrxd_tool_target simple_gpu_test real_multi_model_benchmark bench_cpu comprehensive_benchmark gguf_api_server)
    if(TARGET ${_rawrxd_tool_target})
        list(APPEND _RAWRXD_TOOL_INSTALL_TARGETS ${_rawrxd_tool_target})
    endif()
endforeach()

install(TARGETS ${_RAWRXD_TOOL_INSTALL_TARGETS}
        RUNTIME DESTINATION bin
=======
install(TARGETS RawrXD_KeyGen simple_gpu_test real_multi_model_benchmark gguf_api_server
    RUNTIME DESTINATION bin
>>>>>>> origin/main
)

