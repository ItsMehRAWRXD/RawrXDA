; Sovereign FP8 Quantizer - AVX2 Vectorized Implementation
; Processes 8 floats per iteration (256-bit SIMD)
; Targets: 8x throughput improvement over scalar path
; Author: RawrXD Core Team

EXTERN malloc:PROC
EXTERN free:PROC

.code

; =============================================================================
; AVX2 FP8 E4M3 Quantization Kernel - VECTORIZED VERSION
; Converts 32-bit floats to 8-bit E4M3 format, 8 elements at a time
; Input:  RCX = float* input (32-byte aligned), RDX = uint8_t* output, 
;         R8 = size_t count, XMM3 = float scale
; Output: None (in-place quantization)
; Requirements: count must be multiple of 8, input must be 32-byte aligned
; =============================================================================
SovereignQuantizeE4M3_AVX2 PROC FRAME
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    .pushreg rbx
    .pushreg rsi
    .pushreg rdi
    .pushreg r12
    .pushreg r13
    .pushreg r14
    .endprolog

    mov     rsi, rcx            ; RSI = input float array (32-byte aligned)
    mov     rdi, rdx            ; RDI = output uint8 array
    mov     r13, r8             ; R13 = count
    
    ; Early exit if count < 8
    cmp     r13, 8
    jb      ScalarFallback
    
    ; Broadcast scale to all lanes of YMM register
    vbroadcastss ymm15, xmm3    ; YMM15 = {scale, scale, scale, scale, scale, scale, scale, scale}
    
    ; Load constants into YMM registers
    vbroadcastss ymm14, dword ptr [fp8_e4m3_max]     ; YMM14 = 448.0 (E4M3 max)
    vpcmpeqd ymm13, ymm13, ymm13                       ; YMM13 = all 1s (for absolute value)
    vpsrld  ymm13, ymm13, 1                            ; YMM13 = 0x7FFFFFFF7FFFFFFF... (abs mask)
    
    ; Process 8 elements at a time
    mov     r12, 0              ; Counter = 0
    mov     r14, r13
    and     r14, -8             ; R14 = count rounded down to multiple of 8

VectorLoop:
    cmp     r12, r14
    jae     VectorDone
    
    ; Load 8 floats (256 bits)
    vmovaps ymm0, ymmword ptr [rsi + r12*4]
    
    ; Apply scale: ymm0 = input * scale
    vmulps  ymm0, ymm0, ymm15
    
    ; Extract sign bits (bit 31 of each float)
    vandps  ymm1, ymm0, ymmword ptr [sign_mask_8]     ; ymm1 = sign bits
    vpsrld  ymm1, ymm1, 24                              ; Move sign to bit 7 position
    
    ; Absolute value: ymm0 = |input * scale|
    vandps  ymm0, ymm0, ymm13
    
    ; Clamp to E4M3 max (448.0)
    vminps  ymm0, ymm0, ymm14
    
    ; Convert to integers (8 floats -> 8 int32s)
    vcvtps2dq ymm0, ymm0
    
    ; Clamp to 0-255 range (uint8)
    vpxor   ymm2, ymm2, ymm2                            ; ymm2 = 0
    vpmaxsd ymm0, ymm0, ymm2                            ; max(0, value)
    vpcmpeqd ymm3, ymm3, ymm3                           ; ymm3 = -1
    vpsrld  ymm3, ymm3, 24                              ; ymm3 = 0xFF
    vpminsd ymm0, ymm0, ymm3                            ; min(value, 255)
    
    ; Combine with sign bits
    vpor    ymm0, ymm0, ymm1
    
    ; Pack 8 int32s down to 8 int8s
    ; First pack to 16-bit
    vpackusdw ymm0, ymm0, ymm0                          ; Pack to 16-bit unsigned
    ; Then pack to 8-bit
    vpackuswb ymm0, ymm0, ymm0                          ; Pack to 8-bit unsigned
    
    ; Extract lower 64 bits and store
    vpextrq rax, xmm0, 0                                ; Extract lower 64 bits
    mov     qword ptr [rdi + r12], rax                  ; Store 8 bytes
    
    add     r12, 8
    jmp     VectorLoop

VectorDone:
    ; Handle remaining elements (0-7) with scalar fallback
    cmp     r12, r13
    jae     QuantizeAVX2Done

ScalarFallback:
    ; Process remaining elements one at a time
    ; Scale is still in XMM3 from original call
    movss   xmm15, xmm3

ScalarFallbackLoop:
    cmp     r12, r13
    jae     QuantizeAVX2Done
    
    ; Load single float
    movss   xmm0, dword ptr [rsi + r12*4]
    
    ; Apply scale
    mulss   xmm0, xmm15
    
    ; Extract sign bit
    movss   xmm1, xmm0
    mov     eax, 80000000h
    movd    xmm2, eax
    andps   xmm1, xmm2
    psrld   xmm1, 24
    
    ; Absolute value
    mov     eax, 7FFFFFFFh
    movd    xmm2, eax
    andps   xmm0, xmm2
    
    ; Clamp to E4M3 max
    mov     eax, 43E00000h      ; 448.0
    movd    xmm2, eax
    minss   xmm0, xmm2
    
    ; Convert and clamp
    cvtss2si eax, xmm0
    cmp     eax, 255
    jle     CheckMinFallback
    mov     eax, 255
    jmp     CombineFallback

CheckMinFallback:
    cmp     eax, 0
    jge     CombineFallback
    xor     eax, eax

CombineFallback:
    ; Combine with sign
    movd    ebx, xmm1
    and     ebx, 80h
    or      al, bl
    
    ; Store
    mov     byte ptr [rdi + r12], al
    
    inc     r12
    jmp     ScalarFallbackLoop

QuantizeAVX2Done:
    vzeroupper                                          ; Clear upper YMM state
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
SovereignQuantizeE4M3_AVX2 ENDP

; =============================================================================
; AVX2 FP8 E5M2 Quantization Kernel - VECTORIZED VERSION
; Higher dynamic range, lower precision, 8 elements at a time
; =============================================================================
SovereignQuantizeE5M2_AVX2 PROC FRAME
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    .pushreg rbx
    .pushreg rsi
    .pushreg rdi
    .pushreg r12
    .pushreg r13
    .pushreg r14
    .endprolog

    mov     rsi, rcx
    mov     rdi, rdx
    mov     r13, r8
    
    cmp     r13, 8
    jb      E5M2_ScalarFallback
    
    vbroadcastss ymm15, xmm3
    vbroadcastss ymm14, dword ptr [fp8_e5m2_max]      ; 57344.0
    vpcmpeqd ymm13, ymm13, ymm13
    vpsrld  ymm13, ymm13, 1

    mov     r12, 0
    mov     r14, r13
    and     r14, -8

E5M2_VectorLoop:
    cmp     r12, r14
    jae     E5M2_VectorDone
    
    vmovaps ymm0, ymmword ptr [rsi + r12*4]
    vmulps  ymm0, ymm0, ymm15
    
    vandps  ymm1, ymm0, ymmword ptr [sign_mask_8]
    vpsrld  ymm1, ymm1, 24
    
    vandps  ymm0, ymm0, ymm13
    vminps  ymm0, ymm0, ymm14
    
    vcvtps2dq ymm0, ymm0
    
    vpxor   ymm2, ymm2, ymm2
    vpmaxsd ymm0, ymm0, ymm2
    vpcmpeqd ymm3, ymm3, ymm3
    vpsrld  ymm3, ymm3, 24
    vpminsd ymm0, ymm0, ymm3
    
    vpor    ymm0, ymm0, ymm1
    
    vpackusdw ymm0, ymm0, ymm0
    vpackuswb ymm0, ymm0, ymm0
    
    vpextrq rax, xmm0, 0
    mov     qword ptr [rdi + r12], rax
    
    add     r12, 8
    jmp     E5M2_VectorLoop

E5M2_VectorDone:
    cmp     r12, r13
    jae     QuantizeE5M2_AVX2Done

E5M2_ScalarFallback:
    movss   xmm15, xmm3

E5M2_ScalarFallbackLoop:
    cmp     r12, r13
    jae     QuantizeE5M2_AVX2Done
    
    movss   xmm0, dword ptr [rsi + r12*4]
    mulss   xmm0, xmm15
    
    movss   xmm1, xmm0
    mov     eax, 80000000h
    movd    xmm2, eax
    andps   xmm1, xmm2
    psrld   xmm1, 24
    
    mov     eax, 7FFFFFFFh
    movd    xmm2, eax
    andps   xmm0, xmm2
    
    mov     eax, 47600000h      ; 57344.0
    movd    xmm2, eax
    minss   xmm0, xmm2
    
    cvtss2si eax, xmm0
    cmp     eax, 255
    jle     E5M2_CheckMin
    mov     eax, 255
    jmp     E5M2_Combine

E5M2_CheckMin:
    cmp     eax, 0
    jge     E5M2_Combine
    xor     eax, eax

E5M2_Combine:
    movd    ebx, xmm1
    and     ebx, 80h
    or      al, bl
    mov     byte ptr [rdi + r12], al
    
    inc     r12
    jmp     E5M2_ScalarFallbackLoop

QuantizeE5M2_AVX2Done:
    vzeroupper
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
SovereignQuantizeE5M2_AVX2 ENDP

; =============================================================================
; AVX2 Batch Quantization - Zero-Copy Interface for Pipeline
; Processes exactly 64 tokens (8 AVX2 registers worth)
; Input: RCX = float* input (256-byte aligned), RDX = uint8_t* output
;        R8 = batch_size (must be 64), XMM3 = scale
; =============================================================================
SovereignQuantizeBatch64_AVX2 PROC FRAME
    push    rbx
    push    rsi
    push    rdi
    push    r12
    .pushreg rbx
    .pushreg rsi
    .pushreg rdi
    .pushreg r12
    .endprolog

    mov     rsi, rcx            ; RSI = input (256-byte aligned)
    mov     rdi, rdx            ; RDI = output
    
    ; Verify batch size is 64
    cmp     r8, 64
    jne     Batch64_Done        ; Early exit if wrong size
    
    ; Broadcast scale
    vbroadcastss ymm15, xmm3
    vbroadcastss ymm14, dword ptr [fp8_e4m3_max]
    vpcmpeqd ymm13, ymm13, ymm13
    vpsrld  ymm13, ymm13, 1
    
    ; Process 64 floats = 8 blocks of 8
    mov     r12, 0

Batch64_Loop:
    cmp     r12, 8
    jae     Batch64_Done
    
    ; Calculate offset: r12 * 32 bytes (8 floats)
    lea     rax, [r12*8]
    lea     rbx, [rsi + rax*4]  ; RBX = &input[r12*8]
    lea     rcx, [rdi + rax]    ; RCX = &output[r12*8]
    
    ; Load 8 floats
    vmovaps ymm0, ymmword ptr [rbx]
    
    ; Quantize
    vmulps  ymm0, ymm0, ymm15
    vandps  ymm1, ymm0, ymmword ptr [sign_mask_8]
    vpsrld  ymm1, ymm1, 24
    vandps  ymm0, ymm0, ymm13
    vminps  ymm0, ymm0, ymm14
    vcvtps2dq ymm0, ymm0
    vpxor   ymm2, ymm2, ymm2
    vpmaxsd ymm0, ymm0, ymm2
    vpcmpeqd ymm3, ymm3, ymm3
    vpsrld  ymm3, ymm3, 24
    vpminsd ymm0, ymm0, ymm3
    vpor    ymm0, ymm0, ymm1
    vpackusdw ymm0, ymm0, ymm0
    vpackuswb ymm0, ymm0, ymm0
    
    ; Store 8 bytes
    vpextrq rax, xmm0, 0
    mov     qword ptr [rcx], rax
    
    inc     r12
    jmp     Batch64_Loop

Batch64_Done:
    vzeroupper
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
SovereignQuantizeBatch64_AVX2 ENDP

; =============================================================================
; Data Section - Aligned Constants
; =============================================================================
.data
ALIGN 16
fp8_e4m3_max    real4 448.0
fp8_e5m2_max    real4 57344.0
sign_mask_8     dd 8 dup (80000000h)    ; 8 x sign bit masks for AVX2
abs_mask_8      dd 8 dup (7FFFFFFFh)    ; 8 x abs masks for AVX2

END
