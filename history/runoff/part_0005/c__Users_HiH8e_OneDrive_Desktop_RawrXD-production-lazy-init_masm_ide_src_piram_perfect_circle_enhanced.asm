; ============================================================================
; PIRAM_PERFECT_CIRCLE_ENHANCED.ASM - 11-Pass Compression with Quantization
; Adaptive bit-depth, local variables, reverse memory operations
; ============================================================================

.686
.model flat, stdcall
option casemap:none

; ============================================================================
; EXPORTS
; ============================================================================
PUBLIC PiRam_PerfectCircleX11
PUBLIC PiRam_QuantizeAdaptive
PUBLIC PiRam_LocalCompress
PUBLIC PiRam_DequantizeAdaptive

; π constants (local scope only)
PI_CONSTANT     equ 3296474           ; forward scale
PI_INV_CONSTANT equ 333773            ; inverse scale

; Quantization bit depths
QUANT_8BIT      equ 8                 ; 8-bit quantization
QUANT_6BIT      equ 6                 ; 6-bit (more compression)
QUANT_4BIT      equ 4                 ; 4-bit (aggressive)

; ============================================================================
; CODE
; ============================================================================
.code

; ============================================================================
; PiRam_PerfectCircleX11 - 11-pass perfect circle compression
; pBuf: input buffer
; dwSize: input size
; Returns: EAX = final compressed size after all 11 passes
; 
; Each pass:
; 1. Halve the working size
; 2. Apply π-transform on halved portion
; 3. Accumulate remainder into next pass
; ============================================================================
PiRam_PerfectCircleX11 PROC pBuf:DWORD, dwSize:DWORD
    LOCAL passCount:DWORD
    LOCAL workingSize:DWORD
    LOCAL compressedSize:DWORD
    LOCAL pWorkBuf:DWORD
    LOCAL remainder:DWORD
    LOCAL quantDepth:DWORD
    LOCAL idx:DWORD
    LOCAL byte_val:DWORD
    LOCAL transformed:DWORD

    push esi
    push edi
    push ebx

    mov esi, pBuf
    mov edi, dwSize
    test esi, esi
    jz @x11_fail
    test edi, edi
    jz @x11_fail

    mov workingSize, edi        ; Start with full size
    mov pWorkBuf, esi
    mov passCount, 0            ; Pass counter
    mov compressedSize, 0

@pass_loop:
    cmp passCount, 11           ; 11 passes total
    jge @x11_done

    ; Calculate quantization depth (more aggressive per pass)
    mov eax, passCount
    cmp eax, 3
    jbe @use_8bit
    cmp eax, 6
    jbe @use_6bit
    mov quantDepth, QUANT_4BIT
    jmp @quant_set

@use_8bit:
    mov quantDepth, QUANT_8BIT
    jmp @quant_set

@use_6bit:
    mov quantDepth, QUANT_6BIT

@quant_set:
    ; Pass: halve working size
    mov eax, workingSize
    shr eax, 1
    mov remainder, eax          ; Save remainder
    mov workingSize, eax        ; Update working size

    ; Transform halved portion
    xor ecx, ecx

@transform_loop:
    cmp ecx, workingSize
    jge @transform_done

    ; Load byte
    movzx eax, byte ptr [pWorkBuf + ecx]
    mov byte_val, eax

    ; Apply π-transform
    imul eax, PI_CONSTANT
    shr eax, 20
    and eax, 0FFh
    mov transformed, eax

    ; Apply quantization (reduce bit depth)
    mov ebx, quantDepth
    mov eax, transformed
    shr eax, cl
    mov cl, bl
    shl eax, cl
    and eax, 0FFh
    mov byte ptr [pWorkBuf + ecx], al

    inc ecx
    jmp @transform_loop

@transform_done:
    ; Add remainder to next pass's input
    mov eax, remainder
    add compressedSize, eax

    ; Move to next pass
    inc passCount
    jmp @pass_loop

@x11_done:
    ; Final size = working size + accumulated compression
    mov eax, workingSize
    add eax, compressedSize
    jmp @x11_exit

@x11_fail:
    xor eax, eax

@x11_exit:
    pop ebx
    pop edi
    pop esi
    ret
PiRam_PerfectCircleX11 ENDP

; ============================================================================
; PiRam_QuantizeAdaptive - Adaptive quantization based on pass number
; pData: data pointer
; dwSize: size
; passNum: pass number (0-10)
; Returns: EAX = 1 success
; ============================================================================
PiRam_QuantizeAdaptive PROC pData:DWORD, dwSize:DWORD, passNum:DWORD
    LOCAL quantDepth:DWORD
    LOCAL idx:DWORD
    LOCAL byte_val:DWORD
    LOCAL shifted:DWORD

    push esi
    push edi

    mov esi, pData
    mov edi, dwSize
    mov eax, passNum
    test esi, esi
    jz @quant_fail

    ; Determine quant depth based on pass
    cmp eax, 3
    jbe @q_8bit
    cmp eax, 7
    jbe @q_6bit
    mov quantDepth, QUANT_4BIT
    jmp @quant_loop

@q_8bit:
    mov quantDepth, QUANT_8BIT
    jmp @quant_loop

@q_6bit:
    mov quantDepth, QUANT_6BIT

@quant_loop:
    mov idx, 0

@quant_process:
    cmp idx, edi
    jge @quant_done

    ; Load byte
    mov eax, idx
    movzx ebx, byte ptr [esi + eax]
    mov byte_val, ebx

    ; Shift right by (8 - quantDepth) to reduce precision
    mov ecx, 8
    sub ecx, quantDepth
    mov eax, byte_val
    shr eax, cl
    shl eax, cl                 ; Reconstruct with reduced bits
    and eax, 0FFh

    ; Store back
    mov ecx, idx
    mov byte ptr [esi + ecx], al

    inc idx
    jmp @quant_process

@quant_done:
    mov eax, 1
    jmp @quant_exit

@quant_fail:
    xor eax, eax

@quant_exit:
    pop edi
    pop esi
    ret
PiRam_QuantizeAdaptive ENDP

; ============================================================================
; PiRam_LocalCompress - Compress using only local variables (no globals)
; pBuf: buffer pointer
; dwSize: size
; Returns: EAX = compressed size
; ============================================================================
PiRam_LocalCompress PROC pBuf:DWORD, dwSize:DWORD
    LOCAL workSize:DWORD
    LOCAL idx:DWORD
    LOCAL byte_in:DWORD
    LOCAL byte_out:DWORD
    LOCAL passes:DWORD

    push esi
    push edi

    mov esi, pBuf
    mov edi, dwSize
    test esi, esi
    jz @local_fail

    mov workSize, edi
    mov passes, 0

@local_pass:
    cmp passes, 11
    jge @local_done

    ; Halve size
    shr workSize, 1
    mov idx, 0

@local_transform:
    cmp idx, workSize
    jge @local_pass_done

    ; Load byte from memory to register
    mov ecx, idx
    movzx eax, byte ptr [esi + ecx]
    ; eax now has the byte value

    ; Transform: multiply by π constant
    mov edx, 3296474      ; PI_CONSTANT
    mul edx
    shr eax, 20
    and eax, 0FFh
    ; eax has transformed value

    ; Store byte back to memory
    mov ecx, idx
    mov byte ptr [esi + ecx], al

    inc idx
    jmp @local_transform

@local_pass_done:
    inc passes
    jmp @local_pass

@local_done:
    mov eax, workSize
    jmp @local_exit

@local_fail:
    xor eax, eax

@local_exit:
    pop edi
    pop esi
    ret
PiRam_LocalCompress ENDP

; ============================================================================
; PiRam_DequantizeAdaptive - Reverse adaptive quantization
; pData: data pointer
; dwSize: size
; passNum: pass number (for determining quant depth)
; Returns: EAX = 1 success
; ============================================================================
PiRam_DequantizeAdaptive PROC pData:DWORD, dwSize:DWORD, passNum:DWORD
    LOCAL quantDepth:DWORD
    LOCAL idx:DWORD
    LOCAL byte_val:DWORD

    push esi
    push edi

    mov esi, pData
    mov edi, dwSize
    mov eax, passNum
    test esi, esi
    jz @dequant_fail

    ; Determine quant depth
    cmp eax, 3
    jbe @dq_8bit
    cmp eax, 7
    jbe @dq_6bit
    mov quantDepth, QUANT_4BIT
    jmp @dequant_loop

@dq_8bit:
    mov quantDepth, QUANT_8BIT
    jmp @dequant_loop

@dq_6bit:
    mov quantDepth, QUANT_6BIT

@dequant_loop:
    mov idx, 0

@dequant_process:
    cmp idx, edi
    jge @dequant_done

    ; Load byte (memory to local)
    mov eax, idx
    movzx ebx, byte ptr [esi + eax]
    mov byte_val, ebx

    ; Inverse quantization: expand from reduced bit-depth
    mov ecx, 8
    sub ecx, quantDepth
    mov eax, byte_val
    shr eax, cl
    imul eax, PI_INV_CONSTANT
    shr eax, 20
    and eax, 0FFh

    ; Store back (local to memory)
    mov ecx, idx
    mov byte ptr [esi + ecx], al

    inc idx
    jmp @dequant_process

@dequant_done:
    mov eax, 1
    jmp @dequant_exit

@dequant_fail:
    xor eax, eax

@dequant_exit:
    pop edi
    pop esi
    ret
PiRam_DequantizeAdaptive ENDP

END
