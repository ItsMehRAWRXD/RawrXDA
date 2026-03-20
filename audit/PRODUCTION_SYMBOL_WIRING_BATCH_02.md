# Production Symbol Wiring - Batch 02

Batch 02 focuses on Win32IDE source-list correctness, stub/fallback ownership, and portability-safe CMake behavior.

## Batch 02 fixes applied

1. Guarded optional local SDK include path (`d:/rawrxd/include`) behind `if(EXISTS ...)`.
2. Made `RawrXD_Gold` MASM compile define reflect actual `RAWR_HAS_MASM` state via generator expression.
3. Removed header-only entry `src/win32app/Win32IDE_SubAgent.h` from Win32IDE compile sources.
4. Removed header-only entry `src/win32app/Win32TerminalManager.h` from Win32IDE compile sources.
5. Removed header-only entry `src/win32app/TransparentRenderer.h` from Win32IDE compile sources.
6. Removed header-only entry `src/win32app/ModelConnection.h` from Win32IDE compile sources.
7. Removed header-only entry `src/win32app/ContextManager.h` from Win32IDE compile sources.
8. Removed redundant append of `src/win32app/Win32IDE_CallStackSymbols.cpp` (already removed later as duplicate provider).
9. Removed redundant append of `src/win32app/multi_file_search_stub.cpp` (stub lane removed later).
10. Removed dead append of `src/tool_registry.cpp` from Win32IDE (was appended and stripped).
11. Removed dead append of `src/agentic/RawrXD_ToolRegistry.cpp` from Win32IDE (was appended and stripped).
12. Removed dead append of `src/agent/DiskRecoveryAgent.cpp` from Win32IDE (was appended and stripped).
13. Removed wrong-signature fallback source `src/agentic/masm_agent_failure_fallback.cpp` from Win32IDE sources.
14. Split Win32IDE-only ASM into `WIN32IDE_EXTRA_ASM` instead of mutating global `ASM_KERNEL_SOURCES`.
15. Repositioned `src/core/ai_agent_masm_stubs.cpp` append after broad stub filtering so required MASM bridge symbols remain wired.
16. Removed obsolete Win32IDE “Final Duplicate Elimination” remove block for files no longer appended.
17. Updated `RawrXD-Win32IDE` target creation to include `${WIN32IDE_EXTRA_ASM}` explicitly.

## Remaining for next batches

- Replace broad stub lanes (`rawr_engine_link_shims.cpp`, `auto_feature_stub_impl.cpp`) with concrete providers per subsystem.
- Remove/replace `history/reconstructed` include indirection for `src/core/missing_handler_stubs.cpp`.
- Continue tightening target-scoped compile/link definitions to avoid global leakage.
