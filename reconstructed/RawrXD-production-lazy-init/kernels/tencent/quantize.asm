; tencent_quantize.asm
; Tencent Q4_0 quantization kernel using AVX-512
; Quantizes 512 floats to Q4_0 (4-bit quantized format)

.code

; void tencent_quantize_avx512(
;     const float* input,      RCX
;     int8_t* output,          RDX
;     size_t count,            R8
;     float* scale,            R9
;     float* zero_point        [RSP+32]
; )

tencent_quantize_avx512 PROC FRAME
    push rbx
    push r12
    push r13
    .pushreg rbx
    .pushreg r12
    .pushreg r13
    .endprolog
    
    ; Find absolute maximum value for scaling
    xorps xmm0, xmm0            ; Initialize min/max to 0
    vmovaps zmm1, [rip + @f64_zero]  ; Zero vector
    vmovaps zmm2, [rip + @f64_zero]  ; Max accumulator
    
    mov rax, r8                 ; count
    shr rax, 4                  ; divide by 16 (zmm width)
    test rax, rax
    jz @find_max_scalar
    
    ; Process 16 floats at a time
@find_max_loop:
    vmovups zmm3, [rcx]         ; Load 16 floats
    vcmpps k1, zmm3, zmm1, 17   ; Compare with 0.0 (not ordered)
    vmovaps zmm4, [rip + @f64_abs_mask]
    vandps zmm3, zmm3, zmm4    ; Take absolute value
    vmaxps zmm2, zmm2, zmm3    ; Update max
    
    add rcx, 64                 ; Next 16 floats
    sub rax, 1
    jnz @find_max_loop
    
@find_max_scalar:
    ; Reduce zmm2 to scalar maximum
    vextractf32x8 ymm3, zmm2, 1
    vmaxps ymm2, ymm2, ymm3
    vextractf128 xmm3, ymm2, 1
    vmaxps xmm2, xmm2, xmm3
    vshufps xmm3, xmm2, xmm2, 0x55
    vmaxss xmm2, xmm2, xmm3
    vshufps xmm3, xmm2, xmm2, 0x11
    vmaxss xmm2, xmm2, xmm3
    
    ; Convert max to scale (max_val / 7.0 for Q4_0 range)
    vmovss xmm0, [rip + @f32_seven]
    vdivss xmm0, xmm2, xmm0
    vmovss [r9], xmm0           ; Store scale
    
    ; Quantize: output[i] = round(input[i] / scale)
    vbroadcastss zmm1, [r9]     ; Broadcast scale to all elements
    
    mov rax, r8                 ; count
    shr rax, 4                  ; divide by 16
    test rax, rax
    jz @done
    
    mov r10, rdx                ; output pointer
    mov rcx, [rsp + 40]         ; Restore input (if needed)
    
@quantize_loop:
    vmovups zmm0, [rcx]         ; Load 16 floats
    vdivps zmm0, zmm0, zmm1    ; Divide by scale
    vcvttps2dq zmm2, zmm0      ; Convert to int32
    
    ; Pack int32 to int8 (simplified - truncate to 8-bit)
    ; Real implementation would need nibble packing for Q4_0
    vpmovdb ymm2, zmm2         ; Convert int32 to int8 (lower 128 bits)
    vmovdqu [r10], xmm2        ; Store first 16 int8 values
    
    add rcx, 64
    add r10, 16
    sub rax, 1
    jnz @quantize_loop
    
@done:
    pop r13
    pop r12
    pop rbx
    ret

.data
@f64_zero:      DQ 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0
@f32_seven:     DD 7.0
@f64_abs_mask:  DQ 07FFFFFFFFFFFFFFFh, 07FFFFFFFFFFFFFFFh, 07FFFFFFFFFFFFFFFh, 07FFFFFFFFFFFFFFFh
                DQ 07FFFFFFFFFFFFFFFh, 07FFFFFFFFFFFFFFFh, 07FFFFFFFFFFFFFFFh, 07FFFFFFFFFFFFFFFh

tencent_quantize_avx512 ENDP

END
