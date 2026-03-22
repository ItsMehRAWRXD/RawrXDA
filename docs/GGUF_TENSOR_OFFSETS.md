# GGUF tensor offsets (RawrXD `GGUFRunner`)

## Spec (llama.cpp–style GGUF)

After the header, KV pairs, and the **tensor info table** (name, dims, type, `offset`), the **tensor payload region** starts at the next address aligned to `general.alignment` (default **32**, often **64** in newer files).

Each tensor’s stored `offset` field is **relative to the start of that payload region**, not relative to file byte `0`.

So the **absolute** byte offset in the file is:

```text
abs_offset = align_up(end_of_tensor_info_table, general.alignment) + tensor.offset
```

With a memory-mapped view of the **entire** file from base address `base`:

```cpp
const uint8_t* tensor_ptr = static_cast<const uint8_t*>(base) + abs_offset;
```

That is the same idea as: `mmap_base + header.tensor_data_offset + current_tensor.offset`, where `tensor_data_offset` is the aligned start of the bulk tensor blob (we **compute** that value as `tensorDataBase` instead of reading a separate header field).

## What RawrXD does

In `GGUFRunner::parseGgufTensorTable` (`src/llm_adapter/GGUFRunner.cpp`):

1. **`ingestGgufKvMetadata`** (earlier in `loadGGUFModel`) sets `context_.ggufTensorAlignment` from KV `general.alignment` when present (default **32**).
2. After reading all tensor descriptors, **`endMeta`** = stream position after the last tensor’s metadata fields.
3. **`tensorDataBase`** = `align_up(endMeta, alignment)` → stored as `context_.tensorDataBaseOffset`.
4. Each tensor’s **`desc.offset`** is rewritten from **relative** → **absolute**: `tensorDataBase + rel`.

`readTensorData` / `loadTensor` use **`abs_offset`** into the resident byte image:

- **`RAWRXD_GGUF_USE_UNIFIED_MEMORY` (Win32):** `memcpy` from `unifiedFileBase + abs_offset`.
- **Otherwise**, if the whole GGUF is already resident at **`mappedData`** (mmap / `MapViewOfFile` / small heap mirror): **`memcpy` from `reinterpret_cast<const uint8_t*>(mappedData) + abs_offset`** — no per-tensor `seek`+read on the `ifstream`.
- **Fallback:** `seek(abs_offset)` + `read` when no contiguous backing is available.

## Why this matters for `0xC0000005`

Using **`tensor.offset` alone** (or forgetting alignment) shifts every read by a few bytes → wrong quant blocks, garbage pointers into unmapped space, or unaligned SIMD loads → **access violation**.

## Not the same as “layers not loading”

If the table parses but **`inferenceWeightsReady()`** is false, the usual cause is **no `blk.*` weights in RAM** (quant path / layer loader / RAM budget), not tensor offset math — offsets still must be correct for `token_embd` and any tensor you do read.

## VRAM / huge checkpoints

Tensor offset math is independent of **where** weights live after read (heap float mirror vs future GPU upload). A **38 GB** GGUF on a **16 GB** GPU still needs **staging / shards / CPU tensors**; fixing offsets does not remove that constraint.
