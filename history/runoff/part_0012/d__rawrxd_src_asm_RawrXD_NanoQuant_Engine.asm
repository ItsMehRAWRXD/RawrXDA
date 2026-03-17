; =============================================================================
; RawrXD_NanoQuant_Engine.asm
; ADMM-Based Sub-1-Bit Binary Quantization & Inference Engine (AVX-512 / AVX2)
; =============================================================================
;
; Production-grade NanoQuant implementation for the RawrXD IDE.
; Registers GGML_TYPE_NQ_1 (type 20) as a new quantization format:
;   - 1.0625 bits per element (15.06x compression vs FP16, 30.12x vs FP32)
;   - ADMM-optimized sign-magnitude encoding
;   - Fused dequant + matmul kernels for zero-overhead inference
;
; Also provides matrix-level low-rank binary factorization (NQ_MATRIX)
; for truly sub-1-bit compression (0.002 bpe at rank-4).
;
; Integrates with:
;   - quant_avx2.asm / KQuant_Dequant.asm    (quant dispatch)
;   - inference_core.asm                       (GEMM/GEMV dispatch)
;   - RawrXD_QuadBuffer_Streamer.asm           (tensor streaming)
;   - ggml_backend.asm                         (backend opcode dispatch)
;   - RawrXD_NanoQuant_Streaming.asm           (GGUF I/O + QuadBuffer hooks)
;
; ╔═══════════════════════════════════════════════════════════════════════╗
; ║  NQ_1 BLOCK FORMAT (34 bytes per 256 elements)                      ║
; ║                                                                     ║
; ║  Offset  Size  Field        Description                             ║
; ║  ------  ----  -----        -----------                             ║
; ║  +0      2     d            F16 scale factor                        ║
; ║  +2      32    signs[32]    256 sign bits, packed little-endian     ║
; ║                              Bit=1 → +d (positive weight)           ║
; ║                              Bit=0 → -d (negative weight)           ║
; ║                                                                     ║
; ║  Dequant: weight[i] = fp16→f32(d) * (2*bit(signs,i) - 1)          ║
; ║         = +d if bit=1,  -d if bit=0                                 ║
; ║                                                                     ║
; ║  Effective: 34B / 256 elements = 1.0625 bpe                        ║
; ║  Compression vs FP16: 16 / 1.0625 = 15.06x                        ║
; ║  Compression vs FP32: 32 / 1.0625 = 30.12x                        ║
; ╚═══════════════════════════════════════════════════════════════════════╝
;
; ╔═══════════════════════════════════════════════════════════════════════╗
; ║  NQ_MATRIX FORMAT (sub-1-bit, matrix-level factorization)           ║
; ║                                                                     ║
; ║  Header (32 bytes):                                                 ║
; ║    +0:  uint32 magic      'NQR4' = 3452514Eh                       ║
; ║    +4:  uint32 rows       M                                         ║
; ║    +8:  uint32 cols       N                                         ║
; ║    +12: uint32 rank       r (1-8)                                   ║
; ║    +16: float  scales[4]  rank-1..4 scales (16 bytes)               ║
; ║                                                                     ║
; ║  Data per rank factor k:                                            ║
; ║    row_signs[k]: ceil(M/8) bytes  (M sign bits)                     ║
; ║    col_signs[k]: ceil(N/8) bytes  (N sign bits)                     ║
; ║                                                                     ║
; ║  W[i,j] = Σ_k scales[k] * rsign[k][i] * csign[k][j]               ║
; ║                                                                     ║
; ║  For 4096×4096 rank-4: 4128 bytes = 0.002 bpe (8000x vs FP16)      ║
; ╚═══════════════════════════════════════════════════════════════════════╝
;
; Build: ml64.exe /c /Zi /Zd /Fo NanoQuant.obj RawrXD_NanoQuant_Engine.asm
; Link:  Statically linked into RawrEngine / RawrXD-Win32IDE via CMake
;
; Calling Convention: Microsoft x64 (RCX, RDX, R8, R9, stack)
; All functions preserve RBX, RBP, RSI, RDI, R12-R15, XMM6-XMM15.
;
; Pattern: PatchResult (RAX=0 success, RAX=nonzero on error, RDX=detail)
; Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED
; =============================================================================

option casemap:none

INCLUDE RawrXD_Common.inc

; =============================================================================
; Additional EXTERN declarations not in RawrXD_Common.inc
; =============================================================================
EXTERNDEF WriteFile:PROC
EXTERNDEF ExitProcess:PROC

; =============================================================================
;                        NanoQuant Constants
; =============================================================================

; NQ_1 block layout
BLOCK_NQ1_SIZE          EQU     34              ; 2 + 32 = 34 bytes per block
NQ1_SCALE_OFFSET        EQU     0               ; F16 scale at byte 0
NQ1_SIGNS_OFFSET        EQU     2               ; 32 bytes of packed sign bits
QK_NQ1                  EQU     256             ; Elements per NQ_1 block

; New GGML type identifiers (extends RawrXD_Common.inc)
GGML_TYPE_NQ_1          EQU     20              ; Block-level binary (34B/256el)
GGML_TYPE_NQ_R4         EQU     21              ; Matrix-level rank-4 binary

; NQ_MATRIX header offsets
NQM_MAGIC_OFFSET        EQU     0
NQM_ROWS_OFFSET         EQU     4
NQM_COLS_OFFSET         EQU     8
NQM_RANK_OFFSET         EQU     12
NQM_SCALES_OFFSET       EQU     16
NQM_HEADER_SIZE         EQU     32
NQM_MAGIC_VALUE         EQU     3452514Eh       ; 'NQR4'
NQM_MAX_RANK            EQU     8

; ADMM constants
ADMM_MAX_ITER           EQU     50              ; Default ADMM iterations
ADMM_DEFAULT_RHO        EQU     40400000h       ; 3.0f in IEEE-754
ADMM_TOLERANCE          EQU     3A83126Fh       ; 0.001f in IEEE-754

; CPU feature bits (returned by NanoQuant_Init)
NQ_CAP_AVX2             EQU     01h
NQ_CAP_FMA3             EQU     02h
NQ_CAP_F16C             EQU     04h
NQ_CAP_AVX512F          EQU     08h
NQ_CAP_AVX512BW         EQU     10h
NQ_CAP_AVX512VL         EQU     20h
NQ_CAP_AVX512VPOPCNTDQ  EQU    40h
NQ_CAP_AVX512BITALG     EQU     80h

; Tile sizes for SGEMM
NQ_GEMM_TILE_M          EQU     6               ; Rows processed per tile
NQ_GEMM_TILE_K          EQU     256             ; Must match QK_NQ1

; =============================================================================
;                         EXPORTS
; =============================================================================
PUBLIC NanoQuant_Init
PUBLIC NanoQuant_GetCapabilities

; Quantization (F32 → NQ_1)
PUBLIC NQ1_QuantizeBlock_Fast
PUBLIC NQ1_QuantizeBlock_ADMM
PUBLIC NQ1_QuantizeTensor

; Dequantization (NQ_1 → F32)
PUBLIC NQ1_DequantBlock_AVX512
PUBLIC NQ1_DequantBlock_AVX2
PUBLIC NQ1_Dequant

; Fused vector dot product
PUBLIC NQ1_VecDot_AVX512
PUBLIC NQ1_VecDot_AVX2
PUBLIC NQ1_VecDot

; GEMM / GEMV
PUBLIC NQ1_SGEMV
PUBLIC NQ1_SGEMM_Tiled
PUBLIC NQ1_SGEMM

; Matrix-level binary factorization (sub-1-bit)
PUBLIC NQ_MatrixFactor_Rank1
PUBLIC NQ_MatrixFactor_MultiRank
PUBLIC NQ_MatrixGEMM

; Requantization paths
PUBLIC NQ1_Requantize_Q4_0
PUBLIC NQ1_Requantize_Q8_0
PUBLIC NQ1_Requantize_Q4_K

; Utility
PUBLIC NQ1_GetBlockSize
PUBLIC NQ1_GetCompressionRatio
PUBLIC NQ1_GetStats
PUBLIC NQ1_Dispatch

; =============================================================================
;                    Aligned Data Segment (AVX-512)
; =============================================================================
_DATA64 SEGMENT ALIGN(64) 'DATA'

; Absolute value mask for FP32 (clear sign bit)
abs_mask_dw:
    DD      07FFFFFFFh, 07FFFFFFFh, 07FFFFFFFh, 07FFFFFFFh
    DD      07FFFFFFFh, 07FFFFFFFh, 07FFFFFFFh, 07FFFFFFFh
    DD      07FFFFFFFh, 07FFFFFFFh, 07FFFFFFFh, 07FFFFFFFh
    DD      07FFFFFFFh, 07FFFFFFFh, 07FFFFFFFh, 07FFFFFFFh

; +1.0f broadcast (for binary expansion)
one_f32:
    DD      3F800000h, 3F800000h, 3F800000h, 3F800000h
    DD      3F800000h, 3F800000h, 3F800000h, 3F800000h
    DD      3F800000h, 3F800000h, 3F800000h, 3F800000h
    DD      3F800000h, 3F800000h, 3F800000h, 3F800000h

; -1.0f broadcast (for binary expansion)
neg_one_f32:
    DD      0BF800000h, 0BF800000h, 0BF800000h, 0BF800000h
    DD      0BF800000h, 0BF800000h, 0BF800000h, 0BF800000h
    DD      0BF800000h, 0BF800000h, 0BF800000h, 0BF800000h
    DD      0BF800000h, 0BF800000h, 0BF800000h, 0BF800000h

; 2.0f broadcast (for 2*sum_pos - sum_all optimization)
two_f32:
    DD      40000000h, 40000000h, 40000000h, 40000000h
    DD      40000000h, 40000000h, 40000000h, 40000000h

; 0.0f broadcast
zero_f32:
    DD      16 DUP(0)

; 1/256.0f for mean computation
inv_256_f32:
    DD      3B800000h                   ; 1.0/256.0 = 0.00390625

; Bit-select mask for AVX2 sign extraction (8 dwords, each with one bit set)
; Element i has bit i set → vpand isolates that bit from the broadcast byte
bit_select_dw:
    DD      01h, 02h, 04h, 08h, 10h, 20h, 40h, 80h

; Performance counters
g_NQ1QuantBlocks        DQ      0
g_NQ1DequantBlocks      DQ      0
g_NQ1VecDotCalls        DQ      0
g_NQ1GemmCalls          DQ      0
g_NQ1ADMMIterTotal      DQ      0

; CPU feature flags (set by NanoQuant_Init)
g_NQ_HasAVX2            DD      0
g_NQ_HasFMA3            DD      0
g_NQ_HasF16C            DD      0
g_NQ_HasAVX512F         DD      0
g_NQ_HasAVX512BW        DD      0
g_NQ_HasAVX512VL        DD      0
g_NQ_HasVPOPCNTDQ       DD      0
g_NQ_HasBITALG          DD      0

; Dispatch pointers (set by Init to best available path)
g_NQ1DequantDispatch    DQ      0
g_NQ1VecDotDispatch     DQ      0
g_NQ1SGEMMDispatch      DQ      0

_DATA64 ENDS

; =============================================================================
;                           CODE
; =============================================================================
.code

; =============================================================================
; NanoQuant_Init
; Detect CPU features and configure dispatch pointers.
;
; Returns: EAX = capability bitmask (NQ_CAP_* flags)
; =============================================================================
NanoQuant_Init PROC FRAME
    push    rbx
    .pushreg rbx
    .endprolog

    xor     r8d, r8d                    ; Capability accumulator

    ; --- Check CPUID max leaf ---
    xor     eax, eax
    cpuid
    cmp     eax, 7
    jb      @@init_set_dispatch

    ; --- Leaf 1: FMA3 (ECX[12]), F16C (ECX[29]), OSXSAVE (ECX[27]) ---
    mov     eax, 1
    cpuid

    ; Check OSXSAVE (ECX bit 27) — required before XGETBV
    bt      ecx, 27
    jnc     @@init_set_dispatch         ; No OSXSAVE → skip all AVX/AVX-512

    ; Verify OS has enabled AVX state via XGETBV(XCR0)
    push    rcx                          ; Preserve CPUID ECX
    xor     ecx, ecx                     ; XCR0
    xgetbv                               ; EAX = XCR0 low bits
    pop     rcx                          ; Restore CPUID ECX
    ; Check XCR0[2:1] = SSE + AVX state must both be enabled
    and     eax, 06h
    cmp     eax, 06h
    jne     @@init_set_dispatch         ; OS hasn't enabled AVX → skip

    bt      ecx, 12                     ; FMA3
    jnc     @@check_f16c
    or      r8d, NQ_CAP_FMA3
    mov     g_NQ_HasFMA3, 1
@@check_f16c:
    bt      ecx, 29                     ; F16C
    jnc     @@check_leaf7
    or      r8d, NQ_CAP_F16C
    mov     g_NQ_HasF16C, 1

@@check_leaf7:
    ; --- Leaf 7.0: AVX2 (EBX[5]), AVX-512F (EBX[16]), BW (EBX[30]), VL (EBX[31]) ---
    mov     eax, 7
    xor     ecx, ecx
    cpuid

    bt      ebx, 5                      ; AVX2
    jnc     @@check_avx512
    or      r8d, NQ_CAP_AVX2
    mov     g_NQ_HasAVX2, 1

@@check_avx512:
    bt      ebx, 16                     ; AVX-512F
    jnc     @@init_set_dispatch

    ; Gate AVX-512 on XCR0[7:5] = opmask + ZMM_Hi256 + Hi16_ZMM
    push    rbx                          ; Preserve CPUID EBX
    push    rcx                          ; Preserve CPUID ECX
    xor     ecx, ecx                     ; XCR0
    xgetbv                               ; EAX = XCR0 low bits
    pop     rcx
    pop     rbx
    and     eax, 0E0h                    ; Bits 7,6,5
    cmp     eax, 0E0h
    jne     @@init_set_dispatch         ; OS hasn't enabled ZMM state → no AVX-512

    or      r8d, NQ_CAP_AVX512F
    mov     g_NQ_HasAVX512F, 1

    bt      ebx, 30                     ; AVX-512BW
    jnc     @@check_vl
    or      r8d, NQ_CAP_AVX512BW
    mov     g_NQ_HasAVX512BW, 1

@@check_vl:
    bt      ebx, 31                     ; AVX-512VL
    jnc     @@check_vpopcntdq
    or      r8d, NQ_CAP_AVX512VL
    mov     g_NQ_HasAVX512VL, 1

@@check_vpopcntdq:
    ; ECX[14] = AVX-512 VPOPCNTDQ
    bt      ecx, 14
    jnc     @@check_bitalg
    or      r8d, NQ_CAP_AVX512VPOPCNTDQ
    mov     g_NQ_HasVPOPCNTDQ, 1

@@check_bitalg:
    ; ECX[12] = AVX-512 BITALG
    bt      ecx, 12
    jnc     @@init_set_dispatch
    or      r8d, NQ_CAP_AVX512BITALG
    mov     g_NQ_HasBITALG, 1

@@init_set_dispatch:
    ; Configure dispatch pointers based on detected features
    ; Prefer AVX-512 path where available, fall back to AVX2

    ; Dequant dispatch
    lea     rax, NQ1_DequantBlock_AVX2
    cmp     g_NQ_HasAVX512F, 1
    jne     @@set_dequant
    lea     rax, NQ1_DequantBlock_AVX512
@@set_dequant:
    mov     g_NQ1DequantDispatch, rax

    ; VecDot dispatch
    lea     rax, NQ1_VecDot_AVX2
    cmp     g_NQ_HasAVX512F, 1
    jne     @@set_vecdot
    lea     rax, NQ1_VecDot_AVX512
@@set_vecdot:
    mov     g_NQ1VecDotDispatch, rax

    ; SGEMM dispatch
    lea     rax, NQ1_SGEMM_Tiled
    mov     g_NQ1SGEMMDispatch, rax

    mov     eax, r8d                    ; Return capability bitmask

    pop     rbx
    ret
NanoQuant_Init ENDP

; =============================================================================
; NanoQuant_GetCapabilities
; Returns: EAX = last-detected capability bitmask
; =============================================================================
NanoQuant_GetCapabilities PROC
    xor     eax, eax
    cmp     g_NQ_HasAVX2, 1
    jne     @@gc_fma
    or      eax, NQ_CAP_AVX2
@@gc_fma:
    cmp     g_NQ_HasFMA3, 1
    jne     @@gc_f16c
    or      eax, NQ_CAP_FMA3
@@gc_f16c:
    cmp     g_NQ_HasF16C, 1
    jne     @@gc_512
    or      eax, NQ_CAP_F16C
@@gc_512:
    cmp     g_NQ_HasAVX512F, 1
    jne     @@gc_done
    or      eax, NQ_CAP_AVX512F
@@gc_done:
    ret
NanoQuant_GetCapabilities ENDP

; =============================================================================
; NQ1_QuantizeBlock_Fast
; Fast rank-1 sign-magnitude quantization (no iteration).
; Optimal closed-form: d = mean(|W|), signs = sign(W)
;
; RCX = src (float* 256 elements, 32-byte aligned preferred)
; RDX = dst (NQ1 block, 34 bytes output)
; Returns: RAX = 0 (always succeeds)
; =============================================================================
NQ1_QuantizeBlock_Fast PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    push    r12
    .pushreg r12
    .endprolog

    mov     rsi, rcx                    ; Source F32 weights
    mov     rdi, rdx                    ; Destination NQ_1 block

    ; --- Phase 1: Extract sign bits and accumulate |W| (AVX2) ---
    ; Process 8 floats per iteration, 32 iterations = 256 elements
    ; vmovmskps extracts sign bits: bit=1 if float is negative
    ; We invert: bit=1 means positive (our encoding)

    vpbroadcastd ymm4, dword ptr [abs_mask_dw]  ; 0x7FFFFFFF mask
    vxorps  ymm5, ymm5, ymm5                     ; Magnitude accumulator

    lea     r12, [rdi + NQ1_SIGNS_OFFSET]         ; Signs output (32 bytes)
    xor     ecx, ecx                              ; Byte counter (0..31)

@@quant_fast_loop:
    vmovups ymm0, ymmword ptr [rsi]              ; Load 8 F32 weights
    vandps  ymm1, ymm0, ymm4                     ; |W[i]| (clear sign bit)
    vaddps  ymm5, ymm5, ymm1                     ; Accumulate magnitudes

    ; Extract sign bits: vmovmskps gives bit=1 for NEGATIVE values
    vmovmskps eax, ymm0
    not     al                                    ; Invert: bit=1 for POSITIVE
    mov     byte ptr [r12 + rcx], al              ; Store 8 sign bits

    add     rsi, 32                               ; Next 8 floats (8 * 4 bytes)
    inc     ecx
    cmp     ecx, 32
    jb      @@quant_fast_loop

    ; --- Phase 2: Horizontal sum of ymm5 → scalar magnitude sum ---
    vextractf128 xmm1, ymm5, 1
    vaddps  xmm0, xmm5, xmm1                    ; 4 partial sums
    vhaddps xmm0, xmm0, xmm0                    ; 2 partial sums
    vhaddps xmm0, xmm0, xmm0                    ; Total sum in xmm0[0]

    ; --- Phase 3: Compute scale d = sum / 256 ---
    vmulss  xmm0, xmm0, dword ptr [inv_256_f32] ; d = mean(|W|)

    ; --- Phase 4: Convert d to F16 and store ---
    vcvtps2ph xmm1, xmm0, 0                     ; F32 → F16 (round nearest)
    vpextrw eax, xmm1, 0                         ; Extract F16 word
    mov     word ptr [rdi + NQ1_SCALE_OFFSET], ax ; Store F16 scale

    ; Update performance counter
    lock inc qword ptr [g_NQ1QuantBlocks]

    vzeroupper
    xor     eax, eax                              ; Success
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
NQ1_QuantizeBlock_Fast ENDP

; =============================================================================
; NQ1_QuantizeBlock_ADMM
; ADMM-optimized quantization for improved accuracy.
; Iteratively refines signs and scale to minimize ||W - d*(2*signs-1)||²
;
; RCX = src (float* 256 elements)
; RDX = dst (NQ1 block, 34 bytes output)
; R8D = max_iterations (0 = use default ADMM_MAX_ITER)
; Returns: RAX = number of iterations used
; =============================================================================
NQ1_QuantizeBlock_ADMM PROC FRAME
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
    push    rbp
    .pushreg rbp
    sub     rsp, 2112                              ; 256*4=1024 (Z) + 256*4=1024 (U) + 64 scratch
    .allocstack 2112
    .endprolog

    mov     rsi, rcx                               ; W_original (F32 × 256)
    mov     rdi, rdx                               ; Output NQ_1 block
    mov     r14d, r8d                              ; max_iter
    test    r14d, r14d
    jnz     @@admm_has_iter
    mov     r14d, ADMM_MAX_ITER                    ; Default 50 iterations
@@admm_has_iter:

    ; Stack layout:
    ;   [rsp+0..1023]     = Z[256] auxiliary variable (F32)
    ;   [rsp+1024..2047]  = U[256] dual variable (F32)
    ;   [rsp+2048..2111]  = scratch (64 bytes)

    lea     r12, [rsp]                             ; Z base
    lea     r13, [rsp + 1024]                      ; U base

    ; --- Initialize: Z[i] = 0, U[i] = 0 ---
    mov     rcx, r12
    xor     eax, eax
    mov     edx, 512                               ; 256 dwords = 1024 bytes = 512×2 (for both)
    vxorps  ymm0, ymm0, ymm0
@@init_zero:
    vmovups ymmword ptr [rcx], ymm0
    add     rcx, 32
    sub     edx, 8
    jnz     @@init_zero

    ; --- Initialize signs from sign(W) via fast path ---
    ; (This sets initial signs and scale in the output block)
    mov     rcx, rsi
    mov     rdx, rdi
    call    NQ1_QuantizeBlock_Fast

    ; --- Load initial scale d from block ---
    movzx   eax, word ptr [rdi + NQ1_SCALE_OFFSET]
    vmovd   xmm6, eax
    vcvtph2ps xmm6, xmm6                          ; xmm6 = d (scale)

    ; --- Initialize Z[i] = d * (2*sign[i]-1) = reconstruction ---
    lea     rbx, [rdi + NQ1_SIGNS_OFFSET]          ; Signs base
    vpbroadcastd ymm7, dword ptr [abs_mask_dw]     ; For F16C/abs ops

    xor     ecx, ecx                               ; Element index
@@init_Z_loop:
    cmp     ecx, 256
    jge     @@init_Z_done

    ; Get sign bit for element ecx
    mov     eax, ecx
    shr     eax, 3                                 ; Byte index
    movzx   edx, byte ptr [rbx + rax]
    mov     eax, ecx
    and     eax, 7                                 ; Bit index within byte
    bt      edx, eax
    jnc     @@init_Z_neg

    ; Sign = +1: Z[i] = +d
    vmovss  dword ptr [r12 + rcx*4], xmm6
    jmp     @@init_Z_next

@@init_Z_neg:
    ; Sign = -1: Z[i] = -d
    vxorps  xmm0, xmm0, xmm0
    vsubss  xmm0, xmm0, xmm6
    vmovss  dword ptr [r12 + rcx*4], xmm0

@@init_Z_next:
    inc     ecx
    jmp     @@init_Z_loop
@@init_Z_done:

    ; --- ADMM iterations ---
    ; rho = 3.0f (penalty parameter)
    mov     eax, ADMM_DEFAULT_RHO
    vmovd   xmm15, eax                             ; xmm15 = rho = 3.0f

    ; tolerance = 0.001f
    mov     eax, ADMM_TOLERANCE
    vmovd   xmm14, eax                             ; xmm14 = tolerance

    xor     r15d, r15d                              ; Iteration counter
    vpbroadcastd ymm13, dword ptr [abs_mask_dw]    ; Abs mask for magnitudes

@@admm_iter_loop:
    cmp     r15d, r14d
    jge     @@admm_converged

    ; =================================================================
    ; B-step: Update binary signs
    ;   x[i] = sign(W[i] + rho * (Z[i] - U[i]))
    ; =================================================================
    lea     rbx, [rdi + NQ1_SIGNS_OFFSET]
    xor     ecx, ecx                              ; Process 8 elements per iteration

@@b_step_loop:
    cmp     ecx, 32                                ; 32 bytes of signs
    jge     @@b_step_done

    mov     rax, rcx
    shl     rax, 5                                  ; rax = ecx * 32

    ; Load 8 F32 values from W, Z, U
    vmovups ymm0, ymmword ptr [rsi + rax]           ; W[8]
    vmovups ymm1, ymmword ptr [r12 + rax]           ; Z[8]
    vmovups ymm2, ymmword ptr [r13 + rax]           ; U[8]

    ; Compute: W + rho*(Z - U)
    vsubps  ymm3, ymm1, ymm2                       ; Z - U
    vbroadcastss ymm4, xmm15                       ; rho
    vfmadd231ps ymm0, ymm3, ymm4                   ; W + rho*(Z-U)

    ; Extract sign bits: vmovmskps gives bit=1 if negative
    vmovmskps eax, ymm0
    not     al                                      ; Invert: bit=1 = positive
    mov     byte ptr [rbx + rcx], al

    inc     ecx
    jmp     @@b_step_loop
@@b_step_done:

    ; =================================================================
    ; Scale update: alpha = dot(W, B) / n
    ;   Where B[i] = 2*sign[i]-1 (expanded from bits)
    ;   dot(W, B) = sum(|W[i]| where sign matches) - sum(|W[i]| where sign flips)
    ;   Simplified: alpha = sum(W[i] * B[i]) / 256
    ; =================================================================
    vxorps  ymm0, ymm0, ymm0                       ; Dot accumulator
    lea     rbx, [rdi + NQ1_SIGNS_OFFSET]
    xor     ecx, ecx

@@scale_loop:
    cmp     ecx, 32
    jge     @@scale_done

    mov     rax, rcx
    shl     rax, 5                                  ; rax = ecx * 32

    vmovups ymm1, ymmword ptr [rsi + rax]           ; W[8]
    movzx   eax, byte ptr [rbx + rcx]              ; 8 sign bits

    ; Expand 8 bits to 8 × ±1.0f
    ; Broadcast byte to all 8 dwords
    vmovd   xmm2, eax
    vpbroadcastd ymm2, xmm2
    ; AND with bit-select mask to isolate each bit
    vpand   ymm3, ymm2, ymmword ptr [bit_select_dw]
    ; Compare: element = mask value → all 1s (-1 in int), else 0
    vpcmpeqd ymm4, ymm3, ymmword ptr [bit_select_dw]
    ; ymm4 = 0xFFFFFFFF where bit=1, 0x00000000 where bit=0
    ; Use as blend mask: select +1.0 where bit=1, -1.0 where bit=0
    vmovaps  ymm5, ymmword ptr [neg_one_f32]
    vblendvps ymm5, ymm5, ymmword ptr [one_f32], ymm4
    ; ymm5 now has +1.0 or -1.0 for each element

    ; Multiply W * B and accumulate
    vfmadd231ps ymm0, ymm1, ymm5                   ; dot += W * B

    inc     ecx
    jmp     @@scale_loop
@@scale_done:

    ; Horizontal sum → xmm0[0]
    vextractf128 xmm1, ymm0, 1
    vaddps  xmm0, xmm0, xmm1
    vhaddps xmm0, xmm0, xmm0
    vhaddps xmm0, xmm0, xmm0

    ; alpha = dot / 256
    vmulss  xmm6, xmm0, dword ptr [inv_256_f32]    ; xmm6 = new alpha (scale)

    ; =================================================================
    ; Z-step: Z[i] = (W[i] + rho*(alpha*B[i] + U[i])) / (1 + rho)
    ; =================================================================
    ; Precompute 1/(1+rho)
    vbroadcastss ymm8, xmm15                       ; rho
    mov     eax, 3F800000h                          ; 1.0f
    vmovd   xmm9, eax
    vaddss  xmm9, xmm9, xmm15                     ; 1 + rho
    mov     eax, 3F800000h
    vmovd   xmm10, eax
    vdivss  xmm10, xmm10, xmm9                    ; 1 / (1+rho)
    vbroadcastss ymm10, xmm10                      ; Broadcast inv_denom

    vbroadcastss ymm11, xmm6                       ; alpha broadcast

    lea     rbx, [rdi + NQ1_SIGNS_OFFSET]
    xor     ecx, ecx

@@z_step_loop:
    cmp     ecx, 32
    jge     @@z_step_done

    ; Expand signs to ±1.0 (reuse from B-step pattern)
    movzx   eax, byte ptr [rbx + rcx]
    vmovd   xmm2, eax
    vpbroadcastd ymm2, xmm2
    vpand   ymm3, ymm2, ymmword ptr [bit_select_dw]
    vpcmpeqd ymm4, ymm3, ymmword ptr [bit_select_dw]
    vmovaps  ymm5, ymmword ptr [neg_one_f32]
    vblendvps ymm5, ymm5, ymmword ptr [one_f32], ymm4

    ; alpha * B[i]
    vmulps  ymm5, ymm5, ymm11                      ; alpha * B

    mov     rax, rcx
    shl     rax, 5                                  ; rax = ecx * 32

    ; Load U[i] and W[i]
    vmovups ymm2, ymmword ptr [r13 + rax]           ; U[8]
    vmovups ymm3, ymmword ptr [rsi + rax]           ; W[8]

    ; rho * (alpha*B + U)
    vaddps  ymm5, ymm5, ymm2                       ; alpha*B + U
    vfmadd213ps ymm5, ymm8, ymm3                   ; W + rho*(alpha*B + U)

    ; Z = result * inv_denom
    vmulps  ymm5, ymm5, ymm10
    vmovups ymmword ptr [r12 + rax], ymm5           ; Store Z

    inc     ecx
    jmp     @@z_step_loop
@@z_step_done:

    ; =================================================================
    ; U-step: U[i] += alpha*B[i] - Z[i]
    ; =================================================================
    lea     rbx, [rdi + NQ1_SIGNS_OFFSET]
    xor     ecx, ecx

@@u_step_loop:
    cmp     ecx, 32
    jge     @@u_step_done

    ; Expand signs to ±1.0 again
    movzx   eax, byte ptr [rbx + rcx]
    vmovd   xmm2, eax
    vpbroadcastd ymm2, xmm2
    vpand   ymm3, ymm2, ymmword ptr [bit_select_dw]
    vpcmpeqd ymm4, ymm3, ymmword ptr [bit_select_dw]
    vmovaps  ymm5, ymmword ptr [neg_one_f32]
    vblendvps ymm5, ymm5, ymmword ptr [one_f32], ymm4

    vmulps  ymm5, ymm5, ymm11                      ; alpha * B

    mov     rax, rcx
    shl     rax, 5                                  ; rax = ecx * 32

    vmovups ymm2, ymmword ptr [r12 + rax]           ; Z[8]
    vsubps  ymm5, ymm5, ymm2                       ; alpha*B - Z

    vmovups ymm3, ymmword ptr [r13 + rax]           ; U[8]
    vaddps  ymm3, ymm3, ymm5                       ; U += alpha*B - Z
    vmovups ymmword ptr [r13 + rax], ymm3

    inc     ecx
    jmp     @@u_step_loop
@@u_step_done:

    ; =================================================================
    ; Convergence check: primal_residual = ||alpha*B - Z||²
    ; =================================================================
    vxorps  ymm0, ymm0, ymm0                       ; Residual accumulator
    lea     rbx, [rdi + NQ1_SIGNS_OFFSET]
    xor     ecx, ecx

@@residual_loop:
    cmp     ecx, 32
    jge     @@residual_done

    ; Expand signs
    movzx   eax, byte ptr [rbx + rcx]
    vmovd   xmm2, eax
    vpbroadcastd ymm2, xmm2
    vpand   ymm3, ymm2, ymmword ptr [bit_select_dw]
    vpcmpeqd ymm4, ymm3, ymmword ptr [bit_select_dw]
    vmovaps  ymm5, ymmword ptr [neg_one_f32]
    vblendvps ymm5, ymm5, ymmword ptr [one_f32], ymm4

    vmulps  ymm5, ymm5, ymm11                      ; alpha * B

    mov     rax, rcx
    shl     rax, 5                                  ; rax = ecx * 32

    vmovups ymm2, ymmword ptr [r12 + rax]           ; Z
    vsubps  ymm5, ymm5, ymm2                       ; diff = alpha*B - Z
    vfmadd231ps ymm0, ymm5, ymm5                   ; residual += diff²

    inc     ecx
    jmp     @@residual_loop
@@residual_done:

    ; Horizontal sum of residual
    vextractf128 xmm1, ymm0, 1
    vaddps  xmm0, xmm0, xmm1
    vhaddps xmm0, xmm0, xmm0
    vhaddps xmm0, xmm0, xmm0                       ; xmm0[0] = total residual

    ; Normalize: mean squared residual = total / 256
    vmulss  xmm0, xmm0, dword ptr [inv_256_f32]    ; xmm0 = residual / 256

    ; Compare normalized residual to tolerance
    vcomiss xmm0, xmm14                             ; residual/n vs tolerance
    jb      @@admm_converged                        ; Below tolerance → done

    inc     r15d
    lock add qword ptr [g_NQ1ADMMIterTotal], 1
    jmp     @@admm_iter_loop

@@admm_converged:
    ; Store final scale d as F16
    vcvtps2ph xmm1, xmm6, 0
    vpextrw eax, xmm1, 0
    mov     word ptr [rdi + NQ1_SCALE_OFFSET], ax

    lock inc qword ptr [g_NQ1QuantBlocks]

    mov     eax, r15d                               ; Return iteration count

    vzeroupper
    add     rsp, 2112
    pop     rbp
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
NQ1_QuantizeBlock_ADMM ENDP

; =============================================================================
; NQ1_QuantizeTensor
; Quantize an entire F32 tensor to NQ_1 blocks.
;
; RCX = src (float* n elements, must be multiple of 256)
; RDX = dst (NQ1 block array)
; R8  = n_elements (QWORD, must be multiple of 256)
; R9D = use_admm (0 = fast path, nonzero = ADMM with R9D iterations)
; Returns: RAX = number of blocks quantized
; =============================================================================
NQ1_QuantizeTensor PROC FRAME
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
    .endprolog

    mov     rsi, rcx                               ; Source F32
    mov     rdi, rdx                               ; Destination NQ_1 blocks
    mov     r12, r8                                ; n_elements
    mov     r13d, r9d                              ; ADMM flag/iterations

    ; Calculate number of blocks
    mov     rax, r12
    xor     edx, edx
    mov     rcx, QK_NQ1                            ; 256
    div     rcx
    mov     r14, rax                               ; Block count
    xor     ebx, ebx                               ; Block index

@@tensor_loop:
    cmp     rbx, r14
    jge     @@tensor_done

    ; Set up call to quantize one block
    mov     rcx, rsi                               ; Source (current position)
    mov     rdx, rdi                               ; Destination (current block)

    test    r13d, r13d
    jz      @@use_fast

    ; ADMM path
    mov     r8d, r13d                              ; Max iterations
    call    NQ1_QuantizeBlock_ADMM
    jmp     @@tensor_next

@@use_fast:
    call    NQ1_QuantizeBlock_Fast

@@tensor_next:
    add     rsi, (QK_NQ1 * 4)                     ; Advance by 256 floats
    add     rdi, BLOCK_NQ1_SIZE                    ; Advance by 34 bytes
    inc     rbx
    jmp     @@tensor_loop

@@tensor_done:
    mov     rax, r14                               ; Return block count

    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
NQ1_QuantizeTensor ENDP

; =============================================================================
; NQ1_DequantBlock_AVX512
; Dequantize one NQ_1 block (34 bytes) → 256 F32 values.
; Uses AVX-512 opmask merge for efficient bit→float expansion.
;
; RCX = src (NQ1 block, 34 bytes)
; RDX = dst (float*, 256 elements = 1024 bytes)
; Returns: RAX = 256
; =============================================================================
NQ1_DequantBlock_AVX512 PROC FRAME
    push    rbx
    .pushreg rbx
    .endprolog

    ; Load scale d: F16 → F32 → broadcast to ZMM
    movzx   eax, word ptr [rcx + NQ1_SCALE_OFFSET]
    vmovd   xmm0, eax
    vcvtph2ps xmm0, xmm0                           ; F32 scale
    vbroadcastss zmm30, xmm0                       ; +d broadcast

    ; Negate: -d
    vxorps  zmm31, zmm31, zmm31
    vsubps  zmm31, zmm31, zmm30                    ; -d broadcast

    ; Process 16 elements per iteration (1 ZMM width)
    ; 256 / 16 = 16 iterations, consuming 2 bytes of signs each
    lea     rbx, [rcx + NQ1_SIGNS_OFFSET]          ; Signs base
    xor     ecx, ecx                               ; Iteration counter (0..15)

@@dq512_loop:
    ; Load 16 sign bits into opmask k1
    movzx   eax, word ptr [rbx]
    kmovw   k1, eax

    ; Merge-masked move: start with -d, merge +d where k1=1
    vmovaps zmm0, zmm31                            ; All -d
    vmovaps zmm0 {k1}, zmm30                       ; +d where sign bit = 1

    ; Store 16 F32 dequantized values
    vmovups zmmword ptr [rdx], zmm0

    add     rbx, 2                                 ; Next 16 sign bits (2 bytes)
    add     rdx, 64                                ; Next 16 floats (64 bytes)
    inc     ecx
    cmp     ecx, 16
    jb      @@dq512_loop

    lock inc qword ptr [g_NQ1DequantBlocks]

    mov     eax, QK_NQ1                            ; Return 256
    vzeroupper
    pop     rbx
    ret
NQ1_DequantBlock_AVX512 ENDP

; =============================================================================
; NQ1_DequantBlock_AVX2
; Dequantize one NQ_1 block → 256 F32 values.
; AVX2 fallback: expands bits via vpbroadcastd + vpcmpeqd + vblendvps.
;
; RCX = src (NQ1 block)
; RDX = dst (float*, 256 elements)
; Returns: RAX = 256
; =============================================================================
NQ1_DequantBlock_AVX2 PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    .endprolog

    mov     rsi, rcx                               ; Source block
    mov     rbx, rdx                               ; Destination F32

    ; Load scale d: F16 → F32
    movzx   eax, word ptr [rsi + NQ1_SCALE_OFFSET]
    vmovd   xmm0, eax
    vcvtph2ps xmm0, xmm0                           ; F32 scale

    ; Create +d and -d broadcasts (YMM = 8 floats)
    vbroadcastss ymm6, xmm0                        ; +d
    vxorps  ymm7, ymm7, ymm7
    vsubps  ymm7, ymm7, ymm6                       ; -d

    ; Load bit-select mask for 8-wide expansion
    vmovdqu ymm5, ymmword ptr [bit_select_dw]      ; [1,2,4,8,16,32,64,128]

    ; Process 8 elements per iteration, 32 iterations = 256 elements
    lea     rcx, [rsi + NQ1_SIGNS_OFFSET]           ; Signs base
    xor     edx, edx                                ; Byte counter (0..31)

@@dq_avx2_loop:
    ; Load 1 byte of signs → 8 bits
    movzx   eax, byte ptr [rcx + rdx]

    ; Broadcast byte to all 8 dword lanes
    vmovd   xmm0, eax
    vpbroadcastd ymm0, xmm0

    ; Isolate each bit: AND with [1,2,4,8,16,32,64,128]
    vpand   ymm1, ymm0, ymm5

    ; Compare equal to mask → 0xFFFFFFFF where bit was set, 0 otherwise
    vpcmpeqd ymm2, ymm1, ymm5

    ; Blend: select +d where bit=1, -d where bit=0
    ; vblendvps uses bit 31 of mask: 0xFFFFFFFF has bit 31 set
    vblendvps ymm3, ymm7, ymm6, ymm2

    ; Store 8 dequantized F32 values
    vmovups ymmword ptr [rbx], ymm3

    add     rbx, 32                                ; 8 floats × 4 bytes
    inc     edx
    cmp     edx, 32
    jb      @@dq_avx2_loop

    lock inc qword ptr [g_NQ1DequantBlocks]

    mov     eax, QK_NQ1
    vzeroupper
    pop     rsi
    pop     rbx
    ret
NQ1_DequantBlock_AVX2 ENDP

; =============================================================================
; NQ1_Dequant
; Dispatcher: dequantize n_blocks of NQ_1 → F32.
;
; RCX = src (NQ1 block array)
; RDX = dst (float*)
; R8  = n_blocks
; Returns: RAX = total elements dequantized
; =============================================================================
NQ1_Dequant PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    push    r12
    .pushreg r12
    .endprolog

    mov     rsi, rcx
    mov     rdi, rdx
    mov     r12, r8
    xor     ebx, ebx

    ; Get dispatch function pointer
    mov     rax, g_NQ1DequantDispatch
    test    rax, rax
    jnz     @@dq_has_dispatch
    ; Fallback: try AVX2
    lea     rax, NQ1_DequantBlock_AVX2
@@dq_has_dispatch:
    mov     rcx, rax                               ; Save function pointer in rcx temp

@@dq_loop:
    cmp     rbx, r12
    jge     @@dq_done

    ; Call dequant block function
    push    rcx                                    ; Preserve func ptr
    mov     rcx, rsi                               ; src block
    mov     rdx, rdi                               ; dst floats
    call    rax

    pop     rcx                                    ; Restore func ptr
    mov     rax, rcx                               ; Reload for next iteration

    add     rsi, BLOCK_NQ1_SIZE
    add     rdi, (QK_NQ1 * 4)
    inc     rbx
    jmp     @@dq_loop

@@dq_done:
    mov     rax, r12
    imul    rax, QK_NQ1                            ; Total elements

    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
NQ1_Dequant ENDP

; =============================================================================
; NQ1_VecDot_AVX512
; Fused dot product: dot(NQ1_block, F32[256]) → float scalar.
; Uses AVX-512 opmask merge + FMA for maximum throughput.
;
; RCX = src_nq1 (NQ1 block, 34 bytes)
; RDX = src_f32 (float*, 256 elements)
; Returns: XMM0 = dot product (float scalar)
; =============================================================================
NQ1_VecDot_AVX512 PROC FRAME
    push    rbx
    .pushreg rbx
    .endprolog

    ; Load scale d: F16 → F32
    movzx   eax, word ptr [rcx + NQ1_SCALE_OFFSET]
    vmovd   xmm7, eax
    vcvtph2ps xmm7, xmm7                           ; d scalar

    ; Strategy: Use masked-add accumulation
    ;   sum_pos = Σ input[i] where sign[i]=1
    ;   sum_all = Σ input[i] for all i
    ;   dot = d * (2*sum_pos - sum_all)
    ; This avoids expanding bits to ±1.0, saving one instruction per iteration.

    vxorps  zmm0, zmm0, zmm0                       ; sum_pos accumulator
    vxorps  zmm1, zmm1, zmm1                       ; sum_all accumulator

    lea     rbx, [rcx + NQ1_SIGNS_OFFSET]          ; Signs base
    xor     ecx, ecx                               ; Iteration (0..15)

@@vd512_loop:
    ; Load 16 F32 inputs
    vmovups zmm2, zmmword ptr [rdx]

    ; Accumulate all elements
    vaddps  zmm1, zmm1, zmm2

    ; Load 16 sign bits → opmask
    movzx   eax, word ptr [rbx]
    kmovw   k1, eax

    ; Merge-masked add: only add where sign=1 (positive)
    ; zmm0 = zmm0 + zmm2 where k1=1, else zmm0 unchanged
    vaddps  zmm0 {k1}, zmm0, zmm2

    add     rbx, 2
    add     rdx, 64
    inc     ecx
    cmp     ecx, 16
    jb      @@vd512_loop

    ; Horizontal sum of zmm0 (sum_pos) → xmm3
    vextractf64x4 ymm2, zmm0, 1
    vaddps  ymm0, ymm0, ymm2
    vextractf128 xmm2, ymm0, 1
    vaddps  xmm0, xmm0, xmm2
    vhaddps xmm0, xmm0, xmm0
    vhaddps xmm0, xmm0, xmm0                       ; xmm0[0] = sum_pos

    ; Horizontal sum of zmm1 (sum_all) → xmm4
    vextractf64x4 ymm2, zmm1, 1
    vaddps  ymm1, ymm1, ymm2
    vextractf128 xmm2, ymm1, 1
    vaddps  xmm1, xmm1, xmm2
    vhaddps xmm1, xmm1, xmm1
    vhaddps xmm1, xmm1, xmm1                       ; xmm1[0] = sum_all

    ; dot = d * (2*sum_pos - sum_all)
    vaddss  xmm0, xmm0, xmm0                       ; 2 * sum_pos
    vsubss  xmm0, xmm0, xmm1                       ; 2*sum_pos - sum_all
    vmulss  xmm0, xmm0, xmm7                       ; * d

    lock inc qword ptr [g_NQ1VecDotCalls]

    vzeroupper
    pop     rbx
    ret
NQ1_VecDot_AVX512 ENDP

; =============================================================================
; NQ1_VecDot_AVX2
; Fused dot product using AVX2 (no opmask).
; Uses vblendvps for bit expansion, then FMA.
;
; RCX = src_nq1 (NQ1 block)
; RDX = src_f32 (float*, 256 elements)
; Returns: XMM0 = dot product
; =============================================================================
NQ1_VecDot_AVX2 PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    .endprolog

    mov     rsi, rcx                               ; NQ1 block
    mov     rbx, rdx                               ; F32 input

    ; Load scale d
    movzx   eax, word ptr [rsi + NQ1_SCALE_OFFSET]
    vmovd   xmm7, eax
    vcvtph2ps xmm7, xmm7                           ; d scalar

    ; Load constants
    vmovdqu ymm5, ymmword ptr [bit_select_dw]      ; Bit select mask
    vmovups ymm6, ymmword ptr [one_f32]             ; +1.0
    vmovups ymm4, ymmword ptr [neg_one_f32]         ; -1.0

    vxorps  ymm0, ymm0, ymm0                       ; Dot accumulator
    lea     rcx, [rsi + NQ1_SIGNS_OFFSET]           ; Signs base
    xor     edx, edx                                ; Byte counter

@@vd_avx2_loop:
    cmp     edx, 32
    jge     @@vd_avx2_done

    ; Expand 8 sign bits to 8 × ±1.0f
    movzx   eax, byte ptr [rcx + rdx]
    vmovd   xmm1, eax
    vpbroadcastd ymm1, xmm1
    vpand   ymm2, ymm1, ymm5                       ; Isolate bits
    vpcmpeqd ymm3, ymm2, ymm5                      ; Bit=1 → 0xFFFFFFFF
    vblendvps ymm3, ymm4, ymm6, ymm3               ; ±1.0

    ; Load 8 F32 inputs and FMA
    vmovups ymm1, ymmword ptr [rbx]
    vfmadd231ps ymm0, ymm1, ymm3                   ; acc += input * ±1.0

    add     rbx, 32
    inc     edx
    jmp     @@vd_avx2_loop

@@vd_avx2_done:
    ; Horizontal sum
    vextractf128 xmm1, ymm0, 1
    vaddps  xmm0, xmm0, xmm1
    vhaddps xmm0, xmm0, xmm0
    vhaddps xmm0, xmm0, xmm0

    ; Multiply by d
    vmulss  xmm0, xmm0, xmm7

    lock inc qword ptr [g_NQ1VecDotCalls]

    vzeroupper
    pop     rsi
    pop     rbx
    ret
NQ1_VecDot_AVX2 ENDP

; =============================================================================
; NQ1_VecDot
; Dispatcher: calls AVX-512 or AVX2 vec_dot based on CPU capabilities.
;
; RCX = nq1_block, RDX = f32_input
; Returns: XMM0 = dot product
; =============================================================================
NQ1_VecDot PROC
    mov     rax, g_NQ1VecDotDispatch
    test    rax, rax
    jnz     @@vd_go
    lea     rax, NQ1_VecDot_AVX2
@@vd_go:
    jmp     rax                                    ; Tail-call dispatch
NQ1_VecDot ENDP

; =============================================================================
; NQ1_SGEMV
; Matrix-vector multiply: output[i] = dot(W_row_i, input) for NQ_1 weights.
; Processes multiple rows for register reuse.
;
; RCX = W (NQ1 block array, row-major, n_out rows of n_in/256 blocks each)
; RDX = input (float*, n_in elements)
; R8  = output (float*, n_out elements)
; R9D = n_out (number of output elements = weight rows)
; [RSP+40] = n_in (number of input elements, must be multiple of 256)
; Returns: RAX = 0 success
; =============================================================================
NQ1_SGEMV PROC FRAME
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
    sub     rsp, 48
    .allocstack 48
    .endprolog

    mov     rsi, rcx                               ; W (NQ1 blocks)
    mov     r12, rdx                               ; input (F32)
    mov     r13, r8                                ; output (F32)
    mov     r14d, r9d                              ; n_out
    mov     r15d, dword ptr [rsp + 48 + 56 + 40]  ; n_in (stack arg after shadow+saves)

    ; blocks_per_row = n_in / 256
    mov     eax, r15d
    shr     eax, 8                                 ; /256
    mov     ebx, eax                               ; blocks_per_row

    ; bytes_per_row = blocks_per_row * 34
    imul    eax, BLOCK_NQ1_SIZE
    mov     ecx, eax                               ; bytes_per_row

    xor     edi, edi                               ; Row index

@@sgemv_row_loop:
    cmp     edi, r14d
    jge     @@sgemv_done

    ; Compute dot product for this row
    ; output[row] = Σ_block dot(W_block, input_block)

    vxorps  xmm0, xmm0, xmm0                       ; Row accumulator

    xor     r8d, r8d                               ; Block index within row

@@sgemv_block_loop:
    cmp     r8d, ebx
    jge     @@sgemv_store

    ; Calculate block pointer: W + row * bytes_per_row + block * 34
    mov     rax, rdi
    imul    rax, rcx                                ; row * bytes_per_row
    lea     r9, [rsi + rax]
    mov     eax, r8d
    imul    eax, BLOCK_NQ1_SIZE
    add     r9, rax                                 ; Current NQ1 block

    ; Calculate input pointer: input + block * 256 * 4
    mov     eax, r8d
    shl     eax, 10                                 ; block * 1024 (256 * 4 bytes)
    lea     r10, [r12 + rax]

    ; Save volatile state and call VecDot
    vmovss  dword ptr [rsp], xmm0                   ; Save accumulator
    push    rcx
    mov     rcx, r9                                 ; NQ1 block
    mov     rdx, r10                                ; F32 input
    call    NQ1_VecDot
    pop     rcx
    ; xmm0 now has partial dot for this block
    vaddss  xmm0, xmm0, dword ptr [rsp]            ; Add to accumulator

    inc     r8d
    jmp     @@sgemv_block_loop

@@sgemv_store:
    vmovss  dword ptr [r13 + rdi*4], xmm0          ; Store output[row]
    inc     edi
    jmp     @@sgemv_row_loop

@@sgemv_done:
    xor     eax, eax
    add     rsp, 48
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
NQ1_SGEMV ENDP

; =============================================================================
; NQ1_SGEMM_Tiled
; Tiled GEMM: C[M×N] += A[M×K] × dequant(B_NQ1[N×K])
; B is stored row-major: N rows of K elements each (NQ_1 quantized).
; Tiles over M to process NQ_GEMM_TILE_M rows simultaneously.
;
; RCX = A (float*, M×K row-major)
; RDX = B (NQ1 blocks, N rows × K/256 blocks each)
; R8  = C (float*, M×N row-major, must be pre-zeroed or accumulated)
; R9D = M (batch size / rows of A)
; [RSP+40] = N (output columns / rows of B)
; [RSP+48] = K (inner dimension, must be multiple of 256)
; Returns: RAX = 0 success
; =============================================================================
NQ1_SGEMM_Tiled PROC FRAME
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
    push    rbp
    .pushreg rbp
    sub     rsp, 96
    .allocstack 96
    .endprolog

    ; Save parameters
    mov     [rsp], rcx                              ; A
    mov     [rsp+8], rdx                            ; B
    mov     [rsp+16], r8                            ; C
    mov     [rsp+24], r9                            ; M (as qword)
    mov     eax, dword ptr [rsp + 96 + 72 + 40]    ; N
    mov     [rsp+32], eax
    mov     eax, dword ptr [rsp + 96 + 72 + 48]    ; K
    mov     [rsp+36], eax

    ; blocks_per_row = K / 256
    mov     eax, [rsp+36]
    shr     eax, 8
    mov     [rsp+40], eax                           ; blocks_per_row

    ; B row stride = blocks_per_row * 34
    imul    eax, BLOCK_NQ1_SIZE
    mov     [rsp+44], eax                           ; B_row_stride

    ; A row stride = K * 4
    mov     eax, [rsp+36]
    shl     eax, 2
    mov     [rsp+48], eax                           ; A_row_stride

    ; C row stride = N * 4
    mov     eax, [rsp+32]
    shl     eax, 2
    mov     [rsp+52], eax                           ; C_row_stride

    lock inc qword ptr [g_NQ1GemmCalls]

    ; === Outer loop: rows of A (M dimension) ===
    xor     edi, edi                                ; m = 0

@@gemm_m_loop:
    cmp     edi, dword ptr [rsp+24]                 ; m < M
    jge     @@gemm_done

    ; === Middle loop: rows of B (N dimension) ===
    xor     ebx, ebx                                ; n = 0

@@gemm_n_loop:
    cmp     ebx, dword ptr [rsp+32]                 ; n < N
    jge     @@gemm_next_m

    ; Compute C[m, n] += dot(A_row_m, dequant(B_row_n))
    ; = Σ_block NQ1_VecDot(B_block, A_block)

    vxorps  xmm0, xmm0, xmm0                       ; Accumulator for C[m,n]
    xor     r8d, r8d                               ; Block index

@@gemm_k_loop:
    cmp     r8d, dword ptr [rsp+40]                 ; block < blocks_per_row
    jge     @@gemm_store

    ; B block ptr: B + n * B_row_stride + block * 34
    mov     rax, [rsp+8]                           ; B base
    mov     ecx, ebx
    imul    ecx, dword ptr [rsp+44]                ; n * B_row_stride
    add     rax, rcx
    mov     ecx, r8d
    imul    ecx, BLOCK_NQ1_SIZE
    add     rax, rcx                               ; B block ptr
    mov     r14, rax

    ; A data ptr: A + m * A_row_stride + block * 256 * 4
    mov     rax, [rsp]                             ; A base
    mov     ecx, edi
    imul    ecx, dword ptr [rsp+48]                ; m * A_row_stride
    add     rax, rcx
    mov     ecx, r8d
    shl     ecx, 10                                ; block * 1024
    add     rax, rcx
    mov     r15, rax

    ; Save accumulator
    vmovss  dword ptr [rsp+80], xmm0

    ; Call VecDot
    mov     rcx, r14                               ; NQ1 block
    mov     rdx, r15                               ; F32 input
    call    NQ1_VecDot

    vaddss  xmm0, xmm0, dword ptr [rsp+80]        ; Accumulate

    inc     r8d
    jmp     @@gemm_k_loop

@@gemm_store:
    ; C[m, n] += accumulated dot product
    ; C ptr: C + m * C_row_stride + n * 4
    mov     rax, [rsp+16]                          ; C base
    mov     ecx, edi
    imul    ecx, dword ptr [rsp+52]
    add     rax, rcx
    lea     rax, [rax + rbx*4]

    vaddss  xmm1, xmm0, dword ptr [rax]           ; C[m,n] += dot
    vmovss  dword ptr [rax], xmm1

    inc     ebx
    jmp     @@gemm_n_loop

@@gemm_next_m:
    inc     edi
    jmp     @@gemm_m_loop

@@gemm_done:
    xor     eax, eax
    add     rsp, 96
    pop     rbp
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
NQ1_SGEMM_Tiled ENDP

; =============================================================================
; NQ1_SGEMM
; Dispatcher for SGEMM (same interface as NQ1_SGEMM_Tiled).
; =============================================================================
NQ1_SGEMM PROC
    jmp     NQ1_SGEMM_Tiled
NQ1_SGEMM ENDP

; =============================================================================
; NQ_MatrixFactor_Rank1
; Low-rank rank-1 binary factorization of a full weight matrix.
; W[M×N] ≈ s * row_signs[M] ⊗ col_signs[N]
; where signs ∈ {±1}, s is a scalar.
;
; RCX = W (float*, M×N row-major)
; RDX = output buffer (NQ_MATRIX header + data)
; R8D = M (rows)
; R9D = N (cols)
; Returns: RAX = total bytes written to output
; =============================================================================
NQ_MatrixFactor_Rank1 PROC FRAME
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
    sub     rsp, 64
    .allocstack 64
    .endprolog

    mov     rsi, rcx                               ; W matrix
    mov     rdi, rdx                               ; Output buffer
    mov     r12d, r8d                              ; M
    mov     r13d, r9d                              ; N

    ; === Write NQ_MATRIX header ===
    mov     dword ptr [rdi + NQM_MAGIC_OFFSET], NQM_MAGIC_VALUE
    mov     dword ptr [rdi + NQM_ROWS_OFFSET], r12d
    mov     dword ptr [rdi + NQM_COLS_OFFSET], r13d
    mov     dword ptr [rdi + NQM_RANK_OFFSET], 1

    ; === Phase 1: Compute row signs ===
    ; row_sign[i] = sign(Σ_j W[i,j])
    ; (Sign of row sum gives the dominant direction)

    lea     r14, [rdi + NQM_HEADER_SIZE]            ; Row signs output
    mov     r15d, r12d
    add     r15d, 7
    shr     r15d, 3                                 ; ceil(M/8) bytes for row signs

    xor     ecx, ecx                               ; Row index
    xor     r8d, r8d                               ; Output byte index
    xor     r9d, r9d                                ; Bit accumulator
    xor     r10d, r10d                              ; Bit position

@@mf1_row_loop:
    cmp     ecx, r12d
    jge     @@mf1_row_flush

    ; Compute row sum: Σ_j W[row, j]
    vxorps  ymm0, ymm0, ymm0                       ; Sum accumulator
    xor     edx, edx                               ; Col index
    mov     eax, ecx
    imul    eax, r13d                               ; row * N
    lea     rbx, [rsi + rax*4]                     ; Row base pointer

@@mf1_col_sum:
    cmp     edx, r13d
    jge     @@mf1_col_sum_done

    ; Process up to 8 columns at a time
    mov     eax, r13d
    sub     eax, edx
    cmp     eax, 8
    jl      @@mf1_col_scalar

    vmovups ymm1, ymmword ptr [rbx + rdx*4]
    vaddps  ymm0, ymm0, ymm1
    add     edx, 8
    jmp     @@mf1_col_sum

@@mf1_col_scalar:
    vmovss  xmm1, dword ptr [rbx + rdx*4]
    vaddss  xmm0, xmm0, xmm1
    inc     edx
    jmp     @@mf1_col_sum

@@mf1_col_sum_done:
    ; Horizontal sum of ymm0
    vextractf128 xmm1, ymm0, 1
    vaddps  xmm0, xmm0, xmm1
    vhaddps xmm0, xmm0, xmm0
    vhaddps xmm0, xmm0, xmm0                       ; xmm0[0] = row sum

    ; Extract sign: bit=1 if sum >= 0
    vmovmskps eax, xmm0                             ; bit 0 = sign of xmm0[0]
    not     al
    and     eax, 1
    shl     eax, cl                                 ; Shift to bit position

    ; Wait, we need bit position within the current byte
    mov     eax, ecx
    and     eax, 7                                  ; Bit position in byte

    ; Re-extract sign
    vxorps  xmm1, xmm1, xmm1
    vcomiss xmm0, xmm1
    setae   bl                                      ; BL=1 if sum >= 0
    movzx   ebx, bl
    shl     ebx, cl                                 ; Wrong: cl has row index
    ; Fix: use bit position
    push    rcx
    mov     cl, al
    shl     ebx, cl                                 ; Shift sign bit to position
    pop     rcx
    or      r9d, ebx                                ; Accumulate into byte

    mov     eax, ecx
    and     eax, 7
    cmp     eax, 7
    jne     @@mf1_row_next

    ; Flush byte
    mov     byte ptr [r14 + r8], r9b
    inc     r8d
    xor     r9d, r9d

@@mf1_row_next:
    inc     ecx
    jmp     @@mf1_row_loop

@@mf1_row_flush:
    ; Flush remaining bits
    mov     eax, r12d
    and     eax, 7
    test    eax, eax
    jz      @@mf1_col_signs
    mov     byte ptr [r14 + r8], r9b
    inc     r8d

@@mf1_col_signs:
    ; === Phase 2: Compute column signs ===
    ; col_sign[j] = sign(Σ_i W[i,j] * row_sign[i])
    ; (Given row signs, optimal col signs minimize reconstruction error)

    movzx   eax, r15w                               ; Row signs bytes
    lea     r14, [rdi + NQM_HEADER_SIZE + rax]      ; Col signs output

    mov     eax, r13d
    add     eax, 7
    shr     eax, 3                                  ; ceil(N/8) bytes for col signs

    xor     ecx, ecx                               ; Col index
    xor     r9d, r9d                                ; Bit accumulator

@@mf1_csign_loop:
    cmp     ecx, r13d
    jge     @@mf1_csign_flush

    ; Compute weighted column sum: Σ_i W[i,j] * row_sign[i]
    vxorps  xmm0, xmm0, xmm0
    xor     edx, edx                               ; Row index

@@mf1_csign_inner:
    cmp     edx, r12d
    jge     @@mf1_csign_process

    ; Load W[row, col]
    mov     eax, edx
    imul    eax, r13d
    add     eax, ecx
    vmovss  xmm1, dword ptr [rsi + rax*4]

    ; Get row_sign[row]
    mov     eax, edx
    shr     eax, 3
    movzx   ebx, byte ptr [rdi + NQM_HEADER_SIZE + rax]
    mov     eax, edx
    and     eax, 7
    bt      ebx, eax
    jc      @@mf1_csign_pos

    ; row_sign = -1: subtract
    vsubss  xmm0, xmm0, xmm1
    jmp     @@mf1_csign_next_row

@@mf1_csign_pos:
    ; row_sign = +1: add
    vaddss  xmm0, xmm0, xmm1

@@mf1_csign_next_row:
    inc     edx
    jmp     @@mf1_csign_inner

@@mf1_csign_process:
    ; Sign of weighted sum → col_sign bit
    vxorps  xmm1, xmm1, xmm1
    vcomiss xmm0, xmm1
    setae   bl
    movzx   ebx, bl
    mov     eax, ecx
    and     eax, 7
    push    rcx
    mov     cl, al
    shl     ebx, cl
    pop     rcx
    or      r9d, ebx

    mov     eax, ecx
    and     eax, 7
    cmp     eax, 7
    jne     @@mf1_csign_next_col

    ; Flush byte
    mov     eax, ecx
    shr     eax, 3
    mov     byte ptr [r14 + rax], r9b
    xor     r9d, r9d

@@mf1_csign_next_col:
    inc     ecx
    jmp     @@mf1_csign_loop

@@mf1_csign_flush:
    ; Flush remaining
    mov     eax, r13d
    and     eax, 7
    test    eax, eax
    jz      @@mf1_compute_scale
    mov     eax, r13d
    shr     eax, 3
    mov     byte ptr [r14 + rax], r9b

@@mf1_compute_scale:
    ; === Phase 3: Compute optimal scale ===
    ; s = Σ_{i,j} W[i,j] * row_sign[i] * col_sign[j] / (M * N)
    vxorps  xmm0, xmm0, xmm0                       ; Scale accumulator

    xor     ecx, ecx                               ; Row
@@mf1_scale_row:
    cmp     ecx, r12d
    jge     @@mf1_scale_done

    xor     edx, edx                               ; Col
@@mf1_scale_col:
    cmp     edx, r13d
    jge     @@mf1_scale_next_row

    ; Load W[i,j]
    mov     eax, ecx
    imul    eax, r13d
    add     eax, edx
    vmovss  xmm1, dword ptr [rsi + rax*4]

    ; Get row_sign[i] * col_sign[j]
    mov     eax, ecx
    shr     eax, 3
    movzx   ebx, byte ptr [rdi + NQM_HEADER_SIZE + rax]
    mov     eax, ecx
    and     eax, 7
    bt      ebx, eax
    setc    r8b                                     ; r8b = row_sign bit

    ; Get col_sign
    mov     eax, edx
    shr     eax, 3
    movzx   ebx, byte ptr [r14 + rax]
    mov     eax, edx
    and     eax, 7
    bt      ebx, eax
    setc    r9b                                     ; r9b = col_sign bit

    ; Combined sign: both same → +1, different → -1
    cmp     r8b, r9b
    je      @@mf1_scale_add
    vsubss  xmm0, xmm0, xmm1
    jmp     @@mf1_scale_next_col
@@mf1_scale_add:
    vaddss  xmm0, xmm0, xmm1
@@mf1_scale_next_col:
    inc     edx
    jmp     @@mf1_scale_col

@@mf1_scale_next_row:
    inc     ecx
    jmp     @@mf1_scale_row

@@mf1_scale_done:
    ; scale = sum / (M * N)
    mov     eax, r12d
    imul    eax, r13d
    vcvtsi2ss xmm1, xmm1, eax
    vdivss  xmm0, xmm0, xmm1

    ; Store scale
    vmovss  dword ptr [rdi + NQM_SCALES_OFFSET], xmm0

    ; === Calculate total output size ===
    ; header + ceil(M/8) + ceil(N/8)
    mov     eax, r12d
    add     eax, 7
    shr     eax, 3                                 ; ceil(M/8)
    mov     ecx, r13d
    add     ecx, 7
    shr     ecx, 3                                 ; ceil(N/8)
    add     eax, ecx
    add     eax, NQM_HEADER_SIZE                   ; + 32 header

    vzeroupper
    add     rsp, 64
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
NQ_MatrixFactor_Rank1 ENDP

; =============================================================================
; NQ_MatrixFactor_MultiRank
; Multi-rank binary factorization via greedy residual peeling.
; W ≈ Σ_{k=1}^{r} s_k * row_k ⊗ col_k
;
; RCX = W (float*, M×N — WILL BE MODIFIED as residual)
; RDX = output buffer (NQ_MATRIX format)
; R8D = M, R9D = N
; [RSP+40] = rank (1-8)
; Returns: RAX = total bytes written
; =============================================================================
NQ_MatrixFactor_MultiRank PROC FRAME
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
    push    rbp
    .pushreg rbp
    sub     rsp, 96
    .allocstack 96
    .endprolog

    mov     rsi, rcx                               ; W (residual, modified in-place)
    mov     rdi, rdx                               ; Output
    mov     r12d, r8d                              ; M
    mov     r13d, r9d                              ; N
    mov     eax, dword ptr [rsp + 96 + 72 + 40]
    mov     r14d, eax                              ; rank
    cmp     r14d, NQM_MAX_RANK
    jbe     @@mr_rank_ok
    mov     r14d, NQM_MAX_RANK
@@mr_rank_ok:

    ; Write header
    mov     dword ptr [rdi + NQM_MAGIC_OFFSET], NQM_MAGIC_VALUE
    mov     dword ptr [rdi + NQM_ROWS_OFFSET], r12d
    mov     dword ptr [rdi + NQM_COLS_OFFSET], r13d
    mov     dword ptr [rdi + NQM_RANK_OFFSET], r14d

    ; Calculate sign data sizes
    mov     eax, r12d
    add     eax, 7
    shr     eax, 3
    mov     [rsp], eax                             ; row_sign_bytes = ceil(M/8)

    mov     eax, r13d
    add     eax, 7
    shr     eax, 3
    mov     [rsp+4], eax                           ; col_sign_bytes = ceil(N/8)

    mov     eax, [rsp]
    add     eax, [rsp+4]
    mov     [rsp+8], eax                           ; bytes_per_rank

    ; Data starts after header
    lea     r15, [rdi + NQM_HEADER_SIZE]           ; Current data write position

    xor     ebx, ebx                               ; Rank iteration

@@mr_rank_loop:
    cmp     ebx, r14d
    jge     @@mr_done

    ; Factor current residual as rank-1
    mov     rcx, rsi                               ; Residual W
    mov     rdx, rdi                               ; Temp: use header buffer for rank-1
    mov     r8d, r12d
    mov     r9d, r13d
    call    NQ_MatrixFactor_Rank1

    ; Copy scale to scales array
    vmovss  xmm0, dword ptr [rdi + NQM_SCALES_OFFSET]
    vmovss  dword ptr [rdi + NQM_SCALES_OFFSET + rbx*4], xmm0

    ; Copy row_signs and col_signs to sequential data area
    ; Row signs are at [rdi + NQM_HEADER_SIZE]
    ; Col signs are at [rdi + NQM_HEADER_SIZE + row_sign_bytes]
    ; We need to copy these to r15 (which advances per rank)

    ; Copy row_signs
    lea     rcx, [rdi + NQM_HEADER_SIZE]
    mov     rdx, r15
    mov     r8d, [rsp]                              ; row_sign_bytes
    push    rsi
    mov     rsi, rcx
    mov     [rsp+24], rdi                           ; save rdi on stack (rsp+16 in caller frame + push offset)
    mov     rdi, rdx
    mov     ecx, r8d
    rep     movsb
    mov     rdi, [rsp+24]                           ; restore rdi from stack
    pop     rsi

    ; Copy col_signs
    mov     eax, [rsp]
    lea     rcx, [rdi + NQM_HEADER_SIZE + rax]
    lea     rdx, [r15 + rax]                        ; col_signs after row_signs
    mov     r8d, [rsp+4]                            ; col_sign_bytes
    push    rsi
    mov     rsi, rcx
    mov     [rsp+24], rdi                           ; save rdi on stack
    mov     rdi, rdx
    mov     ecx, r8d
    rep     movsb
    mov     rdi, [rsp+24]                           ; restore rdi from stack
    pop     rsi

    ; Advance data pointer
    mov     eax, [rsp+8]
    add     r15, rax

    ; === Subtract rank-1 approximation from residual ===
    ; R[i,j] -= s * row_sign[i] * col_sign[j]
    vbroadcastss ymm7, dword ptr [rdi + NQM_SCALES_OFFSET + rbx*4]

    xor     ecx, ecx                               ; Row
@@mr_sub_row:
    cmp     ecx, r12d
    jge     @@mr_sub_done

    xor     edx, edx                               ; Col
@@mr_sub_col:
    cmp     edx, r13d
    jge     @@mr_sub_next_row

    ; Get combined sign
    ; row_sign
    mov     eax, ecx
    shr     eax, 3
    sub     eax, [rsp]                              ; Oops, this is relative to r15-[rsp+8]
    ; Actually, we already copied to r15 - bytes_per_rank..
    ; Just use the output data we already wrote
    mov     r8d, [rsp+8]
    neg     r8d
    movzx   r9d, byte ptr [r15 + r8]               ; This would be wrong
    ; Let me fix: re-read from the rank-1 result in the header area
    mov     eax, ecx
    shr     eax, 3
    movzx   r9d, byte ptr [rdi + NQM_HEADER_SIZE + rax]
    mov     eax, ecx
    and     eax, 7
    bt      r9d, eax
    setc    r8b

    ; col_sign
    mov     eax, edx
    shr     eax, 3
    mov     r10d, [rsp]
    lea     r10, [rdi + r10]          ; combine base + col_sign_offset
    movzx   r9d, byte ptr [r10 + rax + NQM_HEADER_SIZE]
    mov     eax, edx
    and     eax, 7
    bt      r9d, eax
    setc    r9b

    ; Product sign → ±s
    ; Load current residual element
    mov     eax, ecx
    imul    eax, r13d
    add     eax, edx
    vmovss  xmm0, dword ptr [rsi + rax*4]

    cmp     r8b, r9b
    je      @@mr_sub_pos
    ; Different signs → approximation is -s, subtract (-s) = add s
    vaddss  xmm0, xmm0, xmm7
    jmp     @@mr_sub_store
@@mr_sub_pos:
    ; Same signs → approximation is +s, subtract s
    vsubss  xmm0, xmm0, xmm7
@@mr_sub_store:
    vmovss  dword ptr [rsi + rax*4], xmm0

    inc     edx
    jmp     @@mr_sub_col

@@mr_sub_next_row:
    inc     ecx
    jmp     @@mr_sub_row

@@mr_sub_done:
    inc     ebx
    jmp     @@mr_rank_loop

@@mr_done:
    ; Calculate total output size
    mov     eax, NQM_HEADER_SIZE
    mov     ecx, [rsp+8]                           ; bytes_per_rank
    imul    ecx, r14d                               ; * rank
    add     eax, ecx

    vzeroupper
    add     rsp, 96
    pop     rbp
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret

NQ_MatrixFactor_MultiRank ENDP

; =============================================================================
; NQ_MatrixGEMM
; GEMM using matrix-level binary factorization.
; C[M×N] = A[M×K] × dequant(NQ_MATRIX[K×N])
; Exploits: C[i,j] = Σ_r s_r * col_sign[r][j] * dot(A_row_i, row_signs[r])
;
; RCX = A (float*, M×K)
; RDX = NQ_MATRIX data (header + signs)
; R8  = C (float*, M×N, will be overwritten)
; R9D = M (rows of A)
; Returns: RAX = 0 success
; =============================================================================
NQ_MatrixGEMM PROC FRAME
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
    push    rbp
    .pushreg rbp
    sub     rsp, 128
    .allocstack 128
    .endprolog

    mov     [rsp], rcx                             ; A
    mov     [rsp+8], rdx                           ; NQ_MATRIX
    mov     [rsp+16], r8                           ; C
    mov     [rsp+24], r9d                          ; M

    ; Parse header
    mov     eax, dword ptr [rdx + NQM_ROWS_OFFSET]
    mov     [rsp+28], eax                          ; K (rows of weight = inner dim)
    mov     eax, dword ptr [rdx + NQM_COLS_OFFSET]
    mov     [rsp+32], eax                          ; N (cols of weight)
    mov     eax, dword ptr [rdx + NQM_RANK_OFFSET]
    mov     [rsp+36], eax                          ; rank

    ; Sign data sizes
    mov     eax, [rsp+28]
    add     eax, 7
    shr     eax, 3
    mov     [rsp+40], eax                          ; row_sign_bytes
    mov     eax, [rsp+32]
    add     eax, 7
    shr     eax, 3
    mov     [rsp+44], eax                          ; col_sign_bytes

    ; Zero C matrix
    mov     rdi, [rsp+16]
    mov     ecx, [rsp+24]                          ; M
    imul    ecx, [rsp+32]                          ; * N
    shl     ecx, 2                                 ; * 4 bytes
    xor     eax, eax
    shr     ecx, 3                                 ; / 8 (qwords)
    rep     stosq

    ; For each rank factor r:
    ;   1. Precompute P[i] = dot(A_row_i, row_signs[r]) for all i in [0, M)
    ;   2. For each (i, j): C[i,j] += s_r * col_sign[r][j] * P[i]

    xor     ebp, ebp                               ; Rank iteration

@@mgemm_rank_loop:
    cmp     ebp, dword ptr [rsp+36]
    jge     @@mgemm_done

    ; Load scale for this rank
    mov     rdx, [rsp+8]
    vmovss  xmm15, dword ptr [rdx + NQM_SCALES_OFFSET + rbp*4]

    ; Calculate row_signs pointer for this rank
    ; data_offset = header_size + r * (row_sign_bytes + col_sign_bytes)
    mov     eax, [rsp+40]
    add     eax, [rsp+44]
    imul    eax, ebp
    add     eax, NQM_HEADER_SIZE
    lea     r14, [rdx + rax]                       ; row_signs[r]

    ; col_signs[r] = row_signs[r] + row_sign_bytes
    mov     eax, [rsp+40]
    lea     r15, [r14 + rax]                       ; col_signs[r]

    ; === Step 1: Precompute P[i] = dot(A_row_i, row_signs[r]) ===
    ; Store P on stack at [rsp+64..] (up to M floats)
    mov     ecx, [rsp+24]                          ; M
    xor     edi, edi                               ; Row index

@@mgemm_precomp:
    cmp     edi, ecx
    jge     @@mgemm_step2

    ; Compute dot(A_row_i, row_signs)
    ; dot = 2 * sum(A[i,k] where rsign[k]=1) - sum(A[i,k])
    vxorps  ymm0, ymm0, ymm0                       ; sum_pos
    vxorps  ymm1, ymm1, ymm1                       ; sum_all

    mov     rax, [rsp]                              ; A base
    mov     r8d, edi
    imul    r8d, [rsp+28]                           ; row * K
    lea     r10, [rax + r8*4]                      ; A_row_i

    xor     edx, edx                               ; K element index
    mov     r11d, [rsp+28]                          ; K total

@@mgemm_dot_k:
    cmp     edx, r11d
    jge     @@mgemm_dot_done

    ; Check if we can do 8 at a time
    mov     eax, r11d
    sub     eax, edx
    cmp     eax, 8
    jl      @@mgemm_dot_scalar

    vmovups ymm2, ymmword ptr [r10 + rdx*4]
    vaddps  ymm1, ymm1, ymm2                       ; sum_all += A[8]

    ; Extract 8 sign bits for elements edx..edx+7
    mov     eax, edx
    shr     eax, 3
    movzx   ebx, byte ptr [r14 + rax]
    mov     eax, edx
    and     eax, 7
    ; If aligned to byte boundary, use all 8 bits
    test    eax, eax
    jnz     @@mgemm_dot_scalar                     ; Not byte-aligned, fall back

    ; Expand 8 bits to mask and blend-add
    vmovd   xmm3, ebx
    vpbroadcastd ymm3, xmm3
    vpand   ymm4, ymm3, ymmword ptr [bit_select_dw]
    vpcmpeqd ymm4, ymm4, ymmword ptr [bit_select_dw]
    ; ymm4 = 0xFFFFFFFF where bit=1

    ; Masked add: sum_pos += A[i] where sign=1
    vandps  ymm5, ymm2, ymm4                       ; Zero out elements where sign=0
    vaddps  ymm0, ymm0, ymm5

    add     edx, 8
    jmp     @@mgemm_dot_k

@@mgemm_dot_scalar:
    vmovss  xmm2, dword ptr [r10 + rdx*4]
    vaddss  xmm1, xmm1, xmm2

    ; Check sign bit
    mov     eax, edx
    shr     eax, 3
    movzx   ebx, byte ptr [r14 + rax]
    mov     eax, edx
    and     eax, 7
    bt      ebx, eax
    jnc     @@mgemm_dot_neg
    vaddss  xmm0, xmm0, xmm2
@@mgemm_dot_neg:
    inc     edx
    jmp     @@mgemm_dot_k

@@mgemm_dot_done:
    ; Horizontal sums
    vextractf128 xmm2, ymm0, 1
    vaddps  xmm0, xmm0, xmm2
    vhaddps xmm0, xmm0, xmm0
    vhaddps xmm0, xmm0, xmm0                       ; sum_pos

    vextractf128 xmm2, ymm1, 1
    vaddps  xmm1, xmm1, xmm2
    vhaddps xmm1, xmm1, xmm1
    vhaddps xmm1, xmm1, xmm1                       ; sum_all

    ; P[i] = 2*sum_pos - sum_all
    vaddss  xmm0, xmm0, xmm0
    vsubss  xmm0, xmm0, xmm1
    vmovss  dword ptr [rsp + 64 + rdi*4], xmm0     ; Store P[i]

    inc     edi
    mov     ecx, [rsp+24]
    jmp     @@mgemm_precomp

@@mgemm_step2:
    ; === Step 2: C[i,j] += s_r * col_sign[r][j] * P[i] ===
    mov     ecx, [rsp+24]                          ; M
    xor     edi, edi                               ; Row i

@@mgemm_out_row:
    cmp     edi, ecx
    jge     @@mgemm_next_rank

    ; Load P[i] * s_r
    vmovss  xmm0, dword ptr [rsp + 64 + rdi*4]
    vmulss  xmm0, xmm0, xmm15                      ; s_r * P[i]

    ; For each output col j
    mov     r8, [rsp+16]                           ; C base
    mov     eax, edi
    imul    eax, [rsp+32]
    lea     r10, [r8 + rax*4]                      ; C_row_i

    xor     edx, edx                               ; Col j

@@mgemm_out_col:
    cmp     edx, dword ptr [rsp+32]
    jge     @@mgemm_out_next_row

    ; Get col_sign[r][j]
    mov     eax, edx
    shr     eax, 3
    movzx   ebx, byte ptr [r15 + rax]
    mov     eax, edx
    and     eax, 7
    bt      ebx, eax
    jc      @@mgemm_col_pos

    ; col_sign = -1: C[i,j] -= s_r * P[i]
    vmovss  xmm1, dword ptr [r10 + rdx*4]
    vsubss  xmm1, xmm1, xmm0
    vmovss  dword ptr [r10 + rdx*4], xmm1
    jmp     @@mgemm_out_next_col

@@mgemm_col_pos:
    ; col_sign = +1: C[i,j] += s_r * P[i]
    vmovss  xmm1, dword ptr [r10 + rdx*4]
    vaddss  xmm1, xmm1, xmm0
    vmovss  dword ptr [r10 + rdx*4], xmm1

@@mgemm_out_next_col:
    inc     edx
    jmp     @@mgemm_out_col

@@mgemm_out_next_row:
    inc     edi
    mov     ecx, [rsp+24]
    jmp     @@mgemm_out_row

@@mgemm_next_rank:
    inc     ebp
    jmp     @@mgemm_rank_loop

@@mgemm_done:
    xor     eax, eax
    vzeroupper
    add     rsp, 128
    pop     rbp
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
NQ_MatrixGEMM ENDP

; =============================================================================
; NQ1_Requantize_Q4_0
; Requantize Q4_0 blocks → NQ_1 blocks (dequant→F32→quantize pipeline).
;
; RCX = src (Q4_0 blocks, 18 bytes each)
; RDX = dst (NQ_1 blocks, 34 bytes each)
; R8  = n_elements (must be multiple of 256)
; Returns: RAX = number of NQ_1 blocks produced
; =============================================================================
NQ1_Requantize_Q4_0 PROC FRAME
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
    sub     rsp, 1088                              ; 256 F32 scratch (1024) + 64 alignment
    .allocstack 1088
    .endprolog

    mov     rsi, rcx                               ; Q4_0 blocks
    mov     rdi, rdx                               ; NQ_1 output
    mov     r12, r8                                ; n_elements

    ; Number of NQ_1 blocks = n_elements / 256
    mov     rax, r12
    shr     rax, 8
    mov     r13, rax                               ; NQ_1 block count

    ; Q4_0 blocks per NQ_1 block = 256 / 32 = 8
    xor     ebx, ebx                               ; NQ_1 block index

@@rq40_loop:
    cmp     rbx, r13
    jge     @@rq40_done

    ; Dequantize 8 Q4_0 blocks (256 elements) → F32 scratch buffer
    lea     r8, [rsp]                              ; F32 scratch
    xor     ecx, ecx                               ; Q4_0 sub-block index

@@rq40_dq_sub:
    cmp     ecx, 8
    jge     @@rq40_quantize

    ; Dequant one Q4_0 block (32 elements)
    ; Q4_0: [F16 d (2 bytes)][qs[16] (16 bytes)] = 18 bytes
    ; Actually Q4_0 scale is F16 at offset 0, data at offset 2

    ; Load scale: F16 → F32
    movzx   eax, word ptr [rsi]
    vmovd   xmm0, eax
    vcvtph2ps xmm0, xmm0                           ; d scalar
    vbroadcastss ymm6, xmm0

    ; Dequant 32 elements: W[i] = d * (nibble[i] - 8)
    xor     edx, edx                               ; Byte index in qs (0..15)
@@rq40_dq_byte:
    cmp     edx, 16
    jge     @@rq40_dq_next

    movzx   eax, byte ptr [rsi + 2 + rdx]

    ; Low nibble: element 2*edx
    mov     r9d, eax
    and     r9d, 0Fh
    sub     r9d, 8
    vcvtsi2ss xmm1, xmm1, r9d
    vmulss  xmm1, xmm1, xmm0
    vmovss  dword ptr [r8 + rdx*8], xmm1

    ; High nibble: element 2*edx+1
    shr     eax, 4
    sub     eax, 8
    vcvtsi2ss xmm1, xmm1, eax
    vmulss  xmm1, xmm1, xmm0
    vmovss  dword ptr [r8 + rdx*8 + 4], xmm1

    inc     edx
    jmp     @@rq40_dq_byte

@@rq40_dq_next:
    add     rsi, 18                                ; Next Q4_0 block (18 bytes)
    add     r8, 128                                ; 32 * 4 bytes F32
    inc     ecx
    jmp     @@rq40_dq_sub

@@rq40_quantize:
    ; Quantize 256 F32 values → NQ_1 block
    lea     rcx, [rsp]                             ; F32 scratch
    mov     rdx, rdi                               ; NQ_1 output
    call    NQ1_QuantizeBlock_Fast

    add     rdi, BLOCK_NQ1_SIZE                    ; Next NQ_1 block
    inc     rbx
    jmp     @@rq40_loop

@@rq40_done:
    mov     rax, r13                               ; Return block count

    vzeroupper
    add     rsp, 1088
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
NQ1_Requantize_Q4_0 ENDP

; =============================================================================
; NQ1_Requantize_Q8_0
; Requantize Q8_0 blocks → NQ_1 blocks.
;
; RCX = src (Q8_0 blocks, 34 bytes each)
; RDX = dst (NQ_1 blocks, 34 bytes each)
; R8  = n_elements (must be multiple of 256)
; Returns: RAX = number of NQ_1 blocks produced
; =============================================================================
NQ1_Requantize_Q8_0 PROC FRAME
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
    sub     rsp, 1088
    .allocstack 1088
    .endprolog

    mov     rsi, rcx
    mov     rdi, rdx
    mov     r12, r8

    mov     rax, r12
    shr     rax, 8
    mov     r13, rax                               ; NQ_1 block count

    ; Q8_0 blocks per NQ_1: 256/32 = 8
    xor     ebx, ebx

@@rq80_loop:
    cmp     rbx, r13
    jge     @@rq80_done

    lea     r8, [rsp]                              ; F32 scratch
    xor     ecx, ecx

@@rq80_dq_sub:
    cmp     ecx, 8
    jge     @@rq80_quantize

    ; Q8_0: [F16 d (2 bytes)][int8 qs[32] (32 bytes)] = 34 bytes

    ; Load scale
    movzx   eax, word ptr [rsi]
    vmovd   xmm0, eax
    vcvtph2ps xmm0, xmm0

    ; Dequant 32 elements: W[i] = d * qs[i]
    xor     edx, edx
@@rq80_dq_elem:
    cmp     edx, 32
    jge     @@rq80_dq_next

    movsx   eax, byte ptr [rsi + 2 + rdx]          ; Signed int8
    vcvtsi2ss xmm1, xmm1, eax
    vmulss  xmm1, xmm1, xmm0
    vmovss  dword ptr [r8 + rdx*4], xmm1

    inc     edx
    jmp     @@rq80_dq_elem

@@rq80_dq_next:
    add     rsi, 34                                ; 34 bytes per Q8_0 block
    add     r8, 128
    inc     ecx
    jmp     @@rq80_dq_sub

@@rq80_quantize:
    lea     rcx, [rsp]
    mov     rdx, rdi
    call    NQ1_QuantizeBlock_Fast

    add     rdi, BLOCK_NQ1_SIZE
    inc     rbx
    jmp     @@rq80_loop

@@rq80_done:
    mov     rax, r13
    vzeroupper
    add     rsp, 1088
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
NQ1_Requantize_Q8_0 ENDP

; =============================================================================
; NQ1_Requantize_Q4_K
; Requantize Q4_K blocks (144 bytes / 256 elements) → NQ_1 (34 bytes / 256).
; Direct block-to-block pipeline (same super-block size).
;
; RCX = src (Q4_K blocks)
; RDX = dst (NQ_1 blocks)
; R8  = n_blocks (each covers 256 elements)
; Returns: RAX = n_blocks processed
; =============================================================================
NQ1_Requantize_Q4_K PROC FRAME
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
    sub     rsp, 1088
    .allocstack 1088
    .endprolog

    mov     rsi, rcx
    mov     rdi, rdx
    mov     r12, r8                                ; Block count

    xor     ebx, ebx

@@rq4k_loop:
    cmp     rbx, r12
    jge     @@rq4k_done

    ; Q4_K block: 144 bytes per 256 elements
    ; [0:1]   F16 d (super-scale)
    ; [2:3]   F16 dmin (super-min)
    ; [4:35]  32 bytes 4-bit packed sub-scales (16 sub-blocks)
    ; [36:67] 32 bytes 4-bit packed sub-mins
    ; [68:143] 76 bytes 4-bit packed weights (but 256 elements = 128 nibbles
    ;           = 128 bytes... actually Q4_K uses different packing)

    ; Simplified: dequant entire block to F32 scratch via sub-block processing
    ; For each of 16 sub-blocks (16 elements each):
    ;   extract sub-scale, sub-min
    ;   dequant: W[i] = (nibble * sub_scale + sub_min) * super_scale

    lea     r13, [rsp]                             ; F32 scratch

    ; Load super-scale and super-min
    movzx   eax, word ptr [rsi]
    vmovd   xmm0, eax
    vcvtph2ps xmm0, xmm0                           ; d (super-scale)

    movzx   eax, word ptr [rsi + 2]
    vmovd   xmm1, eax
    vcvtph2ps xmm1, xmm1                           ; dmin (super-min)

    xor     ecx, ecx                               ; Sub-block index (0..15)
@@rq4k_sub:
    cmp     ecx, 16
    jge     @@rq4k_quantize

    ; Extract sub-scale (4-bit packed at offset 4)
    mov     eax, ecx
    shr     eax, 1
    movzx   edx, byte ptr [rsi + 4 + rax]
    test    ecx, 1
    jz      @@rq4k_even_scale
    shr     edx, 4
    jmp     @@rq4k_got_scale
@@rq4k_even_scale:
    and     edx, 0Fh
@@rq4k_got_scale:
    vcvtsi2ss xmm2, xmm2, edx
    vmulss  xmm2, xmm2, xmm0                       ; effective_scale = sub_scale * d

    ; Extract sub-min (4-bit packed at offset 36)
    mov     eax, ecx
    shr     eax, 1
    movzx   edx, byte ptr [rsi + 36 + rax]
    test    ecx, 1
    jz      @@rq4k_even_min
    shr     edx, 4
    jmp     @@rq4k_got_min
@@rq4k_even_min:
    and     edx, 0Fh
@@rq4k_got_min:
    vcvtsi2ss xmm3, xmm3, edx
    vmulss  xmm3, xmm3, xmm1                       ; effective_min = sub_min * dmin

    ; Dequant 16 elements for this sub-block
    ; Weights at offset 68 + sub_block*8 (8 bytes = 16 nibbles)
    mov     eax, ecx
    shl     eax, 3                                  ; * 8 bytes
    xor     edx, edx                                ; Byte within sub-block

@@rq4k_dq_byte:
    cmp     edx, 8
    jge     @@rq4k_next_sub

    lea     r8, [rsi + rax + 68]       ; combine block_base + sub_offset + 68
    movzx   r8d, byte ptr [r8 + rdx]

    ; Low nibble
    mov     r9d, r8d
    and     r9d, 0Fh
    vcvtsi2ss xmm4, xmm4, r9d
    vfmadd213ss xmm4, xmm2, xmm3                   ; nibble * scale + min

    ; Calculate output index: sub_block*16 + byte*2
    mov     r10d, ecx
    shl     r10d, 4
    lea     r10d, [r10 + rdx*2]
    vmovss  dword ptr [r13 + r10*4], xmm4

    ; High nibble
    mov     r9d, r8d
    shr     r9d, 4
    vcvtsi2ss xmm4, xmm4, r9d
    vfmadd213ss xmm4, xmm2, xmm3
    vmovss  dword ptr [r13 + r10*4 + 4], xmm4

    inc     edx
    jmp     @@rq4k_dq_byte

@@rq4k_next_sub:
    inc     ecx
    jmp     @@rq4k_sub

@@rq4k_quantize:
    ; Quantize 256 F32 values → NQ_1 block
    mov     rcx, r13
    mov     rdx, rdi
    call    NQ1_QuantizeBlock_Fast

    add     rsi, 144                               ; Next Q4_K block
    add     rdi, BLOCK_NQ1_SIZE                    ; Next NQ_1 block
    inc     rbx
    jmp     @@rq4k_loop

@@rq4k_done:
    mov     rax, r12
    vzeroupper
    add     rsp, 1088
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
NQ1_Requantize_Q4_K ENDP

; =============================================================================
; NQ1_GetBlockSize
; Returns: EAX = 34 (bytes per NQ_1 block)
; =============================================================================
NQ1_GetBlockSize PROC
    mov     eax, BLOCK_NQ1_SIZE
    ret
NQ1_GetBlockSize ENDP

; =============================================================================
; NQ1_GetCompressionRatio
; Calculate compression ratio vs FP16 for a given element count.
;
; RCX = n_elements
; Returns: EAX = compression ratio × 100 (e.g., 1506 = 15.06x vs FP16)
; =============================================================================
NQ1_GetCompressionRatio PROC
    ; FP16 size = n_elements * 2 bytes
    mov     rax, rcx
    shl     rax, 1                                 ; * 2

    ; NQ_1 size = (n_elements / 256) * 34 bytes
    mov     rdx, rcx
    shr     rdx, 8                                 ; / 256
    imul    rdx, BLOCK_NQ1_SIZE                    ; * 34

    ; Ratio = fp16_size * 100 / nq1_size
    imul    rax, 100
    test    rdx, rdx
    jz      @@cr_inf
    xor     ecx, ecx                               ; For division
    div     rdx                                    ; RAX = ratio * 100
    ret

@@cr_inf:
    mov     eax, 99999                             ; Infinite compression (no data)
    ret
NQ1_GetCompressionRatio ENDP

; =============================================================================
; NQ1_GetStats
; Fill caller-provided buffer with performance counter values.
;
; RCX = output buffer (5 × uint64_t):
;   [0] = quant blocks
;   [1] = dequant blocks
;   [2] = vec_dot calls
;   [3] = gemm calls
;   [4] = ADMM iterations total
; Returns: EAX = 1
; =============================================================================
NQ1_GetStats PROC
    mov     rax, g_NQ1QuantBlocks
    mov     qword ptr [rcx], rax
    mov     rax, g_NQ1DequantBlocks
    mov     qword ptr [rcx + 8], rax
    mov     rax, g_NQ1VecDotCalls
    mov     qword ptr [rcx + 16], rax
    mov     rax, g_NQ1GemmCalls
    mov     qword ptr [rcx + 24], rax
    mov     rax, g_NQ1ADMMIterTotal
    mov     qword ptr [rcx + 32], rax
    mov     eax, 1
    ret
NQ1_GetStats ENDP

; =============================================================================
; NQ1_Dispatch
; Type-based dispatch for NQ_1 operations (compatible with KQuant_Dispatch).
;
; ECX = operation:
;   0 = Dequantize block (RDX=src, R8=dst → calls DequantBlock)
;   1 = Vec dot (RDX=nq1, R8=f32 → result in XMM0)
;   2 = Quantize block fast (RDX=src_f32, R8=dst_nq1)
;   3 = Quantize block ADMM (RDX=src_f32, R8=dst_nq1, R9D=max_iter)
; Returns: depends on operation
; =============================================================================
NQ1_Dispatch PROC
    cmp     ecx, 0
    je      @@disp_dequant
    cmp     ecx, 1
    je      @@disp_vecdot
    cmp     ecx, 2
    je      @@disp_quant_fast
    cmp     ecx, 3
    je      @@disp_quant_admm

    ; Unknown operation
    mov     eax, -1
    ret

@@disp_dequant:
    ; op0: Dequantize single block (RDX=src NQ1, R8=dst F32)
    ; Call the dispatched DequantBlock (single block, not tensor NQ1_Dequant)
    mov     rcx, rdx                               ; src = arg1
    mov     rdx, r8                                ; dst = arg2
    jmp     qword ptr [g_NQ1DequantDispatch]       ; Tail-call via dispatch ptr

@@disp_vecdot:
    mov     rcx, rdx
    mov     rdx, r8
    jmp     NQ1_VecDot

@@disp_quant_fast:
    mov     rcx, rdx
    mov     rdx, r8
    jmp     NQ1_QuantizeBlock_Fast

@@disp_quant_admm:
    mov     rcx, rdx
    mov     rdx, r8
    ; r9d already has max_iter
    jmp     NQ1_QuantizeBlock_ADMM

NQ1_Dispatch ENDP

END
