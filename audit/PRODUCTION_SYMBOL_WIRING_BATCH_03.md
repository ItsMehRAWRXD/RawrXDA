# Production Symbol Wiring - Batch 03

Batch 03 focuses on build-graph consistency and removing contradictory/no-op CMake wiring paths.

## Batch 03 fixes applied

1. Removed duplicated multi-mechanism MSVC LIBPATH injection loop that stacked `add_link_options` and `CMAKE_*_LINKER_FLAGS` mutations on top of `link_directories`.
2. Kept one authoritative MSVC library path mechanism (`link_directories(...)`) for cleaner link behavior.
3. Removed redundant early `enable_language(ASM_MASM)` call (ASM_MASM enable remains in the dedicated MASM support block).
4. Added `list(REMOVE_DUPLICATES INFERENCE_ENGINE_SOURCES)` guard before creating `RawrXD-InferenceEngine`.
5. Removed stale Win32IDE duplicate-removal block for `Win32IDE_CallStackSymbols.cpp` and `multi_file_search_stub.cpp` after those files were no longer appended in Batch 02.

## Remaining for next batches

- Replace broad stub provider lanes with real providers (`rawr_engine_link_shims.cpp`, `auto_feature_stub_impl.cpp`) once full symbol-closure lane is enabled.
- Replace `src/core/missing_handler_stubs.cpp` history include indirection with local canonical source ownership.
- Continue migration from global compile/link definitions to target-scoped definitions for stricter production reproducibility.
