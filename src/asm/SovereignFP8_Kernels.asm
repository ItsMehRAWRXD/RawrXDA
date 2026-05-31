; =============================================================================
; SovereignFP8_Kernels.asm - Pure MASM64 FP8 Quantization for RX 7800 XT
; Target: AMD RDNA3 (GFX1100) - Zero driver telemetry, sovereign compute
; =============================================================================
; E4M3: 1 sign bit, 4 exponent bits (bias 7), 3 mantissa bits
; E5M2: 1 sign bit, 5 exponent bits (bias 15), 2 mantissa bits
; =============================================================================

OPTION DOTNAME
OPTION CASEMAP:NONE

; =============================================================================
; External Includes
; =============================================================================
INCLUDE rawrxd_win64.inc

; =============================================================================
; Constants
; =============================================================================
E4M3_BIAS_VAL       EQU 7
E4M3_MAX_EXP_VAL    EQU 8
E4M3_MIN_EXP_VAL    EQU -7
E5M2_BIAS_VAL       EQU 15
E5M2_MAX_EXP_VAL    EQU 16
E5M2_MIN_EXP_VAL    EQU -15
F32_BIAS_VAL        EQU 127

; =============================================================================
; Data Section
; =============================================================================
.DATA
ALIGN 16

; Stochastic rounding seed
SR_SEED             DD 12345678h

; =============================================================================
; Code Section
; =============================================================================
.CODE
ALIGN 16

; =============================================================================
; SovereignQuantizeE4M3
; Purpose: Quantize float32 array to E4M3 format using MASM64
; Input:   RCX = float* input, RDX = uint8_t* output, R8 = count, XMM3 = scale
; Output:  None (writes to output buffer)
; Clobber: RAX, R9-R11, XMM4-XMM7
; =============================================================================
SovereignQuantizeE4M3 PROC FRAME
    ; Prologue
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    push    rbx
    .pushreg rbx
    sub     rsp, 40
    .allocstack 40
    .endprolog

    ; Save parameters
    mov     rsi, rcx                    ; RSI = input
    mov     rdi, rdx                    ; RDI = output
    mov     rbx, r8                     ; RBX = count
    movss   xmm6, xmm3                  ; XMM6 = scale

    ; Process elements
    xor     rcx, rcx                    ; Index = 0

.quantize_loop:
    cmp     rcx, rbx
    jge     .done

    ; Load float
    movss   xmm0, dword ptr [rsi + rcx*4]

    ; Apply scale
    mulss   xmm0, xmm6

    ; Extract sign bit
    movss   xmm1, xmm0
    movd    eax, xmm1
    and     eax, 080000000h             ; Sign mask
    shr     eax, 24                     ; Move to bit 7 position
    mov     r8d, eax                    ; Save sign

    ; Extract exponent
    movss   xmm1, xmm0
    movd    eax, xmm1
    and     eax, 07F800000h             ; Exponent mask
    shr     eax, 23                     ; Raw exponent
    sub     eax, F32_BIAS_VAL           ; Unbias
    add     eax, E4M3_BIAS_VAL          ; Apply E4M3 bias

    ; Clamp exponent
    cmp     eax, E4M3_MAX_EXP_VAL
    jle     .exp_not_max
    mov     eax, E4M3_MAX_EXP_VAL
.exp_not_max:
    cmp     eax, E4M3_MIN_EXP_VAL
    jge     .exp_not_min
    mov     eax, E4M3_MIN_EXP_VAL
.exp_not_min:

    mov     r9d, eax                    ; Save exponent

    ; Extract mantissa (top 3 bits)
    movss   xmm1, xmm0
    movd    eax, xmm1
    and     eax, 0007FFFFFh             ; Mantissa mask
    shr     eax, 20                     ; Top 3 bits
    mov     r10d, eax                   ; Save mantissa

    ; Stochastic rounding (simplified - add random bit)
    rdrand  eax
    and     eax, 1
    add     r10d, eax

    ; Clamp mantissa
    cmp     r10d, 7
    jle     .man_ok
    mov     r10d, 7
    inc     r9d                         ; Carry to exponent
.man_ok:

    ; Combine: sign(1) | exp(4) | man(3)
    shl     r9d, 3                      ; Exponent to bits 6:3
    or      r9d, r10d                   ; Add mantissa
    or      r9d, r8d                    ; Add sign

    ; Store result
    mov     byte ptr [rdi + rcx], r9b

    inc     rcx
    jmp     .quantize_loop

.done:
    ; Epilogue
    add     rsp, 40
    pop     rbx
    pop     rdi
    pop     rsi
    pop     rbp
    ret
SovereignQuantizeE4M3 ENDP

; =============================================================================
; SovereignQuantizeE5M2
; Purpose: Quantize float32 array to E5M2 format
; Input:   RCX = float* input, RDX = uint8_t* output, R8 = count, XMM3 = scale
; =============================================================================
SovereignQuantizeE5M2 PROC FRAME
    ; Prologue
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    push    rbx
    .pushreg rbx
    sub     rsp, 40
    .allocstack 40
    .endprolog

    mov     rsi, rcx
    mov     rdi, rdx
    mov     rbx, r8
    movss   xmm6, xmm3

    xor     rcx, rcx

.quantize_loop:
    cmp     rcx, rbx
    jge     .done

    movss   xmm0, dword ptr [rsi + rcx*4]
    mulss   xmm0, xmm6

    ; Sign
    movss   xmm1, xmm0
    movd    eax, xmm1
    and     eax, 080000000h
    shr     eax, 24
    mov     r8d, eax

    ; Exponent
    movd    eax, xmm0
    and     eax, 07F800000h
    shr     eax, 23
    sub     eax, F32_BIAS_VAL
    add     eax, E5M2_BIAS_VAL

    cmp     eax, E5M2_MAX_EXP_VAL
    jle     .exp_not_max
    mov     eax, E5M2_MAX_EXP_VAL
.exp_not_max:
    cmp     eax, E5M2_MIN_EXP_VAL
    jge     .exp_not_min
    mov     eax, E5M2_MIN_EXP_VAL
.exp_not_min:

    mov     r9d, eax

    ; Mantissa (2 bits)
    movd    eax, xmm0
    and     eax, 0007FFFFFh
    shr     eax, 21                     ; Top 2 bits
    mov     r10d, eax

    ; Stochastic rounding
    rdrand  eax
    and     eax, 1
    add     r10d, eax

    cmp     r10d, 3
    jle     .man_ok
    mov     r10d, 3
    inc     r9d
.man_ok:

    ; Combine: sign(1) | exp(5) | man(2)
    shl     r9d, 2
    or      r9d, r10d
    or      r9d, r8d

    mov     byte ptr [rdi + rcx], r9b

    inc     rcx
    jmp     .quantize_loop

.done:
    add     rsp, 40
    pop     rbx
    pop     rdi
    pop     rsi
    pop     rbp
    ret
SovereignQuantizeE5M2 ENDP

; =============================================================================
; SovereignDoubleBufferInit
; Purpose: Initialize lock-free double buffer for KV-cache streaming
; Input:   RCX = buffer_size in bytes
; Output:  RAX = pointer to buffer structure
; =============================================================================
SovereignDoubleBufferInit PROC FRAME
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    sub     rsp, 40
    .allocstack 40
    .endprolog

    mov     rbx, rcx                    ; Save size

    ; Allocate with 64-byte alignment
    mov     rcx, rbx
    add     rcx, 128                    ; Extra for header + alignment
    mov     rdx, 64                     ; Alignment
    call    aligned_alloc_wrapper

    test    rax, rax
    jz      .alloc_failed

    mov     rsi, rax                    ; RSI = base

    ; Initialize header at base
    mov     qword ptr [rsi], 0          ; buf_a offset (will be +64)
    mov     qword ptr [rsi+8], 0        ; buf_b offset
    mov     qword ptr [rsi+16], 0       ; active_flag
    mov     qword ptr [rsi+24], rbx     ; size

    ; Calculate aligned buffer addresses
    lea     rax, [rsi + 64]
    mov     [rsi], rax                  ; buf_a = base + 64
    add     rax, rbx
    mov     [rsi+8], rax                ; buf_b = buf_a + size

    mov     rax, rsi                    ; Return base
    jmp     .done

.alloc_failed:
    xor     rax, rax

.done:
    add     rsp, 40
    pop     rsi
    pop     rbx
    pop     rbp
    ret
SovereignDoubleBufferInit ENDP

; =============================================================================
; aligned_alloc_wrapper
; Simple wrapper to allocate aligned memory
; Input: RCX = size, RDX = alignment
; Output: RAX = pointer or NULL
; =============================================================================
aligned_alloc_wrapper PROC
    push    rcx
    push    rdx
    sub     rsp, 32

    ; Use _aligned_malloc
    mov     r8, rdx                     ; Alignment
    mov     rdx, rcx                    ; Size
    xor     ecx, ecx                    ; Not used
    call    _aligned_malloc

    add     rsp, 32
    pop     rdx
    pop     rcx
    ret
aligned_alloc_wrapper ENDP

; =============================================================================
; SovereignDoubleBufferSwap
; Purpose: Atomic swap of double buffer (lock-free)
; Input:   RCX = pipeline pointer
; Output:  RAX = new active buffer pointer
; =============================================================================
SovereignDoubleBufferSwap PROC FRAME
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    push    rbx
    .pushreg rbx
    .endprolog

    mov     rbx, rcx

    ; Atomic XOR of active flag (0 <-> 1)
    mov     rax, 1
    lock xor qword ptr [rbx+16], rax

    ; Return active buffer pointer
    mov     rax, [rbx+16]
    test    rax, rax
    jnz     .buf_b_active

    mov     rax, [rbx]                  ; Return buf_a
    jmp     .done

.buf_b_active:
    mov     rax, [rbx+8]                ; Return buf_b

.done:
    pop     rbx
    pop     rbp
    ret
SovereignDoubleBufferSwap ENDP

; =============================================================================
; SovereignGPUFlush
; Purpose: Ensure GPU memory visibility (MFENCE + SFENCE)
; =============================================================================
SovereignGPUFlush PROC FRAME
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    .endprolog

    mfence
    sfence

    pop     rbp
    ret
SovereignGPUFlush ENDP

; =============================================================================
; Exports
; =============================================================================
PUBLIC SovereignQuantizeE4M3
PUBLIC SovereignQuantizeE5M2
PUBLIC SovereignDoubleBufferInit
PUBLIC SovereignDoubleBufferSwap
PUBLIC SovereignGPUFlush

END
; =============================================================================
; SovereignFP8_Kernels.asm - Pure MASM64 FP8 Quantization for RX 7800 XT
; Target: AMD RDNA3 (GFX1100) - Zero driver telemetry, sovereign compute
; =============================================================================
; E4M3: 1 sign bit, 4 exponent bits (bias 7), 3 mantissa bits
; E5M2: 1 sign bit, 5 exponent bits (bias 15), 2 mantissa bits
; =============================================================================

OPTION DOTNAME
OPTION CASEMAP:NONE

; =============================================================================
; External Includes
; =============================================================================
INCLUDE rawrxd_win64.inc

; =============================================================================
; Constants
; =============================================================================
E4M3_BIAS           EQU 7
E4M3_MAX_EXP        EQU 8
E4M3_MIN_EXP        EQU -7
E5M2_BIAS           EQU 15
E5M2_MAX_EXP        EQU 16
E5M2_MIN_EXP        EQU -15

; Stochastic rounding state (per-thread)
SR_STATE_SIZE       EQU 8

; =============================================================================
; Data Section
; =============================================================================
.DATA
ALIGN 16

; Lookup tables for fast conversion
E4M3_EXP_TABLE      DB 256 DUP(0)       ; Exponent encoding table
E4M3_MAN_TABLE      DB 256 DUP(0)       ; Mantissa rounding table
E5M2_EXP_TABLE      DB 256 DUP(0)
E5M2_MAN_TABLE      DB 256 DUP(0)

; Stochastic rounding seeds (thread-local via TLS)
SR_SEED             DD 0x12345678

; Constants for bit manipulation
F32_SIGN_MASK       DD 0x80000000
F32_EXP_MASK        DD 0x7F800000
F32_MAN_MASK        DD 0x007FFFFF
F32_EXP_BIAS        DD 127

; =============================================================================
; Code Section
; =============================================================================
.CODE
ALIGN 16

; =============================================================================
; SovereignQuantizeE4M3
; Purpose: Quantize float32 array to E4M3 format using MASM64
; Input:   RCX = float* input, RDX = uint8_t* output, XMM2 = scale
; Output:  None (writes to output buffer)
; Clobber: RAX, R8-R11, XMM3-XMM7
; =============================================================================
SovereignQuantizeE4M3 PROC FRAME
    ; Prologue
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    sub     rsp, 32
    .allocstack 32
    .endprolog

    ; Save parameters
    mov     rsi, rcx                    ; RSI = input
    mov     rdi, rdx                    ; RDI = output
    movss   xmm6, xmm2                  ; XMM6 = scale (preserved)

    ; Load constants
    vbroadcastss ymm7, xmm6             ; YMM7 = scale broadcasted

    ; Process 8 floats at a time (AVX2)
    mov     rcx, 256                    ; Process 256 elements
    shr     rcx, 3                      ; 32 iterations of 8

.quantize_loop:
    ; Load 8 floats
    vmovups ymm0, [rsi]

    ; Apply scale
    vmulps  ymm0, ymm0, ymm7

    ; Extract sign bits
    vandps  ymm1, ymm0, ymmword ptr [F32_SIGN_MASK]
    vpsrld  ymm1, ymm1, 24              ; Sign bit to position 7

    ; Extract exponent
    vandps  ymm2, ymm0, ymmword ptr [F32_EXP_MASK]
    vpsrld  ymm2, ymm2, 23              ; Raw exponent
    vpsubd  ymm2, ymm2, ymmword ptr [F32_EXP_BIAS]
    vpaddd  ymm2, ymm2, ymmword ptr [E4M3_BIAS]  ; Apply E4M3 bias

    ; Clamp exponent to E4M3 range
    vpminsd ymm2, ymm2, ymmword ptr [E4M3_MAX_EXP]
    vpmaxsd ymm2, ymm2, ymmword ptr [E4M3_MIN_EXP]

    ; Extract mantissa
    vandps  ymm3, ymm0, ymmword ptr [F32_MAN_MASK]
    vpsrld  ymm3, ymm3, 20              ; Top 3 bits of mantissa

    ; Stochastic rounding: add random bits to mantissa
    call    SovereignStochasticRound
    vpaddd  ymm3, ymm3, ymm0            ; Add rounding offset

    ; Combine: sign | exponent | mantissa
    vpslld  ymm2, ymm2, 3               ; Exponent to bits 6:3
    vpor    ymm2, ymm2, ymm3            ; Add mantissa
    vpor    ymm2, ymm2, ymm1            ; Add sign

    ; Pack 8x32-bit to 8x8-bit
    vpackusdw ymm2, ymm2, ymm2
    vpackuswb ymm2, ymm2, ymm2
    vpermq  ymm2, ymm2, 0x08            ; Pack to low 64 bits

    ; Store result
    vmovq   [rdi], xmm2

    ; Advance pointers
    add     rsi, 32                     ; 8 floats * 4 bytes
    add     rdi, 8                      ; 8 bytes output

    dec     rcx
    jnz     .quantize_loop

    ; Epilogue
    add     rsp, 32
    pop     rdi
    pop     rsi
    pop     rbx
    ret
SovereignQuantizeE4M3 ENDP

; =============================================================================
; SovereignQuantizeE5M2
; Purpose: Quantize float32 array to E5M2 format
; Input:   RCX = float* input, RDX = uint8_t* output, XMM2 = scale
; =============================================================================
SovereignQuantizeE5M2 PROC FRAME
    ; Prologue
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    sub     rsp, 32
    .allocstack 32
    .endprolog

    mov     rsi, rcx
    mov     rdi, rdx
    movss   xmm6, xmm2
    vbroadcastss ymm7, xmm6

    mov     rcx, 256
    shr     rcx, 3

.quantize_loop:
    vmovups ymm0, [rsi]
    vmulps  ymm0, ymm0, ymm7

    ; Sign extraction
    vandps  ymm1, ymm0, ymmword ptr [F32_SIGN_MASK]
    vpsrld  ymm1, ymm1, 24

    ; Exponent extraction and bias conversion
    vandps  ymm2, ymm0, ymmword ptr [F32_EXP_MASK]
    vpsrld  ymm2, ymm2, 23
    vpsubd  ymm2, ymm2, ymmword ptr [F32_EXP_BIAS]
    vpaddd  ymm2, ymm2, ymmword ptr [E5M2_BIAS]

    ; Clamp to E5M2 range
    vpminsd ymm2, ymm2, ymmword ptr [E5M2_MAX_EXP]
    vpmaxsd ymm2, ymm2, ymmword ptr [E5M2_MIN_EXP]

    ; Mantissa (2 bits)
    vandps  ymm3, ymm0, ymmword ptr [F32_MAN_MASK]
    vpsrld  ymm3, ymm3, 21              ; Top 2 bits

    ; Stochastic rounding
    call    SovereignStochasticRound
    vpaddd  ymm3, ymm3, ymm0

    ; Combine: sign(1) | exp(5) | man(2)
    vpslld  ymm2, ymm2, 2               ; Exponent to bits 6:2
    vpor    ymm2, ymm2, ymm3
    vpor    ymm2, ymm2, ymm1

    ; Pack and store
    vpackusdw ymm2, ymm2, ymm2
    vpackuswb ymm2, ymm2, ymm2
    vpermq  ymm2, ymm2, 0x08
    vmovq   [rdi], xmm2

    add     rsi, 32
    add     rdi, 8
    dec     rcx
    jnz     .quantize_loop

    add     rsp, 32
    pop     rdi
    pop     rsi
    pop     rbx
    ret
SovereignQuantizeE5M2 ENDP

; =============================================================================
; SovereignStochasticRound
; Purpose: Generate stochastic rounding offset using xoshiro128+ PRNG
; Output:  YMM0 = 8 random 32-bit values (low bits only)
; Clobber: RAX, RDX
; =============================================================================
SovereignStochasticRound PROC
    ; Load seed from TLS
    mov     eax, [SR_SEED]

    ; xoshiro128+ step
    mov     edx, eax
    shl     edx, 13
    xor     eax, edx
    mov     edx, eax
    shr     edx, 7
    xor     eax, edx
    mov     edx, eax
    shl     edx, 17
    xor     eax, edx

    ; Store updated seed
    mov     [SR_SEED], eax

    ; Broadcast to all lanes
    vmovd   xmm0, eax
    vpbroadcastd ymm0, xmm0

    ; Mask to get stochastic bits
    vpand   ymm0, ymm0, ymmword ptr [STOCH_MASK]

    ret
SovereignStochasticRound ENDP

; =============================================================================
; SovereignDoubleBufferInit
; Purpose: Initialize lock-free double buffer for KV-cache streaming
; Input:   RCX = buffer_size in bytes
; Output:  RAX = pointer to buffer structure
; =============================================================================
SovereignDoubleBufferInit PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 40
    .allocstack 40
    .endprolog

    ; Allocate aligned memory for double buffer
    ; Structure: [buf_a_ptr][buf_b_ptr][active_flag][size]
    mov     rbx, rcx                    ; Save size
    add     rcx, 64 + 32                ; Size + alignment padding + header

    ; Allocate with VirtualAlloc for page alignment
    mov     r8d, PAGE_READWRITE
    mov     edx, MEM_COMMIT or MEM_RESERVE
    xor     r9d, r9d                    ; No preferred address
    mov     rcx, rbx
    add     rcx, 4096                   ; Page align
    call    VirtualAlloc

    test    rax, rax
    jz      .alloc_failed

    ; Initialize buffer structure
    mov     [rax], rax                  ; buf_a = base + 32
    add     qword ptr [rax], 32
    mov     rdx, [rax]
    add     rdx, rbx
    mov     [rax+8], rdx                ; buf_b = buf_a + size
    mov     qword ptr [rax+16], 0       ; active_flag = 0 (A active)
    mov     [rax+24], rbx               ; size

.alloc_failed:
    add     rsp, 40
    pop     rbx
    ret
SovereignDoubleBufferInit ENDP

; =============================================================================
; SovereignDoubleBufferSwap
; Purpose: Atomic swap of double buffer (lock-free)
; Input:   RCX = pipeline pointer
; Output:  RAX = new active buffer pointer
; =============================================================================
SovereignDoubleBufferSwap PROC FRAME
    push    rbx
    .pushreg rbx
    .endprolog

    mov     rbx, rcx

    ; Atomic XOR of active flag (0 <-> 1)
    lock xor qword ptr [rbx+16], 1

    ; Return active buffer pointer
    mov     rax, [rbx+16]
    test    rax, rax
    jnz     .buf_b_active

    mov     rax, [rbx]                  ; Return buf_a
    jmp     .done

.buf_b_active:
    mov     rax, [rbx+8]                ; Return buf_b

.done:
    pop     rbx
    ret
SovereignDoubleBufferSwap ENDP

; =============================================================================
; SovereignGPUFlush
; Purpose: Ensure GPU memory visibility (MFENCE + SFENCE)
; =============================================================================
SovereignGPUFlush PROC FRAME
    mfence
    sfence
    ret
SovereignGPUFlush ENDP

; =============================================================================
; Data Section - Constants
; =============================================================================
.DATA
ALIGN 32

STOCH_MASK          DD 8 DUP(0x00000007)    ; 3 bits for E4M3 rounding

; Exponent bias constants (broadcasted)
F32_EXP_BIAS        DD 8 DUP(127)
E4M3_BIAS           DD 8 DUP(7)
E4M3_MAX_EXP        DD 8 DUP(8)
E4M3_MIN_EXP        DD 8 DUP(-7)
E5M2_BIAS           DD 8 DUP(15)
E5M2_MAX_EXP        DD 8 DUP(16)
E5M2_MIN_EXP        DD 8 DUP(-15)

; Sign mask broadcasted
F32_SIGN_MASK       DD 8 DUP(0x80000000)
F32_EXP_MASK        DD 8 DUP(0x7F800000)
F32_MAN_MASK        DD 8 DUP(0x007FFFFF)

; =============================================================================
; Exports
; =============================================================================
PUBLIC SovereignQuantizeE4M3
PUBLIC SovereignQuantizeE5M2
PUBLIC SovereignStochasticRound
PUBLIC SovereignDoubleBufferInit
PUBLIC SovereignDoubleBufferSwap
PUBLIC SovereignGPUFlush

END
