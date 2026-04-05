# Omega HIP Bridge (RDNA3)

This lane keeps MASM as the orchestrator and moves tensor math to HIP kernels.

## Files

- `src/kernels/omega_kernels.hip`
- `src/kernels/omega_kernels.h`

## C ABI Function

```
int RawrXD_Omega_Attention11x_HIP(
    const void* q_half,
    const void* k_half,
    const float* v_f32,
    float* out_f32,
    int seq_len,
    int head_dim,
    uintptr_t stream_handle);
```

## Data Layout

- `q_half`: `[11, head_dim]` fp16
- `k_half`: `[seq_len, head_dim]` fp16
- `v_f32`: `[seq_len, head_dim]` fp32
- `out_f32`: `[11, head_dim]` fp32

## Build (manual, clean-room)

Use `hipcc` to produce an object or static lib and link into the Titan lane.

Example:

```powershell
hipcc -O3 --offload-arch=gfx1101 -c d:\rawrxd\src\kernels\omega_kernels.hip -o d:\rawrxd\build-ninja\omega_kernels.obj
lib /OUT:d:\rawrxd\build-ninja\omega_kernels.lib d:\rawrxd\build-ninja\omega_kernels.obj
```

Notes:

- `gfx1101` matches RX 7800 XT (Navi32 / RDNA3).
- If your `hipcc` path is not in `PATH`, use the absolute executable path.
- The kernel code includes a scalar fallback path when AMD-specific inline ISA is not active.

## MASM Bridge Call Shape

```asm
EXTERN RawrXD_Omega_Attention11x_HIP:PROC

; rcx = q_half
; rdx = k_half
; r8  = v_f32
; r9  = out_f32
; [rsp+28h] = seq_len (int)
; [rsp+30h] = head_dim (int)
; [rsp+38h] = stream_handle (uintptr_t)
; call RawrXD_Omega_Attention11x_HIP
```

This lets Omega Singularity keep ring-buffer/seq orchestration in MASM while offloading attention math to HIP.
