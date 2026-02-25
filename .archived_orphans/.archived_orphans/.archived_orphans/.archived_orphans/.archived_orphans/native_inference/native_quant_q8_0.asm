; native_quant_q8_0.asm - Native Q8_0 quantization kernel for x64
; Implements GGML Q8_0 quantization: 8-bit quantization with per-block scaling


; ─── Cross-module symbol resolution ───
INCLUDE rawrxd_master.inc

.code

; Quantize F32 array to Q8_0 format
; rcx = input float array
; rdx = output quantized array
; r8 = number of elements
; r9 = scales array (one per 32 elements)
NativeQuantizeQ8_0 PROC
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15

    xor r10, r10        ; element counter
    xor r11, r11        ; block counter
    mov r12, rcx        ; input ptr
    mov r13, rdx        ; output ptr
    mov r14, r8         ; total elements
    mov r15, r9         ; scales ptr

quantize_loop:
    cmp r10, r14
    jge quantize_done

    ; Process 32 elements per block
    mov rbx, 32
    cmp r14, r10
    sub r14, r10
    cmovg rbx, r14
    add r14, r10

    ; Calculate scale for this block
    ; For Q8_0, scale = max(abs(values)) / 127
    movss xmm0, dword ptr [r12]    ; load first element
    vandps xmm0, xmm0, xmmword ptr [abs_mask]  ; abs
    movss xmm1, xmm0                ; max_abs = first

    mov rsi, 1
find_maxabs_loop:
    cmp rsi, rbx
    jge find_maxabs_done

    movss xmm2, dword ptr [r12 + rsi*4]
    vandps xmm2, xmm2, xmmword ptr [abs_mask]  ; abs
    vmaxss xmm1, xmm1, xmm2

    inc rsi
    jmp find_maxabs_loop

find_maxabs_done:
    ; scale = max_abs / 127
    movss xmm2, xmm1
    divss xmm2, dword ptr [q8_scale]
    movss dword ptr [r15 + r11*4], xmm2

    ; Quantize elements
    mov rsi, 0
quantize_block_loop:
    cmp rsi, rbx
    jge quantize_block_done

    ; Load float and quantize
    movss xmm0, dword ptr [r12 + rsi*4]
    vdivss xmm0, xmm0, xmm2                    ; divide by scale
    vcvtss2si eax, xmm0                        ; convert to int
    movsx eax, al                              ; sign extend to 32-bit

    ; Clamp to -128..127
    cmp eax, -128
    jge clamp_min_ok
    mov eax, -128
clamp_min_ok:
    cmp eax, 127
    jle clamp_max_ok
    mov eax, 127
clamp_max_ok:

    ; Store as signed byte
    mov byte ptr [r13 + rsi], al

    inc rsi
    jmp quantize_block_loop

quantize_block_done:
    ; Update pointers and counters
    lea r12, [r12 + rbx*4]
    add r13, rbx
    add r10, rbx
    inc r11

    jmp quantize_loop

quantize_done:
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret

abs_mask dd 7FFFFFFFh, 7FFFFFFFh, 7FFFFFFFh, 7FFFFFFFh
q8_scale real4 127.0

NativeQuantizeQ8_0 ENDP

; Dequantize Q8_0 array to F32
; rcx = input quantized array
; rdx = output float array
; r8 = number of elements
; r9 = scales array
NativeDequantizeQ8_0 PROC
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15

    xor r10, r10        ; element counter
    xor r11, r11        ; block counter
    mov r12, rcx        ; input ptr
    mov r13, rdx        ; output ptr
    mov r14, r8         ; total elements
    mov r15, r9         ; scales ptr

dequantize_loop:
    cmp r10, r14
    jge dequantize_done

    ; Process 32 elements per block
    mov rbx, 32
    cmp r14, r10
    sub r14, r10
    cmovg rbx, r14
    add r14, r10

    ; Load scale for this block
    movss xmm2, dword ptr [r15 + r11*4]

    ; Dequantize elements
    mov rsi, 0
dequantize_block_loop:
    cmp rsi, rbx
    jge dequantize_block_done

    ; Load signed byte and convert to float
    movsx eax, byte ptr [r12 + rsi]
    vcvtsi2ss xmm0, xmm0, eax
    vmulss xmm0, xmm0, xmm2

    ; Store result
    movss dword ptr [r13 + rsi*4], xmm0

    inc rsi
    jmp dequantize_block_loop

dequantize_block_done:
    ; Update pointers and counters
    add r12, rbx
    lea r13, [r13 + rbx*4]
    add r10, rbx
    inc r11

    jmp dequantize_loop

dequantize_done:
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret

NativeDequantizeQ8_0 ENDP

END