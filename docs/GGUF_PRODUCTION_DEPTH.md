# GGUF production depth (E02)

Last updated: 2026-03-21.

## RAM and layer loading

- **`RAWRXD_GGUF_MAX_LAYER_FLOAT_RAM_GB`** — caps estimated float mirror RAM before per-layer dequant; refusal/clamp paths log in `GGUFRunner.cpp` (`ggufEstimatedLayerFloatMirrorWithinRamBudget`, runInference guards).
- **`RAWRXD_GGUF_USE_UNIFIED_MEMORY`** — optional unified arena for GGUF bytes; failures fall back to file-backed reads with explicit log lines.

## Quantization / IQ-family

- K-quants and mixed paths are implemented under `src/llm_adapter/` (`gguf_k_quants`, `GGUFRunner_kdequant`, etc.). Unsupported or partially wired GGUF tensor families must **fail loudly** with a single runner error string (no silent fallback to unrelated backends).

## Operational checks

- **`RawrXD-TpsSmoke`** / `src/tools/RawrXD_TpsSmoke.cpp` — use for throughput smoke on reference models; pair with `scripts/Run-TpsSmoke.ps1` when available in your environment.

## WASM parity (same runner equation)

Win32 **LocalGGUF** hosts the **BGzipXD** stack natively; the **WASM** product is the same loader + runner + codec + RE with a different platform layer — **`docs/BGZIPXD_WASM.md`**.

## Reference

- `docs/INFERENCE_PATH_MATRIX.md` — user-action routing.
- `docs/BGZIPXD_WASM.md` — stack identity (native Win32 vs WASM).
- `docs/GGUF_TENSOR_OFFSETS.md`, `docs/GGUF_UNIFIED_MEMORY.md` — tensor layout and memory lanes.
