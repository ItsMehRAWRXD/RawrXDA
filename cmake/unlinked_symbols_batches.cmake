# unlinked_symbols_batches.cmake
# CMake configuration for unlinked symbol batch implementations
# Full production - no stubs

# Unlinked symbol batch implementations (12 legacy + batch 13 cathedral)
set(UNLINKED_SYMBOLS_BATCH_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/unlinked_symbols_batch_001.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/unlinked_symbols_batch_002.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/unlinked_symbols_batch_003.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/unlinked_symbols_batch_004.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/unlinked_symbols_batch_005.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/unlinked_symbols_batch_006.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/unlinked_symbols_batch_007.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/unlinked_symbols_batch_008.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/unlinked_symbols_batch_009.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/unlinked_symbols_batch_010.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/unlinked_symbols_batch_011.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/unlinked_symbols_batch_012.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/unlinked_symbols_batch_013.cpp
)

# Add to Win32IDE target
if(TARGET RawrXD-Win32IDE)
    target_sources(RawrXD-Win32IDE PRIVATE ${UNLINKED_SYMBOLS_BATCH_SOURCES})
    message(STATUS "[Unlinked Symbols] Added unlinked symbol batches (001–013) to Win32IDE")
endif()

# Symbol batch summary
message(STATUS "[Unlinked Symbols] Batch 1: Shutdown functions (15 symbols)")
message(STATUS "[Unlinked Symbols] Batch 2: GPU/Compute (15 symbols)")
message(STATUS "[Unlinked Symbols] Batch 3: V280 UI/RTP (15 symbols)")
message(STATUS "[Unlinked Symbols] Batch 4: Hotpatch/Snapshot (15 symbols)")
message(STATUS "[Unlinked Symbols] Batch 5: Watchdog/Camellia/Omega (15 symbols)")
message(STATUS "[Unlinked Symbols] Batch 6: Omega/Mesh (15 symbols)")
message(STATUS "[Unlinked Symbols] Batch 7: Mesh/Speciator (15 symbols)")
message(STATUS "[Unlinked Symbols] Batch 8: Speciator/Neural (15 symbols)")
message(STATUS "[Unlinked Symbols] Batch 9: HWSynth/Modes (15 symbols)")
message(STATUS "[Unlinked Symbols] Batch 10: Modes/Streaming (15 symbols)")
message(STATUS "[Unlinked Symbols] Batch 11: Streaming/Collab/CoT (15 symbols)")
message(STATUS "[Unlinked Symbols] Batch 12: Model hot-swap + native log + SPEngine CPU refresh")
message(STATUS "[Unlinked Symbols] Batch 13: Cathedral orchestrator + quadbuf + GGUF staging (15 symbols)")
message(STATUS "[Unlinked Symbols] See docs/UNLINKED_SYMBOLS_RESOLUTION.md")
