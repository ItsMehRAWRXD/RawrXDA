; Fused Decode + FP8 Quantization Kernel
; Eliminates intermediate buffers by combining decode and quantize in one pass
; Input:  RCX = float* decoded_tokens (from model output)
;         RDX = uint8_t* output_fp8
;         R8  = size_t count
;         XMM3 = float scale
; Output: None (direct FP8 quantization)

EXTERN malloc:PROC
EXTERN free:PROC

.code

; =============================================================================
; Fused Decode + FP8 E4M3 Quantization - STREAMING VERSION
; Processes decoded tokens directly to FP8 without intermediate storage
; Uses AVX2 for 8-wide processing with memory prefetching
; =============================================================================
SovereignFusedDecodeFP8_AVX2 PROC FRAME
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

    mov     rsi, rcx            ; RSI = input float array (decoded)
    mov     rdi, rdx            ; RDI = output uint8 array (FP8)
    mov     r13, r8             ; R13 = count
    
    ; Early exit
    cmp     r13, 0
    jz      FusedDone
    
    ; Broadcast scale
    vbroadcastss ymm15, xmm3    ; YMM15 = scale
    vbroadcastss ymm14, dword ptr [fp8_e4m3_max]  ; YMM14 = 448.0
    vpcmpeqd ymm13, ymm13, ymm13
    vpsrld  ymm13, ymm13, 1     ; YMM13 = abs mask
    
    ; Process 8 elements at a time
    mov     r12, 0              ; Counter
    mov     r14, r13
    and     r14, -8             ; R14 = count rounded to multiple of 8

FusedVectorLoop:
    cmp     r12, r14
    jae     FusedVectorDone
    
    ; Prefetch next cache line (64 bytes ahead)
    prefetcht0 [rsi + r12*4 + 64]
    
    ; Load 8 floats (no intermediate - direct from decode)
    vmovaps ymm0, ymmword ptr [rsi + r12*4]
    
    ; Apply scale (fused: decode output → scale → quantize)
    vmulps  ymm0, ymm0, ymm15
    
    ; Extract sign
    vandps  ymm1, ymm0, ymmword ptr [sign_mask_8]
    vpsrld  ymm1, ymm1, 24
    
    ; Absolute value
    vandps  ymm0, ymm0, ymm13
    
    ; Clamp to E4M3 max
    vminps  ymm0, ymm0, ymm14
    
    ; Convert to int
    vcvtps2dq ymm0, ymm0
    
    ; Clamp to 0-255
    vpxor   ymm2, ymm2, ymm2
    vpmaxsd ymm0, ymm0, ymm2
    vpcmpeqd ymm3, ymm3, ymm3
    vpsrld  ymm3, ymm3, 24
    vpminsd ymm0, ymm0, ymm3
    
    ; Combine with sign
    vpor    ymm0, ymm0, ymm1
    
    ; Pack to 8 bytes
    vpackusdw ymm0, ymm0, ymm0
    vpackuswb ymm0, ymm0, ymm0
    
    ; Store 8 bytes (direct to output, no intermediate)
    vpextrq rax, xmm0, 0
    mov     qword ptr [rdi + r12], rax
    
    add     r12, 8
    jmp     FusedVectorLoop

FusedVectorDone:
    ; Handle remaining elements
    cmp     r12, r13
    jae     FusedDone

FusedScalarFallback:
    movss   xmm15, xmm3

FusedScalarLoop:
    cmp     r12, r13
    jae     FusedDone
    
    ; Load and quantize (scalar)
    movss   xmm0, dword ptr [rsi + r12*4]
    mulss   xmm0, xmm15
    
    ; Sign extraction
    movss   xmm1, xmm0
    mov     eax, 80000000h
    movd    xmm2, eax
    andps   xmm1, xmm2
    psrld   xmm1, 24
    
    ; Abs + clamp
    mov     eax, 7FFFFFFFh
    movd    xmm2, eax
    andps   xmm0, xmm2
    mov     eax, 43E00000h      ; 448.0
    movd    xmm2, eax
    minss   xmm0, xmm2
    
    ; Convert
    cvtss2si eax, xmm0
    cmp     eax, 255
    jle     FusedCheckMin
    mov     eax, 255
    jmp     FusedCombine

FusedCheckMin:
    cmp     eax, 0
    jge     FusedCombine
    xor     eax, eax

FusedCombine:
    movd    ebx, xmm1
    and     ebx, 80h
    or      al, bl
    mov     byte ptr [rdi + r12], al
    
    inc     r12
    jmp     FusedScalarLoop

FusedDone:
    vzeroupper
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
SovereignFusedDecodeFP8_AVX2 ENDP

; =============================================================================
; Cache-Optimized Streaming FP8 Pipeline
; Processes tokens in cache-friendly chunks with prefetching
; =============================================================================
SovereignStreamingFP8Pipeline_AVX2 PROC FRAME
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

    mov     rsi, rcx            ; Input
    mov     rdi, rdx            ; Output
    mov     r13, r8             ; Count
    
    cmp     r13, 0
    jz      StreamDone
    
    ; Constants
    vbroadcastss ymm15, xmm3    ; Scale
    vbroadcastss ymm14, dword ptr [fp8_e4m3_max]
    vpcmpeqd ymm13, ymm13, ymm13
    vpsrld  ymm13, ymm13, 1
    
    ; Process in 64-token chunks (one cache line of output)
    mov     r12, 0
    mov     r14, r13
    shr     r14, 6              ; r14 = count / 64
    shl     r14, 6              ; r14 = floor(count / 64) * 64

StreamChunkLoop:
    cmp     r12, r14
    jae     StreamChunkDone
    
    ; Prefetch next 2 cache lines
    prefetcht0 [rsi + r12*4 + 256]
    prefetcht0 [rsi + r12*4 + 320]
    
    ; Process 64 tokens (8 AVX2 iterations)
    mov     rbx, 0
StreamInnerLoop:
    cmp     rbx, 64
    jae     StreamInnerDone
    
    lea     rax, [r12 + rbx]
    vmovaps ymm0, ymmword ptr [rsi + rax*4]
    
    ; Fused: scale + quantize
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
    
    ; Store
    vpextrq rcx, xmm0, 0
    mov     qword ptr [rdi + rax], rcx
    
    add     rbx, 8
    jmp     StreamInnerLoop

StreamInnerDone:
    add     r12, 64
    jmp     StreamChunkLoop

StreamChunkDone:
    ; Handle remaining tokens
    cmp     r12, r13
    jae     StreamDone

StreamScalarFallback:
    movss   xmm15, xmm3

StreamScalarLoop:
    cmp     r12, r13
    jae     StreamDone
    
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
    mov     eax, 43E00000h
    movd    xmm2, eax
    minss   xmm0, xmm2
    cvtss2si eax, xmm0
    cmp     eax, 255
    jle     StreamCheckMin
    mov     eax, 255
    jmp     StreamCombine

StreamCheckMin:
    cmp     eax, 0
    jge     StreamCombine
    xor     eax, eax

StreamCombine:
    movd    ebx, xmm1
    and     ebx, 80h
    or      al, bl
    mov     byte ptr [rdi + r12], al
    
    inc     r12
    jmp     StreamScalarLoop

StreamDone:
    vzeroupper
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
SovereignStreamingFP8Pipeline_AVX2 ENDP

; =============================================================================
; Data Section
; =============================================================================
.data
ALIGN 16
fp8_e4m3_max    real4 448.0
sign_mask_8     dd 8 dup (80000000h)
abs_mask_8      dd 8 dup (7FFFFFFFh)

END
