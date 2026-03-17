; ============================================================================
; DirectML_Bridge.asm — MASM x64 Bridge for DirectML Compute Engine
; ============================================================================
; High-performance assembly bridge for DirectML tensor operations.
; Provides:
;   - Fast host-to-device memory staging (SIMD memcpy for tensor upload)
;   - DML dispatch hot-path (COM vtable calls inlined)
;   - Fence synchronization fast-path
;   - Dequantization helpers (Q4_0, Q4_K_M, Q8_0 → FP16/FP32)
;   - RoPE rotation kernel (sincos pair rotation for positional encoding)
;   - Atomic stats counters
;
; Calling Convention: Windows x64 fastcall
;   args: RCX, RDX, R8, R9, [RSP+40], [RSP+48]
;   return: RAX (0 = success, -1 = failure)
;   volatile: RAX, RCX, RDX, R8, R9, R10, R11, XMM0-XMM5
;   non-volatile: RBX, RSI, RDI, R12-R15, XMM6-XMM15
;
; Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
; ============================================================================

INCLUDE RawrXD_Common.inc

option casemap:none

; ============================================================================
; Public exports
; ============================================================================
PUBLIC asm_dml_fast_memcpy_h2d
PUBLIC asm_dml_fast_memcpy_d2h
PUBLIC asm_dml_fence_spin_wait
PUBLIC asm_dml_vtable_dispatch
PUBLIC asm_dml_dequant_q4_0_to_fp32
PUBLIC asm_dml_dequant_q8_0_to_fp32
PUBLIC asm_dml_rope_rotate_fp16
PUBLIC asm_dml_rope_rotate_fp32
PUBLIC asm_dml_atomic_inc_dispatch
PUBLIC asm_dml_atomic_add_bytes
PUBLIC asm_dml_prefetch_tensor_block
PUBLIC asm_dml_compute_op_hash

; ============================================================================
; External imports
; ============================================================================
EXTERNDEF WaitForSingleObject:PROC
EXTERNDEF GetLastError:PROC

; ============================================================================
; Constants
; ============================================================================
DML_CACHE_LINE_SIZE         EQU     64
DML_SIMD_BLOCK_SIZE         EQU     256         ; Process 256 bytes per iteration (4x YMM)
DML_FENCE_SPIN_LIMIT        EQU     10000       ; Spin iterations before yielding
DML_Q4_BLOCK_SIZE           EQU     32          ; Q4_0: 32 elements per block
DML_Q4_BLOCK_BYTES          EQU     18          ; Q4_0: 2 bytes scale + 16 bytes data
DML_Q8_BLOCK_SIZE           EQU     32          ; Q8_0: 32 elements per block
DML_Q8_BLOCK_BYTES          EQU     34          ; Q8_0: 2 bytes scale + 32 bytes data
FNV1A_OFFSET                EQU     0CBF29CE484222325h
FNV1A_PRIME                 EQU     100000001B3h

; ============================================================================
; Data section
; ============================================================================
.data
ALIGN 8
g_DMLDispatchCount          DQ      0
g_DMLBytesUploaded          DQ      0
g_DMLBytesDownloaded        DQ      0
g_DMLFenceSpinTotal         DQ      0
g_DMLFenceYieldCount        DQ      0
g_DMLDequantBlocks          DQ      0
g_DMLRoPERotations          DQ      0

; Q4_0 lookup table: 4-bit value (-8..7) → float
ALIGN 16
g_Q4LookupFP32             DD      -8.0, -7.0, -6.0, -5.0, -4.0, -3.0, -2.0, -1.0
                            DD       0.0,  1.0,  2.0,  3.0,  4.0,  5.0,  6.0,  7.0

; Two-pi constant for RoPE
ALIGN 16
g_TwoPi                     DD      6.28318530  ; 2*pi
g_RoPETheta                 DD      10000.0     ; Default base theta
g_Half                      DD      0.5

.code

; ============================================================================
; asm_dml_fast_memcpy_h2d — SIMD-accelerated Host→Device staging copy
; ============================================================================
; Args:
;   RCX = dest (mapped upload buffer pointer — already Map'd by D3D12)
;   RDX = src (host data pointer)
;   R8  = byteCount
; Returns:
;   RAX = bytes copied (== byteCount on success)
; Notes:
;   Uses AVX2 256-bit stores for aligned bulk copy.
;   Falls back to REP MOVSB for tail bytes.
; ============================================================================
asm_dml_fast_memcpy_h2d PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    sub     rsp, 32
    .allocstack 32
    .endprolog

    ; save args
    mov     rdi, rcx        ; dest
    mov     rsi, rdx        ; src
    mov     rbx, r8         ; byteCount
    xor     rax, rax        ; bytes copied = 0

    ; Validate
    test    rdi, rdi
    jz      @@h2d_fail
    test    rsi, rsi
    jz      @@h2d_fail
    test    rbx, rbx
    jz      @@h2d_done      ; zero bytes = success

    ; Check alignment — if both 32-byte aligned, use AVX path
    mov     rcx, rdi
    or      rcx, rsi
    test    rcx, 1Fh        ; both aligned to 32?
    jnz     @@h2d_unaligned

    ; --- AVX2 aligned path ---
    mov     rcx, rbx
    shr     rcx, 8          ; number of 256-byte blocks
    test    rcx, rcx
    jz      @@h2d_tail

@@h2d_avx_loop:
    vmovdqa ymm0, YMMWORD PTR [rsi]
    vmovdqa ymm1, YMMWORD PTR [rsi + 32]
    vmovdqa ymm2, YMMWORD PTR [rsi + 64]
    vmovdqa ymm3, YMMWORD PTR [rsi + 96]
    vmovdqa ymm4, YMMWORD PTR [rsi + 128]
    vmovdqa ymm5, YMMWORD PTR [rsi + 160]
    ; Use non-temporal stores to bypass cache (bulk uploads)
    vmovntdq YMMWORD PTR [rdi], ymm0
    vmovntdq YMMWORD PTR [rdi + 32], ymm1
    vmovntdq YMMWORD PTR [rdi + 64], ymm2
    vmovntdq YMMWORD PTR [rdi + 96], ymm3
    vmovntdq YMMWORD PTR [rdi + 128], ymm4
    vmovntdq YMMWORD PTR [rdi + 160], ymm5

    vmovdqa ymm0, YMMWORD PTR [rsi + 192]
    vmovdqa ymm1, YMMWORD PTR [rsi + 224]
    vmovntdq YMMWORD PTR [rdi + 192], ymm0
    vmovntdq YMMWORD PTR [rdi + 224], ymm1

    add     rsi, DML_SIMD_BLOCK_SIZE
    add     rdi, DML_SIMD_BLOCK_SIZE
    add     rax, DML_SIMD_BLOCK_SIZE
    dec     rcx
    jnz     @@h2d_avx_loop

    sfence                  ; Flush NT stores
    vzeroupper

@@h2d_tail:
    ; Handle remaining bytes with REP MOVSB
    mov     rcx, rbx
    sub     rcx, rax        ; remaining = total - copied
    test    rcx, rcx
    jz      @@h2d_done

    ; rdi and rsi already point to remainder
    rep     movsb
    add     rax, rcx        ; total = avx_copied + tail

    jmp     @@h2d_done

@@h2d_unaligned:
    ; Unaligned fallback: use vmovdqu + NT store
    mov     rcx, rbx
    shr     rcx, 7          ; 128-byte blocks
    test    rcx, rcx
    jz      @@h2d_scalar

@@h2d_unaligned_loop:
    vmovdqu ymm0, YMMWORD PTR [rsi]
    vmovdqu ymm1, YMMWORD PTR [rsi + 32]
    vmovdqu ymm2, YMMWORD PTR [rsi + 64]
    vmovdqu ymm3, YMMWORD PTR [rsi + 96]
    vmovntdq YMMWORD PTR [rdi], ymm0
    vmovntdq YMMWORD PTR [rdi + 32], ymm1
    vmovntdq YMMWORD PTR [rdi + 64], ymm2
    vmovntdq YMMWORD PTR [rdi + 96], ymm3

    add     rsi, 128
    add     rdi, 128
    add     rax, 128
    dec     rcx
    jnz     @@h2d_unaligned_loop

    sfence
    vzeroupper

@@h2d_scalar:
    ; Remaining bytes
    mov     rcx, rbx
    sub     rcx, rax
    test    rcx, rcx
    jz      @@h2d_done
    rep     movsb
    add     rax, rcx

@@h2d_done:
    ; Update stats
    lock add QWORD PTR [g_DMLBytesUploaded], rax

    add     rsp, 32
    pop     rdi
    pop     rsi
    pop     rbx
    ret

@@h2d_fail:
    xor     rax, rax
    dec     rax             ; return -1
    add     rsp, 32
    pop     rdi
    pop     rsi
    pop     rbx
    ret
asm_dml_fast_memcpy_h2d ENDP


; ============================================================================
; asm_dml_fast_memcpy_d2h — SIMD-accelerated Device→Host readback copy
; ============================================================================
; Args:
;   RCX = dest (host buffer)
;   RDX = src (mapped readback buffer)
;   R8  = byteCount
; Returns:
;   RAX = bytes copied
; Notes:
;   Uses temporal loads (readback data is cold), non-temporal stores to host.
; ============================================================================
asm_dml_fast_memcpy_d2h PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    sub     rsp, 32
    .allocstack 32
    .endprolog

    mov     rdi, rcx        ; dest (host)
    mov     rsi, rdx        ; src (mapped GPU readback)
    mov     rbx, r8         ; byteCount
    xor     rax, rax

    test    rdi, rdi
    jz      @@d2h_fail
    test    rsi, rsi
    jz      @@d2h_fail
    test    rbx, rbx
    jz      @@d2h_done

    ; Prefetch source data
    prefetchnta [rsi]
    prefetchnta [rsi + 64]
    prefetchnta [rsi + 128]

    ; Bulk copy: 128-byte blocks
    mov     rcx, rbx
    shr     rcx, 7
    test    rcx, rcx
    jz      @@d2h_tail

@@d2h_loop:
    prefetchnta [rsi + 256]
    prefetchnta [rsi + 320]

    vmovdqu ymm0, YMMWORD PTR [rsi]
    vmovdqu ymm1, YMMWORD PTR [rsi + 32]
    vmovdqu ymm2, YMMWORD PTR [rsi + 64]
    vmovdqu ymm3, YMMWORD PTR [rsi + 96]

    vmovdqu YMMWORD PTR [rdi], ymm0
    vmovdqu YMMWORD PTR [rdi + 32], ymm1
    vmovdqu YMMWORD PTR [rdi + 64], ymm2
    vmovdqu YMMWORD PTR [rdi + 96], ymm3

    add     rsi, 128
    add     rdi, 128
    add     rax, 128
    dec     rcx
    jnz     @@d2h_loop

    vzeroupper

@@d2h_tail:
    mov     rcx, rbx
    sub     rcx, rax
    test    rcx, rcx
    jz      @@d2h_done
    rep     movsb
    add     rax, rcx

@@d2h_done:
    lock add QWORD PTR [g_DMLBytesDownloaded], rax

    add     rsp, 32
    pop     rdi
    pop     rsi
    pop     rbx
    ret

@@d2h_fail:
    xor     rax, rax
    dec     rax
    add     rsp, 32
    pop     rdi
    pop     rsi
    pop     rbx
    ret
asm_dml_fast_memcpy_d2h ENDP


; ============================================================================
; asm_dml_fence_spin_wait — Fast fence-based GPU synchronization
; ============================================================================
; Args:
;   RCX = fence (ID3D12Fence* — COM object)
;   RDX = targetValue (uint64)
;   R8  = eventHandle (HANDLE for fallback WaitForSingleObject)
;   R9  = timeoutMs (DWORD)
; Returns:
;   RAX = 0 success, -1 timeout
; Notes:
;   Spins on ID3D12Fence::GetCompletedValue (vtable[8]) to avoid
;   kernel transition. Falls back to SetEventOnCompletion + Wait
;   after DML_FENCE_SPIN_LIMIT iterations.
; ============================================================================
asm_dml_fence_spin_wait PROC FRAME
    push    rbx
    .pushreg rbx
    push    r12
    .pushreg r12
    push    r13
    .pushreg r13
    push    r14
    .pushreg r14
    push    r15
    .pushreg r15
    sub     rsp, 48
    .allocstack 48
    .endprolog

    mov     r12, rcx        ; fence
    mov     r13, rdx        ; targetValue
    mov     r14, r8         ; eventHandle
    mov     r15d, r9d       ; timeoutMs
    xor     ebx, ebx        ; spin counter

    test    r12, r12
    jz      @@fence_fail

    ; Get vtable pointer
    mov     rax, QWORD PTR [r12]    ; vtable base

@@fence_spin:
    ; Call ID3D12Fence::GetCompletedValue (vtable[8])
    ; RCX = this
    mov     rcx, r12
    mov     rax, QWORD PTR [r12]            ; reload vtable (COM safety)
    call    QWORD PTR [rax + 8 * 8]         ; vtable[8] → GetCompletedValue
    ; RAX = completed value

    cmp     rax, r13
    jae     @@fence_complete                ; completed >= target → done

    ; Spin
    inc     ebx
    cmp     ebx, DML_FENCE_SPIN_LIMIT
    jae     @@fence_yield

    pause                                   ; CPU hint: spin-wait
    jmp     @@fence_spin

@@fence_yield:
    ; Spin limit reached — fall back to kernel wait
    lock inc QWORD PTR [g_DMLFenceYieldCount]

    ; Call ID3D12Fence::SetEventOnCompletion (vtable[9])
    ; RCX = this, RDX = value, R8 = event
    mov     rcx, r12
    mov     rdx, r13
    mov     r8, r14
    mov     rax, QWORD PTR [r12]
    call    QWORD PTR [rax + 9 * 8]        ; vtable[9]
    test    eax, eax
    js      @@fence_fail                    ; HRESULT < 0 = error

    ; WaitForSingleObject(event, timeout)
    mov     rcx, r14
    mov     edx, r15d
    call    WaitForSingleObject
    cmp     eax, 0                          ; WAIT_OBJECT_0 = 0
    jne     @@fence_timeout

@@fence_complete:
    lock add QWORD PTR [g_DMLFenceSpinTotal], rbx
    xor     eax, eax        ; success
    jmp     @@fence_exit

@@fence_timeout:
    mov     eax, -1
    jmp     @@fence_exit

@@fence_fail:
    mov     eax, -1

@@fence_exit:
    add     rsp, 48
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rbx
    ret
asm_dml_fence_spin_wait ENDP


; ============================================================================
; asm_dml_vtable_dispatch — Generic COM vtable call forwarder
; ============================================================================
; Args:
;   RCX = COM object pointer (this)
;   RDX = vtable index (uint32)
;   R8  = arg1 for the target method
;   R9  = arg2 for the target method
;   [RSP+40] = arg3
;   [RSP+48] = arg4
; Returns:
;   RAX = return value from COM method
; Notes:
;   Universal vtable dispatcher for any DML/D3D12 COM call.
;   Shifts args: target method gets (this=RCX, arg1→RDX, arg2→R8, arg3→R9, arg4→stack)
; ============================================================================
asm_dml_vtable_dispatch PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 56             ; shadow + extra args
    .allocstack 56
    .endprolog

    test    rcx, rcx
    jz      @@vtdisp_fail

    mov     rbx, rcx            ; save this
    mov     eax, edx            ; vtable index

    ; Load vtable
    mov     r10, QWORD PTR [rbx]            ; vtable base
    mov     r10, QWORD PTR [r10 + rax * 8]  ; target function pointer

    ; Rearrange args for COM call:
    ; COM call: RCX=this, RDX=arg1(was R8), R8=arg2(was R9), R9=arg3(was [RSP+40+56+8])
    mov     rcx, rbx            ; this
    mov     rdx, r8             ; arg1
    mov     r8, r9              ; arg2
    mov     r9, QWORD PTR [rsp + 56 + 8 + 40]  ; arg3 from original stack
    ; arg4 goes to [RSP+32]
    mov     rax, QWORD PTR [rsp + 56 + 8 + 48]
    mov     QWORD PTR [rsp + 32], rax

    call    r10                 ; call vtable method
    ; RAX = return value

    jmp     @@vtdisp_exit

@@vtdisp_fail:
    mov     rax, 80004003h      ; E_POINTER

@@vtdisp_exit:
    add     rsp, 56
    pop     rbx
    ret
asm_dml_vtable_dispatch ENDP


; ============================================================================
; asm_dml_dequant_q4_0_to_fp32 — Dequantize Q4_0 GGUF blocks to FP32
; ============================================================================
; Args:
;   RCX = dest (float* output, must hold blockCount * 32 floats)
;   RDX = src (const uint8_t* Q4_0 block data)
;   R8  = blockCount (number of Q4_0 blocks)
; Returns:
;   RAX = elements dequantized
; Notes:
;   Q4_0 block format:
;     [0..1] = fp16 scale (half)
;     [2..17] = 16 bytes = 32 nibbles (packed 4-bit signed, offset by 8)
;   Dequant: float = (nibble - 8) * scale
; ============================================================================
asm_dml_dequant_q4_0_to_fp32 PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    push    r12
    .pushreg r12
    push    r13
    .pushreg r13
    sub     rsp, 32
    .allocstack 32
    .endprolog

    mov     rdi, rcx        ; dest (float*)
    mov     rsi, rdx        ; src (Q4_0 block data)
    mov     r12, r8         ; blockCount
    xor     r13, r13        ; total elements

    test    rdi, rdi
    jz      @@q4_fail
    test    rsi, rsi
    jz      @@q4_fail
    test    r12, r12
    jz      @@q4_done

@@q4_block_loop:
    ; Read FP16 scale from first 2 bytes of block
    movzx   eax, WORD PTR [rsi]
    ; Convert FP16 → FP32 using vcvtph2ps
    vmovd   xmm0, eax
    vcvtph2ps xmm0, xmm0       ; xmm0 = scale as fp32
    vbroadcastss ymm5, xmm0    ; broadcast scale to all 8 lanes

    ; Process 16 bytes = 32 nibbles (2 nibbles per byte)
    add     rsi, 2              ; skip scale bytes
    xor     ebx, ebx            ; byte index

@@q4_byte_loop:
    movzx   eax, BYTE PTR [rsi + rbx]

    ; Low nibble
    mov     ecx, eax
    and     ecx, 0Fh
    sub     ecx, 8              ; offset: nibble - 8
    vcvtsi2ss xmm1, xmm1, ecx
    vmulss  xmm1, xmm1, xmm0  ; (nibble - 8) * scale
    vmovss  DWORD PTR [rdi], xmm1
    add     rdi, 4

    ; High nibble
    shr     eax, 4
    sub     eax, 8
    vcvtsi2ss xmm1, xmm1, eax
    vmulss  xmm1, xmm1, xmm0
    vmovss  DWORD PTR [rdi], xmm1
    add     rdi, 4

    inc     ebx
    cmp     ebx, 16             ; 16 bytes per block
    jb      @@q4_byte_loop

    add     rsi, 16             ; Advance past data bytes
    add     r13, DML_Q4_BLOCK_SIZE  ; 32 elements per block
    dec     r12
    jnz     @@q4_block_loop

@@q4_done:
    lock add QWORD PTR [g_DMLDequantBlocks], r8
    mov     rax, r13
    add     rsp, 32
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret

@@q4_fail:
    xor     rax, rax
    dec     rax
    add     rsp, 32
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
asm_dml_dequant_q4_0_to_fp32 ENDP


; ============================================================================
; asm_dml_dequant_q8_0_to_fp32 — Dequantize Q8_0 GGUF blocks to FP32
; ============================================================================
; Args:
;   RCX = dest (float* output)
;   RDX = src (const uint8_t* Q8_0 block data)
;   R8  = blockCount
; Returns:
;   RAX = elements dequantized
; Notes:
;   Q8_0 block: [0..1] fp16 scale, [2..33] 32 signed int8 values
;   Dequant: float = int8_val * scale
;   AVX2 vectorized: 32 int8s → 32 fp32s using vpmovsx + vcvtdq2ps + vmulps
; ============================================================================
asm_dml_dequant_q8_0_to_fp32 PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    push    r12
    .pushreg r12
    push    r13
    .pushreg r13
    sub     rsp, 32
    .allocstack 32
    .endprolog

    mov     rdi, rcx
    mov     rsi, rdx
    mov     r12, r8
    xor     r13, r13

    test    rdi, rdi
    jz      @@q8_fail
    test    rsi, rsi
    jz      @@q8_fail
    test    r12, r12
    jz      @@q8_done

@@q8_block_loop:
    ; Read FP16 scale
    movzx   eax, WORD PTR [rsi]
    vmovd   xmm0, eax
    vcvtph2ps xmm0, xmm0
    vbroadcastss ymm5, xmm0        ; scale broadcast

    add     rsi, 2

    ; Process 32 int8 values in 4 groups of 8 using AVX2
    ; Group 1: bytes [0..7]
    vpmovsxbd ymm0, QWORD PTR [rsi]        ; sign-extend 8 bytes → 8 int32s
    vcvtdq2ps ymm0, ymm0                    ; int32 → fp32
    vmulps  ymm0, ymm0, ymm5                ; * scale
    vmovups YMMWORD PTR [rdi], ymm0
    add     rdi, 32

    ; Group 2: bytes [8..15]
    vpmovsxbd ymm0, QWORD PTR [rsi + 8]
    vcvtdq2ps ymm0, ymm0
    vmulps  ymm0, ymm0, ymm5
    vmovups YMMWORD PTR [rdi], ymm0
    add     rdi, 32

    ; Group 3: bytes [16..23]
    vpmovsxbd ymm0, QWORD PTR [rsi + 16]
    vcvtdq2ps ymm0, ymm0
    vmulps  ymm0, ymm0, ymm5
    vmovups YMMWORD PTR [rdi], ymm0
    add     rdi, 32

    ; Group 4: bytes [24..31]
    vpmovsxbd ymm0, QWORD PTR [rsi + 24]
    vcvtdq2ps ymm0, ymm0
    vmulps  ymm0, ymm0, ymm5
    vmovups YMMWORD PTR [rdi], ymm0
    add     rdi, 32

    add     rsi, 32            ; advance past 32 data bytes
    add     r13, DML_Q8_BLOCK_SIZE
    dec     r12
    jnz     @@q8_block_loop

    vzeroupper

@@q8_done:
    lock add QWORD PTR [g_DMLDequantBlocks], r8
    mov     rax, r13
    add     rsp, 32
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret

@@q8_fail:
    xor     rax, rax
    dec     rax
    add     rsp, 32
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
asm_dml_dequant_q8_0_to_fp32 ENDP


; ============================================================================
; asm_dml_rope_rotate_fp32 — Rotary Positional Encoding (FP32 in-place)
; ============================================================================
; Args:
;   RCX = qk (float* — Q or K tensor, interleaved pairs)
;   RDX = cosTable (float* — cos values, seqLen * halfDim)
;   R8  = sinTable (float* — sin values, seqLen * halfDim)
;   R9  = halfDim (uint32 — headDim / 2)
;   [RSP+40] = seqLen (uint32)
; Returns:
;   RAX = 0 success, -1 failure
; Notes:
;   For each position pos and each pair (x0, x1) at indices (2i, 2i+1):
;     out[2i]   = x0 * cos[pos*halfDim+i] - x1 * sin[pos*halfDim+i]
;     out[2i+1] = x0 * sin[pos*halfDim+i] + x1 * cos[pos*halfDim+i]
;   AVX2 vectorized: process 4 pairs (8 floats) per iteration.
; ============================================================================
asm_dml_rope_rotate_fp32 PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    push    r12
    .pushreg r12
    push    r13
    .pushreg r13
    push    r14
    .pushreg r14
    push    r15
    .pushreg r15
    sub     rsp, 40
    .allocstack 40
    .endprolog

    mov     rdi, rcx        ; qk
    mov     rsi, rdx        ; cosTable
    mov     r12, r8         ; sinTable
    mov     r13d, r9d       ; halfDim
    mov     r14d, DWORD PTR [rsp + 40 + 56 + 40]  ; seqLen  (7 pushes * 8 = 56 + allocstack 40)

    test    rdi, rdi
    jz      @@rope_fail
    test    rsi, rsi
    jz      @@rope_fail
    test    r12, r12
    jz      @@rope_fail
    test    r13d, r13d
    jz      @@rope_fail
    test    r14d, r14d
    jz      @@rope_fail

    xor     r15d, r15d      ; pos = 0

@@rope_pos_loop:
    ; Compute table offset for this position
    mov     eax, r15d
    imul    eax, r13d       ; pos * halfDim
    shl     eax, 2          ; * sizeof(float)
    movsxd  rbx, eax        ; table byte offset

    xor     ecx, ecx        ; i = 0 (pair index)

@@rope_pair_loop:
    ; Check if we can do AVX2 (4 pairs = 8 floats at once)
    lea     eax, [ecx + 4]
    cmp     eax, r13d
    ja      @@rope_scalar   ; not enough pairs for AVX2

    ; Load 4 cos and 4 sin values
    vmovups xmm4, XMMWORD PTR [rsi + rbx]     ; cos[i..i+3]
    vmovups xmm5, XMMWORD PTR [r12 + rbx]     ; sin[i..i+3]

    ; Load 4 pairs from qk: [x0,x1, x2,x3, x4,x5, x6,x7]
    ; We need x0,x2,x4,x6 as "even" and x1,x3,x5,x7 as "odd"
    lea     rax, [ecx * 8]     ; pair index * 2 * sizeof(float)
    ; Total qk offset = (pos * headDim + 2*i) * sizeof(float)
    mov     edx, r15d
    imul    edx, r13d
    shl     edx, 1             ; * 2 (headDim = 2 * halfDim)
    add     edx, ecx
    add     edx, ecx           ; +2*i
    shl     edx, 2             ; * sizeof(float)
    movsxd  rdx, edx

    ; Load 8 consecutive floats
    vmovups ymm0, YMMWORD PTR [rdi + rdx]     ; x0,x1,x2,x3,x4,x5,x6,x7

    ; Deinterleave: even = x0,x2,x4,x6 and odd = x1,x3,x5,x7
    ; Use vshufps to extract even/odd
    vshufps ymm1, ymm0, ymm0, 088h    ; even: [0,2,0,2] from each 128-bit lane
    vshufps ymm2, ymm0, ymm0, 0DDh    ; odd: [1,3,1,3] from each 128-bit lane

    ; Load 4 cos/sin values (we process 4 pairs at a time via AVX2)
    vmovups xmm4, XMMWORD PTR [rsi + rbx]   ; cos[i..i+3]
    vmovups xmm5, XMMWORD PTR [r12 + rbx]   ; sin[i..i+3]

    ; Apply rotation: even_out = even * cos - odd * sin
    vmulps  xmm6, xmm1, xmm4      ; even * cos
    vmulps  xmm7, xmm2, xmm5      ; odd * sin
    vsubps  xmm6, xmm6, xmm7      ; even*cos - odd*sin = new_even

    ; odd_out = even * sin + odd * cos
    vmulps  xmm7, xmm1, xmm5      ; even * sin
    vmulps  xmm3, xmm2, xmm4      ; odd * cos
    vaddps  xmm7, xmm7, xmm3      ; even*sin + odd*cos = new_odd

    ; Re-interleave: merge new_even and new_odd back into pairs
    vunpcklps xmm0, xmm6, xmm7    ; [e0,o0,e1,o1]
    vunpckhps xmm1, xmm6, xmm7    ; [e2,o2,e3,o3]
    vmovups XMMWORD PTR [rdi + rdx], xmm0        ; store first 4 floats
    vmovups XMMWORD PTR [rdi + rdx + 16], xmm1   ; store next 4 floats

    add     rbx, 16         ; advance table by 4 * sizeof(float)
    add     ecx, 4          ; advance by 4 pairs
    
    ; Check if we can do another AVX2 batch of 4 pairs
    lea     eax, [ecx + 4]
    cmp     eax, r13d
    jbe     @@rope_avx2_loop_check
    ; Fall through to scalar for remaining elements

@@rope_avx2_loop_check:
    lea     eax, [ecx + 4]
    cmp     eax, r13d
    ja      @@rope_scalar    ; Not enough for AVX2, use scalar
    
    ; Compute next qk offset for AVX2 batch
    mov     eax, r15d
    imul    eax, r13d
    shl     eax, 1
    add     eax, ecx
    add     eax, ecx
    shl     eax, 2
    movsxd  rdx, eax
    vmovups ymm0, YMMWORD PTR [rdi + rdx]
    vshufps ymm1, ymm0, ymm0, 088h
    vshufps ymm2, ymm0, ymm0, 0DDh
    vmovups xmm4, XMMWORD PTR [rsi + rbx]
    vmovups xmm5, XMMWORD PTR [r12 + rbx]
    vmulps  xmm6, xmm1, xmm4
    vmulps  xmm7, xmm2, xmm5
    vsubps  xmm6, xmm6, xmm7
    vmulps  xmm7, xmm1, xmm5
    vmulps  xmm3, xmm2, xmm4
    vaddps  xmm7, xmm7, xmm3
    vunpcklps xmm0, xmm6, xmm7
    vunpckhps xmm1, xmm6, xmm7
    vmovups XMMWORD PTR [rdi + rdx], xmm0
    vmovups XMMWORD PTR [rdi + rdx + 16], xmm1
    add     rbx, 16
    add     ecx, 4
    jmp     @@rope_avx2_loop_check

@@rope_scalar:
    cmp     ecx, r13d
    jae     @@rope_next_pos

    ; Compute qk offset: (pos * 2 * halfDim + 2 * i) * sizeof(float)
    mov     eax, r15d
    imul    eax, r13d
    shl     eax, 1          ; * 2 (headDim)
    lea     eax, [eax + ecx * 2]
    shl     eax, 2          ; * sizeof(float)
    movsxd  rdx, eax

    ; x0 = qk[offset], x1 = qk[offset + 4]
    vmovss  xmm0, DWORD PTR [rdi + rdx]       ; x0
    vmovss  xmm1, DWORD PTR [rdi + rdx + 4]   ; x1

    ; cos_val, sin_val
    vmovss  xmm2, DWORD PTR [rsi + rbx]       ; cos[i]
    vmovss  xmm3, DWORD PTR [r12 + rbx]       ; sin[i]

    ; out[0] = x0 * cos - x1 * sin
    vmulss  xmm4, xmm0, xmm2     ; x0 * cos
    vmulss  xmm5, xmm1, xmm3     ; x1 * sin
    vsubss  xmm4, xmm4, xmm5     ; x0*cos - x1*sin
    vmovss  DWORD PTR [rdi + rdx], xmm4

    ; out[1] = x0 * sin + x1 * cos
    vmulss  xmm4, xmm0, xmm3     ; x0 * sin
    vmulss  xmm5, xmm1, xmm2     ; x1 * cos
    vaddss  xmm4, xmm4, xmm5     ; x0*sin + x1*cos
    vmovss  DWORD PTR [rdi + rdx + 4], xmm4

    add     rbx, 4          ; advance table by sizeof(float)
    inc     ecx
    jmp     @@rope_scalar

@@rope_next_pos:
    inc     r15d
    cmp     r15d, r14d
    jb      @@rope_pos_loop

    lock add QWORD PTR [g_DMLRoPERotations], r14
    vzeroupper
    xor     eax, eax
    add     rsp, 40
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret

@@rope_fail:
    mov     eax, -1
    add     rsp, 40
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
asm_dml_rope_rotate_fp32 ENDP


; ============================================================================
; asm_dml_rope_rotate_fp16 — RoPE for FP16 tensors (converts via FP32)
; ============================================================================
; Args: same as fp32 version but qk is uint16_t* (fp16)
;   RCX = qk (uint16_t* fp16 data, in-place)
;   RDX = cosTable (float*)
;   R8  = sinTable (float*)
;   R9  = seqLen (uint32_t)
;   [RSP+40] = halfDim (uint32_t)
;   [RSP+48] = posOffset (uint32_t)
; Returns: RAX = 0 success, -1 fail
; ============================================================================
asm_dml_rope_rotate_fp16 PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    push    r12
    .pushreg r12
    push    r13
    .pushreg r13
    push    r14
    .pushreg r14
    push    r15
    .pushreg r15
    sub     rsp, 40
    .allocstack 40
    .endprolog

    ; Validate inputs
    test    rcx, rcx
    jz      @@fp16_fail
    test    rdx, rdx
    jz      @@fp16_fail
    test    r8, r8
    jz      @@fp16_fail
    test    r9d, r9d
    jz      @@fp16_fail

    mov     rdi, rcx        ; qk fp16 ptr
    mov     rsi, rdx        ; cosTable ptr
    mov     r12, r8         ; sinTable ptr
    mov     r14d, r9d       ; seqLen
    mov     r13d, DWORD PTR [rsp + 40 + 56]  ; halfDim (7 pushes * 8 = 56)
    mov     r15d, DWORD PTR [rsp + 40 + 64]  ; posOffset (unused for pos init)

    ; headDim = 2 * halfDim
    ; For each position pos in [0, seqLen):
    ;   For each i in [0, halfDim):
    ;     idx = pos * headDim + i*2
    ;     x0 = fp16→fp32(qk[idx]), x1 = fp16→fp32(qk[idx+1])
    ;     qk[idx]   = fp32→fp16(x0*cos[pos*halfDim+i] - x1*sin[pos*halfDim+i])
    ;     qk[idx+1] = fp32→fp16(x0*sin[pos*halfDim+i] + x1*cos[pos*halfDim+i])

    xor     r15d, r15d      ; pos = 0

@@fp16_pos_loop:
    cmp     r15d, r14d
    jae     @@fp16_done

    xor     ecx, ecx        ; i = 0
    ; table offset = pos * halfDim * sizeof(float)
    mov     eax, r15d
    imul    eax, r13d
    shl     eax, 2          ; * sizeof(float)
    movsxd  rbx, eax        ; rbx = table byte offset for this pos

@@fp16_inner:
    cmp     ecx, r13d
    jae     @@fp16_next_pos

    ; Compute qk index: (pos * 2 * halfDim + 2 * i) * sizeof(uint16_t)
    mov     eax, r15d
    imul    eax, r13d
    shl     eax, 1          ; * 2 (headDim = 2 * halfDim)
    lea     eax, [eax + ecx * 2]
    shl     eax, 1          ; * sizeof(uint16_t)
    movsxd  rdx, eax

    ; Load two fp16 values and convert to fp32
    ; vcvtph2ps converts 4 fp16 → 4 fp32, we load 2 fp16 (32 bits)
    movd    xmm6, DWORD PTR [rdi + rdx]   ; load 2 x fp16 (32-bit)
    vcvtph2ps xmm0, xmm6                   ; xmm0 = [x0_fp32, x1_fp32, ?, ?]

    ; Extract x0 and x1
    vmovss  xmm1, xmm0                     ; x0 = xmm1
    vshufps xmm2, xmm0, xmm0, 055h         ; x1 = xmm2

    ; cos_val, sin_val
    vmovss  xmm3, DWORD PTR [rsi + rbx]    ; cos[pos*halfDim + i]
    vmovss  xmm4, DWORD PTR [r12 + rbx]    ; sin[pos*halfDim + i]

    ; out0 = x0 * cos - x1 * sin
    vmulss  xmm5, xmm1, xmm3     ; x0 * cos
    vmulss  xmm6, xmm2, xmm4     ; x1 * sin
    vsubss  xmm5, xmm5, xmm6     ; x0*cos - x1*sin

    ; out1 = x0 * sin + x1 * cos
    vmulss  xmm6, xmm1, xmm4     ; x0 * sin
    vmulss  xmm7, xmm2, xmm3     ; x1 * cos
    vaddss  xmm6, xmm6, xmm7     ; x0*sin + x1*cos

    ; Pack out0 and out1 into xmm5 as [out0, out1, ?, ?]
    vinsertps xmm5, xmm5, xmm6, 010h  ; insert out1 at position 1

    ; Convert fp32 → fp16 and store back
    vcvtps2ph xmm7, xmm5, 0            ; xmm7 = [out0_fp16, out1_fp16, ?, ?]
    movd    DWORD PTR [rdi + rdx], xmm7  ; store 2 x fp16

    add     rbx, 4          ; advance table by sizeof(float)
    inc     ecx
    jmp     @@fp16_inner

@@fp16_next_pos:
    inc     r15d
    jmp     @@fp16_pos_loop

@@fp16_done:
    lock add QWORD PTR [g_DMLRoPERotations], r14
    vzeroupper
    xor     eax, eax

    add     rsp, 40
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret

@@fp16_fail:
    mov     eax, -1
    add     rsp, 40
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
asm_dml_rope_rotate_fp16 ENDP


; ============================================================================
; asm_dml_atomic_inc_dispatch — Atomic increment of dispatch counter
; ============================================================================
; Args: none
; Returns: RAX = new counter value
; ============================================================================
asm_dml_atomic_inc_dispatch PROC
    mov     rax, 1
    lock xadd QWORD PTR [g_DMLDispatchCount], rax
    inc     rax     ; xadd returns OLD value, so +1 for new
    ret
asm_dml_atomic_inc_dispatch ENDP


; ============================================================================
; asm_dml_atomic_add_bytes — Atomic add to byte counter
; ============================================================================
; Args:
;   RCX = 0 for upload counter, 1 for download counter
;   RDX = bytes to add (uint64)
; Returns:
;   RAX = new total
; ============================================================================
asm_dml_atomic_add_bytes PROC
    test    ecx, ecx
    jnz     @@add_download
    lock xadd QWORD PTR [g_DMLBytesUploaded], rdx
    lea     rax, [rdx]      ; new = old + add (xadd stored old in rdx)
    ret
@@add_download:
    lock xadd QWORD PTR [g_DMLBytesDownloaded], rdx
    lea     rax, [rdx]
    ret
asm_dml_atomic_add_bytes ENDP


; ============================================================================
; asm_dml_prefetch_tensor_block — Prefetch tensor data into L1/L2 cache
; ============================================================================
; Args:
;   RCX = address (void* tensor data)
;   RDX = byteCount (prefetch range)
; Returns:
;   RAX = 0
; Notes:
;   Issues prefetchnta for streaming access pattern (tensor upload),
;   prefetcht0 for reuse pattern (weight tensor).
; ============================================================================
asm_dml_prefetch_tensor_block PROC
    test    rcx, rcx
    jz      @@pf_done
    test    rdx, rdx
    jz      @@pf_done

    ; Prefetch in 64-byte cache line strides
    xor     rax, rax

@@pf_loop:
    prefetcht0 [rcx + rax]
    add     rax, DML_CACHE_LINE_SIZE
    cmp     rax, rdx
    jb      @@pf_loop

@@pf_done:
    xor     eax, eax
    ret
asm_dml_prefetch_tensor_block ENDP


; ============================================================================
; asm_dml_compute_op_hash — FNV-1a hash for operator cache keys
; ============================================================================
; Args:
;   RCX = string (const char* operator name)
;   RDX = params (const uint32_t* parameter array)
;   R8  = paramCount (uint32)
; Returns:
;   RAX = 64-bit hash
; ============================================================================
asm_dml_compute_op_hash PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    sub     rsp, 24
    .allocstack 24
    .endprolog

    mov     rsi, rcx        ; string
    mov     rbx, rdx        ; params
    ; R8 = paramCount (already there)

    ; FNV-1a offset basis
    mov     rax, FNV1A_OFFSET

    ; Hash the string
    test    rsi, rsi
    jz      @@hash_params

@@hash_str_loop:
    movzx   ecx, BYTE PTR [rsi]
    test    ecx, ecx
    jz      @@hash_params
    xor     rax, rcx
    imul    rax, FNV1A_PRIME
    inc     rsi
    jmp     @@hash_str_loop

@@hash_params:
    ; Hash the uint32 params
    test    rbx, rbx
    jz      @@hash_done
    test    r8d, r8d
    jz      @@hash_done

    xor     ecx, ecx        ; param index

@@hash_param_loop:
    mov     edx, DWORD PTR [rbx + rcx * 4]
    ; Mix param into hash
    xor     rax, rdx
    imul    rax, FNV1A_PRIME
    ; Rotate hash for better distribution
    rol     rax, 13
    inc     ecx
    cmp     ecx, r8d
    jb      @@hash_param_loop

@@hash_done:
    add     rsp, 24
    pop     rsi
    pop     rbx
    ret
asm_dml_compute_op_hash ENDP

END
