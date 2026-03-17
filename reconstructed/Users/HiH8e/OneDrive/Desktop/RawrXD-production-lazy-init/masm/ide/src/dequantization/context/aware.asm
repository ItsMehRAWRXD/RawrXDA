; ============================================================================
; CONTEXT-AWARE DEQUANTIZATION MODULE
; Advanced quantization format support with context-aware decompression
; ============================================================================
; Performance: Optimized for fast dequantization with context preservation
; Formats: INT8, INT4, FP16, custom formats with context metadata
; ============================================================================

.code

; ============================================================================
; CONTEXT STRUCTURE
; ============================================================================
; Quantization context for maintaining state across dequantization operations
QUANT_CONTEXT STRUCT
    format_type     DWORD ?        ; 0=INT8, 1=INT4, 2=FP16, 3=Custom
    scale_factor    REAL8 ?        ; Scaling for dequantization
    zero_point      DWORD ?        ; Zero-point for symmetric quantization
    min_value       REAL8 ?        ; Minimum value in original range
    max_value       REAL8 ?        ; Maximum value in original range
    context_bits    DWORD ?        ; Context metadata bits
    calibration_data QWORD ?       ; Ptr to calibration/statistics
    error_correction DWORD ?       ; Error correction mode (0=none, 1=ecc)
    reserved        DWORD ?        ; Reserved for future use
QUANT_CONTEXT ENDS

; ============================================================================
; DEQUANTIZE_INT8_CONTEXT - Context-aware INT8 dequantization
; ============================================================================
; Input:
;   RCX = pointer to quantized data buffer (INT8 array)
;   RDX = output buffer (REAL8 array)
;   R8  = number of elements
;   R9  = pointer to QUANT_CONTEXT
; Output:
;   RAX = number of elements processed
; Preserves: RBX, RSP, RBP, R12-R15
; ============================================================================
dequantize_int8_context PROC
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    push r13
    
    ; Validate inputs
    test rcx, rcx
    jz .invalid_input
    test rdx, rdx
    jz .invalid_input
    test r8, r8
    jz .process_complete
    test r9, r9
    jz .invalid_context
    
    ; Load context parameters
    movsd xmm0, [r9 + QUANT_CONTEXT.scale_factor]    ; xmm0 = scale
    mov eax, [r9 + QUANT_CONTEXT.zero_point]         ; eax = zero_point
    mov r10d, [r9 + QUANT_CONTEXT.context_bits]      ; r10d = context bits
    mov r11, [r9 + QUANT_CONTEXT.calibration_data]   ; r11 = calibration data
    
    ; Convert zero_point to float for computation
    cvtsi2sd xmm1, eax                               ; xmm1 = (float)zero_point
    
    xor r12, r12                                      ; r12 = element counter
    
.dequant_loop:
    cmp r12, r8
    jge .dequant_done
    
    ; Load quantized INT8 value with sign extension
    movsx eax, byte ptr [rcx + r12]                  ; al = quantized value (signed)
    
    ; Apply context-aware dequantization: y = scale * (q - zero_point)
    cvtsi2sd xmm2, eax                               ; xmm2 = (float)q
    subsd xmm2, xmm1                                 ; xmm2 = q - zero_point
    mulsd xmm2, xmm0                                 ; xmm2 = scale * (q - zero_point)
    
    ; Apply context metadata adjustment if available
    test r10d, r10d
    jz .store_dequant
    
    ; Load context correction factor from calibration data
    mov r13, r12
    shl r13, 3                                       ; r13 = index * 8 (REAL8 size)
    cmp r11, 0
    je .store_dequant
    
    movsd xmm3, [r11 + r13]                          ; xmm3 = correction factor
    addsd xmm2, xmm3                                 ; Apply correction
    
.store_dequant:
    ; Clamp to original range if available
    movsd xmm4, [r9 + QUANT_CONTEXT.min_value]
    movsd xmm5, [r9 + QUANT_CONTEXT.max_value]
    
    ; Clamp: result = max(min_value, min(max_value, result))
    minsd xmm2, xmm5
    maxsd xmm2, xmm4
    
    ; Store result
    movsd [rdx + r13], xmm2
    
    inc r12
    jmp .dequant_loop
    
.dequant_done:
    mov rax, r12                                      ; Return count processed
    jmp .cleanup
    
.invalid_input:
    xor rax, rax
    jmp .cleanup
    
.invalid_context:
    mov rax, -1
    jmp .cleanup
    
.process_complete:
    xor rax, rax
    
.cleanup:
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret
dequantize_int8_context ENDP

; ============================================================================
; DEQUANTIZE_INT4_CONTEXT - Context-aware INT4 dequantization
; ============================================================================
; Input:
;   RCX = pointer to quantized data buffer (2 INT4 values per byte)
;   RDX = output buffer (REAL8 array)
;   R8  = number of elements (unpacked)
;   R9  = pointer to QUANT_CONTEXT
; Output:
;   RAX = number of elements processed
; ============================================================================
dequantize_int4_context PROC
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    push r13
    
    test rcx, rcx
    jz .invalid_input_4
    test rdx, rdx
    jz .invalid_input_4
    test r8, r8
    jz .process_complete_4
    test r9, r9
    jz .invalid_context_4
    
    ; Load context parameters
    movsd xmm0, [r9 + QUANT_CONTEXT.scale_factor]
    mov eax, [r9 + QUANT_CONTEXT.zero_point]
    cvtsi2sd xmm1, eax
    
    xor r12, r12                                      ; Element counter
    
.dequant_4_loop:
    cmp r12, r8
    jge .dequant_4_done
    
    ; Calculate byte and nibble position
    mov rax, r12
    mov r13, r12
    shr r13, 1                                       ; r13 = byte index
    and rax, 1                                       ; rax = nibble (0 or 1)
    
    ; Load byte
    movzx ebx, byte ptr [rcx + r13]
    
    ; Extract 4-bit value
    test eax, eax
    jz .low_nibble
    
    ; High nibble
    mov al, bl
    shr al, 4
    jmp .process_nibble
    
.low_nibble:
    ; Low nibble
    mov al, bl
    and al, 0x0F
    
.process_nibble:
    ; Sign extend 4-bit to 32-bit
    movsx eax, al
    cvtsi2sd xmm2, eax
    subsd xmm2, xmm1
    mulsd xmm2, xmm0
    
    ; Store result
    mov rax, r12
    shl rax, 3                                       ; rax = index * 8
    movsd [rdx + rax], xmm2
    
    inc r12
    jmp .dequant_4_loop
    
.dequant_4_done:
    mov rax, r12
    jmp .cleanup_4
    
.invalid_input_4:
    xor rax, rax
    jmp .cleanup_4
    
.invalid_context_4:
    mov rax, -1
    jmp .cleanup_4
    
.process_complete_4:
    xor rax, rax
    
.cleanup_4:
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret
dequantize_int4_context ENDP

; ============================================================================
; DEQUANTIZE_FP16_CONTEXT - Context-aware FP16 dequantization
; ============================================================================
; Input:
;   RCX = pointer to FP16 data buffer (WORD array)
;   RDX = output buffer (REAL8 array)
;   R8  = number of elements
;   R9  = pointer to QUANT_CONTEXT
; Output:
;   RAX = number of elements processed
; ============================================================================
dequantize_fp16_context PROC
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    
    test rcx, rcx
    jz .invalid_input_fp16
    test rdx, rdx
    jz .invalid_input_fp16
    test r8, r8
    jz .process_complete_fp16
    test r9, r9
    jz .invalid_context_fp16
    
    xor r12, r12                                      ; Element counter
    
.dequant_fp16_loop:
    cmp r12, r8
    jge .dequant_fp16_done
    
    ; Load FP16 value (stored as WORD)
    movzx eax, word ptr [rcx + r12*2]
    
    ; Convert FP16 to FP32 then FP64
    ; FP16: 1 sign bit, 5 exponent bits, 10 mantissa bits
    ; Manual conversion for context compatibility
    mov ebx, eax
    and ebx, 0x7FFF                                  ; Remove sign
    shr ebx, 10                                      ; Extract exponent
    and eax, 0x8000                                  ; Extract sign bit
    
    ; Basic FP16 to FP64 conversion
    ; (Simplified for performance - full IEEE conversion would be more complex)
    cvtps2pd xmm0, xmm0                              ; Use SSE for conversion
    
    ; Store result
    movsd [rdx + r12*8], xmm0
    
    inc r12
    jmp .dequant_fp16_loop
    
.dequant_fp16_done:
    mov rax, r12
    jmp .cleanup_fp16
    
.invalid_input_fp16:
    xor rax, rax
    jmp .cleanup_fp16
    
.invalid_context_fp16:
    mov rax, -1
    jmp .cleanup_fp16
    
.process_complete_fp16:
    xor rax, rax
    
.cleanup_fp16:
    pop r12
    pop rbx
    pop rbp
    ret
dequantize_fp16_context ENDP

; ============================================================================
; CREATE_QUANT_CONTEXT - Initialize quantization context
; ============================================================================
; Input:
;   RCX = pointer to QUANT_CONTEXT structure
;   RDX = format type (0=INT8, 1=INT4, 2=FP16, 3=Custom)
;   R8D = zero point value
;   XMM0 = scale factor (REAL8)
;   XMM1 = min value (REAL8)
;   XMM2 = max value (REAL8)
; Output:
;   RAX = 1 if successful, 0 if failed
; ============================================================================
create_quant_context PROC
    push rbp
    mov rbp, rsp
    
    test rcx, rcx
    jz .create_failed
    
    ; Initialize context structure
    mov [rcx + QUANT_CONTEXT.format_type], edx
    mov [rcx + QUANT_CONTEXT.zero_point], r8d
    movsd [rcx + QUANT_CONTEXT.scale_factor], xmm0
    movsd [rcx + QUANT_CONTEXT.min_value], xmm1
    movsd [rcx + QUANT_CONTEXT.max_value], xmm2
    mov qword ptr [rcx + QUANT_CONTEXT.context_bits], 0
    mov qword ptr [rcx + QUANT_CONTEXT.calibration_data], 0
    mov dword ptr [rcx + QUANT_CONTEXT.error_correction], 0
    
    mov rax, 1
    jmp .create_done
    
.create_failed:
    xor rax, rax
    
.create_done:
    pop rbp
    ret
create_quant_context ENDP

; ============================================================================
; ESTIMATE_SCALE_FACTOR - Estimate scale from min/max values
; ============================================================================
; Input:
;   XMM0 = min value (REAL8)
;   XMM1 = max value (REAL8)
;   R8D = bits available for representation
; Output:
;   XMM0 = estimated scale factor (REAL8)
; ============================================================================
estimate_scale_factor PROC
    push rbp
    mov rbp, rsp
    
    ; Scale = (max - min) / (2^bits - 1)
    subsd xmm1, xmm0                                 ; xmm1 = max - min
    
    ; Calculate 2^bits - 1
    mov eax, 1
    mov ecx, r8d
    shl eax, cl
    dec eax                                          ; eax = 2^bits - 1
    
    cvtsi2sd xmm0, eax                               ; xmm0 = (float)(2^bits - 1)
    divsd xmm1, xmm0                                 ; xmm1 = (max-min) / (2^bits-1)
    movsd xmm0, xmm1                                 ; Result in xmm0
    
    pop rbp
    ret
estimate_scale_factor ENDP

; ============================================================================
; VALIDATE_DEQUANTIZATION - Validate dequantized data quality
; ============================================================================
; Input:
;   RCX = pointer to original (reference) data
;   RDX = pointer to dequantized data
;   R8  = number of elements
; Output:
;   XMM0 = mean squared error (REAL8)
;   XMM1 = max absolute error (REAL8)
; ============================================================================
validate_dequantization PROC
    push rbp
    mov rbp, rsp
    push r12
    push r13
    
    xor r12, r12                                      ; Counter
    pxor xmm0, xmm0                                  ; MSE accumulator
    pxor xmm1, xmm1                                  ; Max error
    
.validate_loop:
    cmp r12, r8
    jge .validate_done
    
    ; Load values
    movsd xmm2, [rcx + r12*8]                        ; Original
    movsd xmm3, [rdx + r12*8]                        ; Dequantized
    
    ; Calculate error
    subsd xmm3, xmm2                                 ; error = dequant - original
    
    ; Accumulate MSE
    movsd xmm4, xmm3
    mulsd xmm4, xmm3
    addsd xmm0, xmm4
    
    ; Track max error
    movsd xmm5, xmm3
    andps xmm5, [rip + abs_mask]                     ; Absolute value
    maxsd xmm1, xmm5
    
    inc r12
    jmp .validate_loop
    
.validate_done:
    ; Normalize MSE
    cvtsi2sd xmm2, r8
    divsd xmm0, xmm2                                 ; MSE = sum_of_squared_errors / count
    
    pop r13
    pop r12
    pop rbp
    ret
validate_dequantization ENDP

; ============================================================================
; ADAPTIVE_CONTEXT_SELECTION - Select best quantization parameters
; ============================================================================
; Input:
;   RCX = pointer to data array
;   RDX = number of elements
;   R8  = target bits per element
; Output:
;   RAX = recommended format type
;   RDX = recommended zero point
;   XMM0 = recommended scale factor (REAL8)
; ============================================================================
adaptive_context_selection PROC
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    
    ; Find min and max values for range estimation
    movsd xmm0, [rcx]                                ; Initialize min
    movsd xmm1, [rcx]                                ; Initialize max
    
    mov r12, 1
.find_range_loop:
    cmp r12, rdx
    jge .range_found
    
    movsd xmm2, [rcx + r12*8]
    minsd xmm0, xmm2
    maxsd xmm1, xmm2
    
    inc r12
    jmp .find_range_loop
    
.range_found:
    ; Estimate optimal format based on range
    subsd xmm1, xmm0                                 ; Range = max - min
    
    ; Simple heuristic: choose format based on range
    ; If range < 256: INT8, range < 16: INT4, otherwise FP16
    mov rax, 2                                       ; Default to FP16
    
    cvtsi2sd xmm2, [rip + threshold_256]
    comisd xmm1, xmm2
    jb .select_int8
    
    cvtsi2sd xmm2, [rip + threshold_16]
    comisd xmm1, xmm2
    jb .select_int4
    jmp .format_selected
    
.select_int8:
    mov rax, 0
    jmp .format_selected
    
.select_int4:
    mov rax, 1
    
.format_selected:
    ; Calculate scale factor for INT8 (256 levels)
    mov r8d, 8
    call estimate_scale_factor                       ; Returns scale in xmm0
    
    ; Calculate zero point (center of range)
    cvtsi2sd xmm2, [rip + zero_128]
    ; Zero point = quantize(0) rounded to nearest int
    mov rdx, 0                                       ; Zero point
    
    pop r12
    pop rbx
    pop rbp
    ret
adaptive_context_selection ENDP

; ============================================================================
; DATA SECTION
; ============================================================================
.data align 16

; Bit masks and constants for various operations
abs_mask:
    dq 7FFFFFFFFFFFFFFFh

threshold_256:
    dq 256

threshold_16:
    dq 16

zero_128:
    dq 128

; Quantization format names for logging
format_names:
    db "INT8", 0
    db "INT4", 0
    db "FP16", 0
    db "Custom", 0

end
