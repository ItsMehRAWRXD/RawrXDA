Orchestrator Test — build instructions

This directory contains `test_stream_orch.c`, a minimal C harness that uses
the MASM streaming orchestrator routines exposed via `src/core/stream_orch.h`.

How to build (suggested):

1. Add the following small CMake target to your top-level `CMakeLists.txt` or
   include it from the existing test subdirectory. Adjust paths as needed.

```cmake
# Test harness for streaming orchestrator
add_executable(test_stream_orch
  test_stream_orch.c
)

# Ensure the MASM object/library providing the orchestrator symbols is linked.
# Replace `gguf_analyzer_masm64` below with the real target or import the .obj/.lib.
target_link_libraries(test_stream_orch PRIVATE gguf_analyzer_masm64)

target_include_directories(test_stream_orch PRIVATE
  ${CMAKE_SOURCE_DIR}/src/core
)

set_target_properties(test_stream_orch PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/test
)
```

2. Run CMake configure/generate and build the test target:

```powershell
cmake --build . --config Release --target test_stream_orch
```

3. Run the produced executable (example):

```powershell
.\test\test_stream_orch.exe
```

Notes:
- The test assumes the MASM orchestrator symbols are available to link. If
  the assembler produces an object or static library, link that into the
  `test_stream_orch` target.
- The test uses aligned allocation. On Windows it calls `_aligned_malloc`.
