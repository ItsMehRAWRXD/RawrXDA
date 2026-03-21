# Production Symbol Wiring - Batch 23

## Batch 23 — unlinked / dissolved symbols (15 + bridge), no stubs

1. **`unlinked_symbols_batch_013.cpp`** — Fifteen `extern "C"` symbols aligned with **`include/masm_bridge_cathedral.h`**: orchestrator lifecycle/dispatch/metrics/hooks/vtable/async queue/LSP sync; `asm_quadbuf_init` / `asm_quadbuf_render_thread`; GGUF staging + residency; **`fnv1a_hash64`** (real FNV-1a 64-bit).
2. **`unlinked_symbols_batch_012.cpp`** — Production **`HotSwapModel`** (path validation + `g_RawrXD_HotSwapModelRequest`), **`RawrXD_Native_Log`** (varargs + debug output), **`asm_spengine_cpu_optimize`** (`void(void)` + `__cpuid` feature refresh). Removed duplicate / wrong-ABI **`Enterprise_DevUnlock`**.
3. **`enterprise_devunlock_bridge.cpp`** — Always compiles **`extern "C" int64_t Enterprise_DevUnlock()`** (removed `RAWR_HAS_MASM` empty-TU gate). Logging uses **`OutputDebugStringA`** + **`stderr`** (no `std::cout`).
4. **`runtime_symbol_bridge.cpp`** — Removed empty **`asm_orchestrator_shutdown`** to avoid duplicate definition with batch 13.
5. **`CMakeLists.txt`** — Appends **`unlinked_symbols_batch_013.cpp`** to **RawrXD-Win32IDE**.
6. **`cmake/unlinked_symbols_batches.cmake`** — Lists batch 013 for optional include-based workflows.
7. **`docs/UNLINKED_SYMBOLS_RESOLUTION.md`** — Batch 12/13 descriptions updated.
8. **Rationale:** Win32IDE filters **`rawr_engine_link_shims.cpp`** out of the link; production real-lane also strips `*_fallback*.cpp`. Batches must supply cathedral-shaped symbols with mutex/atomic state — not empty TU placeholders.
9. **Hot-swap:** Consumers read **`g_RawrXD_HotSwapModelRequest`** after **`HotSwapModel`** returns true.
10. **Orchestrator:** **`asm_orchestrator_drain_queue`** CAS-drains async queue; metrics blob is zeroed with first qwords populated.
11. **GGUF lite state:** Default-open virtual session so **`asm_gguf_loader_stage`** succeeds when no separate init TU is linked.
12. **Quad buffer:** Stores HWND/size/flags under mutex for init; render-thread entry increments a run counter (telemetry hook).
13. **AVX-512 flag:** **`asm_spengine_cpu_optimize`** uses CPUID.7 EBX bit 16 as a **proxy** “foundation” bit — not a full ISA validator.
14. **Non-MSVC:** **`HotSwapModel`** uses `strncpy` fallback when **`strcpy_s`** unavailable.
15. **Next:** If Tier-2 ASM modules that export **`Enterprise_DevUnlock`** are linked into Win32IDE, resolve **LNK2005** via a single provider (bridge vs ASM), not both.
