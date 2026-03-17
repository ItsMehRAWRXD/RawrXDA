# Model Streaming & Hotpatching (800B-ready)

**Purpose**: Load ultra-large models (up to ~800B params) on CPU while keeping resident RAM under a fixed window.

## Streaming Mechanics
- **Threshold**: Files >= ~1GB trigger streaming (`STREAM_THRESHOLD_BYTES`).
- **Window Cap**: Configurable via `HotPatch_SetStreamCap` (default 512MB, max 1GB window).
- **IO Path**: Single reusable window buffer (`VirtualAlloc`) reused per chunk to avoid heap churn.
- **Apply Hook**: Each chunk is passed to `HotPatch_StreamedApplyChunk` (stub hook for patch/inject).
- **Safety**: Reads are bounded; borrow-safe decrement on 64-bit file sizes; closes handles on failure.

## Key Procedures (model_hotpatch_engine.asm)
- `HotPatch_SetStreamCap(dwWindowMB)`: Clamp + apply streaming window in MB.
- `HotPatch_StreamedLoadModel(lpPath, pEntry)`: Stream-read model under cap, invoke apply hook per chunk.
- `HotPatch_StreamedApplyChunk(pChunk, cbChunk, pEntry)`: Stub hook to patch/inject chunk (always returns 1 by default).
- `LoadModelByPath`: Size-aware; routes large files to streaming path; small files to `GgufUnified_LoadModelAutomatic`.

## Usage Example
```asm
; keep RAM under 768MB window
push 768
call HotPatch_SetStreamCap

; stream-load huge model (800B-scale file)
push 0          ; optional ModelEntry ptr
push OFFSET szPathHugeModel
call HotPatch_StreamedLoadModel
add  esp, 8
; eax = 1 on success
```

## Operational Notes
- Works on CPU-only workflows; no GPU residency assumed.
- Designed for 64GB hosts: window stays <=1GB; only one window live at a time.
- Hotpatch flow: read chunk -> apply hook -> continue; avoids double-buffering.
- Integrates with hot-swap: loader returns non-zero for success so existing swap logic remains intact.

## Next Hooks to Implement (optional)
- Wire `HotPatch_StreamedApplyChunk` to inference context writer/mapper.
- Add disk-prefetch or overlapped IO for higher throughput.
- Track telemetry per chunk (bytes/sec, time-to-first-token) in `HotPatchContext`.
