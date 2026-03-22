# Integrated runtime (Transcendence E→Ω)

**Scope:** native **Win32 / C++** IDE only. This is **not** the Electron `bigdaddyg-ide` agent (see **`docs/AUTONOMOUS_AGENT_ELECTRON.md`** for Ollama-backed orchestration there).

The Win32 IDE runs **one** coordinated boot path for the Transcendence stack after the main window is shown:

- **Phase hook:** `integrated_runtime` in `main_win32.cpp` **`heavyPhases`** (~L1827, after `showWindow` + a few `pumpMessages` ticks, **before** `ide.runMessageLoop()`). Also listed in **`config/startup_phases.txt`** and `src/core/startup_phase_registry.cpp` for probe / lazy-phase wiring. Heavy phases may be skipped when `RawrXD::Startup::isPhaseLazy(name)` is true for that name.
- **Phase runner:** `runPhase(..., "integrated_runtime")` → `RawrXD::IntegratedRuntime::boot()` (~L990–995 in `main_win32.cpp`).
- **Implementation:** `RawrXD::IntegratedRuntime::boot()` in `src/core/integrated_runtime.cpp` calls `rawrxd::TranscendenceCoordinator::instance().initializeAll()` (failures are **non-fatal**; see `PatchResult` logging in that file).
- **Shutdown:** `RawrXD::IntegratedRuntime::shutdown()` (~L1904–1908 in `main_win32.cpp`) runs **after** the message loop exits, **before** reverse-engineered ASM / MASM teardown, calling `shutdownAll()` on the coordinator.

## Opt out (debug)

Set `RAWRXD_SKIP_INTEGRATED_RUNTIME=1` to skip the coordinator boot; shutdown remains safe (no-op if boot was skipped).

## Related sources

- `src/core/integrated_runtime.{hpp,cpp}` — boot / shutdown implementation (`initializeAll` / `shutdownAll` wiring)
- `src/win32app/main_win32.cpp` — `heavyPhases`, `runPhase("integrated_runtime", …)`, message loop, shutdown order
- `config/startup_phases.txt` — listed phase name for probes / ordering
- `src/core/startup_phase_registry.cpp` — default phase order / lazy-phase registry
- `docs/AUTONOMOUS_AGENT_ELECTRON.md` — **Electron** shell agent (workspace + gates); orthogonal to Transcendence boot here
- `docs/BGZIPXD_WASM.md` — **Loader + Runner + Codec + RE = BGzipXD WASM** (stack identity + file anchors)
- `docs/AGENTIC_BROWSER_LAYER.md` — in-IDE WebView2 browser pane for agentic automation (separate from Monaco)
- `docs/VSCODE_LOGS_AND_FREEZE.md` — VS Code/Cursor log folders, **patch** tool spam, workspace excludes (orthogonal to Win32 IDE)
- `docs/GGUF_TENSOR_OFFSETS.md` — **tensor blob base + relative offset** (alignment / mmap one-liner); `GGUFRunner` already applies this in `parseGgufTensorTable`
- `docs/GGUF_UNIFIED_MEMORY.md` — optional **`RAWRXD_GGUF_USE_UNIFIED_MEMORY`** Win32 path (unified arena + tensor `memcpy`)
- `src/core/transcendence_coordinator.{hpp,cpp}` — phase order and health
- `src/win32app/Win32IDE_TranscendencePanel.cpp` — UI / commands (still available alongside automatic boot)

## TpsSmoke (`RawrXD-TpsSmoke`)

`D:\path\to\real-model.gguf` in examples is a **placeholder**. Use a real file path to a valid **GGUF** (e.g. downloaded weights). The tool rejects missing paths, non-files, and tiny/non-GGUF blobs before inference.

**Same regime as `RAWRXD_TPS_REF` (239):** default **8** decode steps (`max_tokens` argv / `Run-TpsSmoke.ps1 -MaxTokens`), matching `scripts/Benchmark-TpsSmoke-Models.ps1`. Use a modest-size model for apples-to-apples TPS; pass a larger second argument only when you intentionally want a longer decode.

### Batch CSV: `no_json_line` vs crash

- **`Exit = -1073741819`** is **`0xC0000005` `STATUS_ACCESS_VIOLATION`**: the exe **crashed** (bad pointer / heap corruption / stack blow-up). You will not see `RAWRXD_TPS_JSON=` because the process died mid-run. Rebuilding only for machine-JSON does **not** fix that — debug `RawrXD-TpsSmoke` / `GGUFRunner` under the debugger or capture a dump.
- **`Skipped = crashed`** in updated `Benchmark-TpsSmoke-Models.ps1` means the script detected an abnormal NTSTATUS-style exit; **`no_json_line`** with a normal small exit code may mean an **old** TpsSmoke without `RAWRXD_TPS_MACHINE_JSON` support.
- If your CSV still says **`rebuild TpsSmoke with RAWRXD_TPS_MACHINE_JSON`**, you are on an **old copy** of `Benchmark-TpsSmoke-Models.ps1` — pull/sync `D:\rawrxd` and re-run the script from there.
- **`Exit 2` + `weights_incomplete`** (and a `RAWRXD_TPS_JSON` row) means `parseGgufLayerWeights` did not produce tensors that pass **`inferenceWeightsReady()`** (missing **`blk.*`**, unsupported GGML type in **`GgufTensorBytes`**, **GQA/FFN** shape mismatch, or **`RAWRXD_GGUF_MAX_LAYER_FLOAT_RAM_GB`** cap). Check logs for **`[GGUFRunner] blk.* weights loaded`** / **`inference_ready=yes`**. Older binaries without guards could **0xC0000005** instead.
- **`RAWRXD_GGUF_MAX_LAYER_FLOAT_RAM_GB`:** optional upper bound (gibibytes) on the **estimated** float32 per-layer mirror before `parseGgufLayerWeights` runs; unset = no cap (large dequant may still **OOM**).
- Very large **F32** GGUFs can stress RAM and allocator paths; **K-quants** (**Q2_K**–**Q8_K**) and huge vocabs may still hit **unsupported types** (**IQ***) or **buggy** tensor paths in the reference runner — treat big checkpoints as **may crash** until the loader/inference path is hardened.
