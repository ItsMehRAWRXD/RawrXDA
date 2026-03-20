# Production Symbol Wiring - Batch 14

Batch 14 removes stale fallback/stub entries from Win32IDE source wiring and strengthens fallback detection so strict lanes cannot silently re-introduce dissolved symbol providers.

## Batch 14 fixes applied (15)

1. Removed `src/win32app/win32ide_extension_command_fallback.cpp` from the default `WIN32IDE_SOURCES` set.
2. Removed `src/core/enterprise_devunlock_stub_masm.cpp` from default `WIN32IDE_SOURCES`.
3. Removed `src/core/memory_patch_byte_search_stubs.cpp` from default `WIN32IDE_SOURCES`.
4. Removed `src/core/monaco_core_stubs.cpp` from default `WIN32IDE_SOURCES`.
5. Removed `src/core/mesh_brain_asm_stubs.cpp` from default `WIN32IDE_SOURCES`.
6. Removed `src/win32app/benchmark_menu_stub.cpp` from default `WIN32IDE_SOURCES`.
7. Removed `src/win32app/benchmark_runner_stub.cpp` from default `WIN32IDE_SOURCES`.
8. Removed stale `src/core/enterprise_devunlock_stub_masm.cpp` from Win32IDE normalization `list(REMOVE_ITEM ...)` block (no longer pre-added).
9. Removed stale `src/core/memory_patch_byte_search_stubs.cpp` from Win32IDE normalization `list(REMOVE_ITEM ...)` block.
10. Removed stale `src/core/mesh_brain_asm_stubs.cpp` from Win32IDE normalization `list(REMOVE_ITEM ...)` block.
11. Removed stale `src/core/monaco_core_stubs.cpp` from Win32IDE normalization `list(REMOVE_ITEM ...)` block.
12. Removed stale `src/win32app/benchmark_menu_stub.cpp` from Win32IDE normalization `list(REMOVE_ITEM ...)` block.
13. Removed stale `src/win32app/benchmark_runner_stub.cpp` from Win32IDE normalization `list(REMOVE_ITEM ...)` block.
14. Extended strict production strip (`RAWRXD_PRODUCTION_STRIP_STUB_SOURCES`) to remove additional fallback units: `win32_ide_link_stubs.cpp`, `win32ide_asm_fallback.cpp`, `rtp_protocol_fallback.cpp`, and `win32ide_extension_command_fallback.cpp`.
15. Strengthened Win32IDE fallback enforcement logic by treating `*_fallback.cpp` as forbidden in all strict filters/gates (`RAWRXD_ALLOW_AGENTIC_STUB_FALLBACK` filtering, strict pre-check, `EnforceNoStubs`, broad source filter, and strict post-check).

## Why this batch matters

This batch reduces dissolved runtime behavior caused by compatibility fallbacks shadowing real implementations (notably extension command handling), and makes strict production lanes enforceable against both `stub` and `fallback` naming classes.

## Remaining for next batches

- Eliminate history-indirected ownership in `src/core/missing_handler_stubs.cpp` by promoting canonical implementation ownership under `src/core`.
- Continue replacing fallback-only translation units with real providers or explicit non-production options.
- Add strict CI assertions for RawrEngine/Gold/Inference to mirror Win32IDE no-fallback enforcement depth.
