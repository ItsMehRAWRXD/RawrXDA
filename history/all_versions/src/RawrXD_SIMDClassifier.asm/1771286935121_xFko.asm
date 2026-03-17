; ============================================================================
; RawrXD SIMD Classifier - MASM Implementation
; Generated: 2026-02-01
; Optimized for AVX2
; ============================================================================

option casemap:none

; ─── PUBLIC Exports ──────────────────────────────────────────────────────────
PUBLIC SIMD_Classify

.code

; ----------------------------------------------------------------------------
; SIMD_Classify
; RCX = Input Data Pointer (float*)
; RDX = Input Size (count of floats)
; R8  = Threshold (float)
; Returns: EAX = Count of elements greater than threshold
; ----------------------------------------------------------------------------
SIMD_Classify PROC FRAME
    .endprolog
    
    ; Setup locals
    xor rax, rax        ; Result count
    test rcx, rcx
    jz @done
    test rdx, rdx
    jz @done

    ; Broadcast threshold to YMM0
    vmovss xmm0, xmm2   ; xmm2 is R8 (float args come in XMM0-3 but R8 usually integer... waiting, float args: XMM0, XMM1, XMM2)
                        ; Calling convention: float args are XMM0, XMM1, XMM2...
                        ; Wait, Function signature: int SIMD_Classify(float* data, size_t size, float threshold)
                        ; RCX = data, RDX = size, XMM2 = threshold (3rd arg)
    
    vbroadcastss ymm0, xmm2
    
    xor r9, r9          ; Loop index
    
    ; Align check? Assume unaligned for safety
    
@loop_avx:
    mov r10, rdx
    sub r10, r9
    cmp r10, 8
    jl @loop_scalar     ; Less than 8 floats left
    
    ; Load 8 floats
    vmovups ymm1, ymmword ptr [rcx + r9*4]
    
    ; Compare > threshold
    vcmpgtps ymm2, ymm1, ymm0 
    
    ; Extract mask
    vmovmskps r11d, ymm2
    
    ; Count set bits
    popcnt r11d, r11d
    add eax, r11d
    
    add r9, 8
    jmp @loop_avx

@loop_scalar:
    cmp r9, rdx
    jge @done
    
    vmovss xmm1, dword ptr [rcx + r9*4]
    vcomiss xmm1, xmm2
    jbe @next_scalar
    inc eax
    
@next_scalar:
    inc r9
    jmp @loop_scalar

@done:
    ret
SIMD_Classify ENDP

END
