# Production Symbol Wiring - Batch 23

## Local / backup line (pre-merge)

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

---

## RawrXDA branch `cursor/unlinked-dissolved-symbols-4264`

Batch 23 extends strict production filtering to cover mock/fake and ASM-stub naming variants, and blocks a remaining stub-backed auxiliary library (`RawrXD-Crypto`) from strict production strip mode.

### Batch 23 fixes applied (15)

1. Added strict shared-source filtering for singular mock suffix files (`*_mock.cpp`).
2. Added strict shared-source filtering for singular fake suffix files (`*_fake.cpp`).
3. Added strict shared-source filtering for prefix/mock-bearing names (`*mock_*` / `*fake_*`).
4. Added strict shared-source filtering for ASM stub file names (`*_stubs.asm`).
5. Added strict `RAWR_ENGINE_SOURCES` filtering for mock/fake suffix and mock_/fake_ patterns.
6. Added strict `RAWR_ENGINE_SOURCES` filtering for ASM stub file names (`*_stubs.asm`).
7. Added strict `GOLD_UNDERSCORE_SOURCES` filtering for mock/fake suffix and mock_/fake_ patterns.
8. Added strict `GOLD_UNDERSCORE_SOURCES` filtering for ASM stub file names (`*_stubs.asm`).
9. Added strict `INFERENCE_ENGINE_SOURCES` filtering for mock/fake suffix and mock_/fake_ patterns.
10. Added strict `INFERENCE_ENGINE_SOURCES` filtering for ASM stub file names (`*_stubs.asm`).
11. Added strict Win32IDE strip filtering for mock/fake suffix patterns, mock_/fake_ patterns, and ASM stub file names.
12. Added Win32IDE agentic real-lane pre-filter exclusions for mock/fake suffix/prefix naming patterns.
13. Extended Win32IDE strict pre-check forbidden regex to include mock/fake suffix/prefix variants.
14. Extended `EnforceNoStubs`, broad Win32IDE post-filter, and strict post-check forbidden regexes to include mock/fake suffix variants consistently.
15. Gated `RawrXD-Crypto` out of strict production strip mode (it depends on `uac_bypass_impl_stub.cpp`) and added explicit strict-mode skip status messaging.

### Why this batch matters

This closes additional naming loopholes where strict production strip could miss compatibility units that are not named as classic `_stubs.cpp` but are still mock/fake or ASM-stub providers.

### Remaining for next batches

- Replace `src/core/missing_handler_stubs.cpp` history-indirected ownership wrapper with canonical source ownership under `src/core`.
- Continue collapsing non-strict fallback lanes into explicit compatibility options.
- Add stronger compiled-symbol closure checks for RawrEngine/Gold/Inference targets.
