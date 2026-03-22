# GGUF + UnifiedMemoryExecutor (Win32)

## Purpose

When enabled, `GGUFRunner` loads the **entire** `.gguf` file into the host-backed **unified memory arena** (`UnifiedMemoryExecutor::loadModelUnified`) instead of using `MapViewOfFile` / a heap mirror. Tensor payload reads then go through **`memcpy` from `unifiedFileBase + offset`** (`readTensorData`), using the same **absolute** offsets as the mmap/heap path (`parseGgufTensorTable`).

When **unified memory is off**, large GGUFs still use **`memcpy` from `mappedData + offset`** (full-file mmap or heap mirror) in `readTensorData` — only unusual paths fall back to `seek`+read on `NativeFile`.

## Enable

Set environment variable:

```text
RAWRXD_GGUF_USE_UNIFIED_MEMORY=1
```

Also accepted: `y`, `Y`, `t`, `T` as the first character (truthy).

**PowerShell** (same session as the IDE or TpsSmoke):

```powershell
$env:RAWRXD_GGUF_USE_UNIFIED_MEMORY = "1"
```

## Interaction with tensor offsets

Unified backing still uses **absolute file offsets** produced by `parseGgufTensorTable` (`tensorDataBaseOffset` + relative tensor offset, aligned per `general.alignment`). `readTensorData` becomes `memcpy(unified_base + offset, …)` instead of `seek` + read — see `docs/GGUF_TENSOR_OFFSETS.md`.

## Limits

- The executor’s **host-backed** path reserves a **512 MiB** region and keeps **64 MiB** slack; the bump heap is therefore on the order of **~448 MiB** usable for model copies.
- **`loadModelUnified` copies the full file** into that heap. Checkpoints **larger than remaining heap** are **not** loaded into unified memory; `GGUFRunner` logs a warning and **falls back** to the normal Win32 read-only map (or small-file heap mirror).
- **`UnifiedMemoryExecutor::free`** does not reclaim bump-heap space today; repeated loads into unified backing can exhaust the arena until process restart.

## Related

- `docs/GGUF_TENSOR_OFFSETS.md` — absolute tensor offsets (`tensorDataBase + relative offset`).
- `src/core/unified_memory_executor.{h,cpp}` — arena, `getUnifiedHeapRemainingBytes()`, `loadModelUnified()`.
- `src/llm_adapter/gguf_k_quants.cpp` / `GgufTensorBytes` — dequant for layer tensors; if layer load fails, unified vs mmap behaves the same (weights still missing).
- `RawrXD-TpsSmoke` + `scripts/Benchmark-TpsSmoke-Models.ps1` — look for log `blk.* weights loaded` and `inference_ready=yes` before expecting TPS numbers.
