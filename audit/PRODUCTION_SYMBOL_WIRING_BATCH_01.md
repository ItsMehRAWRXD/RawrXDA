# Production Symbol Wiring - Batch 01

This batch applies high-confidence production-grade source wiring fixes focused on
duplicate/contradictory source inclusion and dissolved shim ownership.

## Batch 01 fixes applied

1. Removed duplicate `src/asm/RawrXD_PDBKernel_v3.asm` entry from `ASM_KERNEL_SOURCES`.
2. Added `list(REMOVE_DUPLICATES ASM_KERNEL_SOURCES)` safety guard.
3. Added `list(REMOVE_DUPLICATES GOLD_UNDERSCORE_SOURCES)` safety guard.
4. Set `RAWRXD_LINK_ANALYZER_DISTILLER_ASM=0` for `RawrXD_Gold` to match excluded ASM units.
5. Set `RAWRXD_LINK_STREAMING_ORCHESTRATOR_ASM=0` for `RawrXD_Gold` to match excluded ASM units.
6. Removed `src/win32app/HeadlessIDE.h` from `WIN32IDE_SOURCES` compile list.
7. Removed duplicate `src/win32app/HeadlessIDE.cpp` from `WIN32IDE_SOURCES`.
8. Removed duplicate `src/core/shadow_page_detour.cpp` from `WIN32IDE_SOURCES`.
9. Removed duplicate `src/core/sentinel_watchdog.cpp` from `WIN32IDE_SOURCES`.
10. Removed duplicate `src/security/InputSanitizer.cpp` from `WIN32IDE_SOURCES`.
11. Removed non-compile header entry `src/editor/ghost_text_renderer.hpp` from `WIN32IDE_SOURCES`.
12. Removed non-compile header entry `src/terminal/embedded_terminal.hpp` from `WIN32IDE_SOURCES`.
13. Removed non-compile header entry `src/lsp/lsp_client_wired.hpp` from `WIN32IDE_SOURCES`.
14. Removed non-compile header entry `src/agentic/multi_file_composer.hpp` from `WIN32IDE_SOURCES`.
15. Removed non-compile header entry `src/git/git_wired.hpp` from `WIN32IDE_SOURCES`.
16. Removed non-compile header entry `src/ui/warp_hud.hpp` from `WIN32IDE_SOURCES`.
17. Removed non-compile header entry `src/ui/logic_bridge.hpp` from `WIN32IDE_SOURCES`.
18. Removed non-compile header entry `src/config/settings.hpp` from `WIN32IDE_SOURCES`.
19. Fixed Win32IDE `list(REMOVE_ITEM ...)` to use relative paths that match list entries.
20. Removed duplicate `RobustOllamaParser::parse_tags_response()` shim from `src/core/rawr_engine_link_shims.cpp`.
21. Removed fallback `int main()` shim from `src/core/rawr_engine_link_shims.cpp`.

## Remaining for next batches

- De-stub production command/provider lanes (`rawr_engine_link_shims.cpp`, `auto_feature_stub_impl.cpp`).
- Replace `history/reconstructed` include indirection in `src/core/missing_handler_stubs.cpp` with local canonical source ownership.
- Resolve target-level duplicate provider choices where the same subsystem appears in both global `SOURCES` and target-local source appends.
