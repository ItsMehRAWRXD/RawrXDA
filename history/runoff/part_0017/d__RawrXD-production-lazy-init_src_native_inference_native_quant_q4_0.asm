; native_quant_q4_0.asm - Native Q4_0 quantization kernel for x64
; Implements GGML Q4_0 quantization: 4-bit quantization with per-block scaling

.code

; Quantize F32 array to Q4_0 format
; rcx = input float array
; rdx = output quantized array
; r8 = number of elements
; r9 = scales array (one per 32 elements)
NativeQuantizeQ4_0 PROC
    ; Save registers
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15

    ; Initialize
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
    cmovg rbx, r14      ; min(32, remaining)
    add r14, r10

    ; Find min/max for this block
    movss xmm0, dword ptr [r12]    ; load first element
    movss xmm1, xmm0                ; min = first
    movss xmm2, xmm0                ; max = first

    mov rsi, 1
find_minmax_loop:
    cmp rsi, rbx
    jge find_minmax_done

    movss xmm3, dword ptr [r12 + rsi*4]
    vminss xmm1, xmm1, xmm3
    vmaxss xmm2, xmm2, xmm3

    inc rsi
    jmp find_minmax_loop

find_minmax_done:
    ; Calculate scale: (max - min) / 15
    subss xmm2, xmm1          ; max - min
    movss xmm3, xmm2
    divss xmm3, dword ptr [fifteen]  ; / 15
    movss dword ptr [r15 + r11*4], xmm3      ; store scale

    ; Quantize elements in this block
    mov rsi, 0
    mov rdi, 0                      ; output byte counter

quantize_block_loop:
    cmp rsi, rbx
    jge quantize_block_done

    ; Load 8 floats at once if possible
    cmp rsi, rbx
    jge quantize_single

    mov rax, rbx
    sub rax, rsi
    cmp rax, 8
    jl quantize_single

    ; Process 8 elements with AVX
    movups xmm0, xmmword ptr [r12 + rsi*4]    ; load 8 floats (use xmm for 4 floats)
    subps xmm0, xmm1                    ; subtract min
    divps xmm0, xmm3                    ; divide by scale

    ; Convert to int and clamp to 0-15
    cvtps2dq xmm0, xmm0
    pminsd xmm0, xmmword ptr [fifteen_vec]
    pmaxsd xmm0, xmmword ptr [zero_vec]

    ; Pack 8 values into 4 bytes (2 values per byte)
    vextracti128 xmm1, ymm0, 0
    vextracti128 xmm2, ymm0, 1

    ; Pack pairs: [v0,v1,v2,v3] -> [v0|v1, v2|v3]
    vpslld xmm1, xmm1, 4              ; shift left 4 bits
    vpand xmm2, xmm2, xmmword ptr [low_nibble]  ; mask low 4 bits
    vpor xmm1, xmm1, xmm2             ; combine

    ; Store 4 bytes
    vmovd dword ptr [r13 + rdi], xmm1
    add rdi, 4
    add rsi, 8
    jmp quantize_block_loop

quantize_single:
    ; Process remaining elements one by one
    movss xmm0, dword ptr [r12 + rsi*4]
    vsubss xmm0, xmm0, xmm1           ; subtract min
    vdivss xmm0, xmm0, xmm3           ; divide by scale

    vcvtss2si eax, xmm0
    and eax, 0Fh                     ; clamp to 0-15

    ; Pack into bytes (2 values per byte)
    mov rdx, rsi
    and rdx, 1                       ; even/odd index
    jz pack_high

    ; Odd index: pack as low nibble
    mov cl, al
    mov byte ptr [r13 + rdi - 1], cl
    jmp pack_done

pack_high:
    ; Even index: pack as high nibble
    mov cl, al
    shl cl, 4
    mov byte ptr [r13 + rdi], cl
    inc rdi

pack_done:
    inc rsi
    jmp quantize_block_loop

quantize_block_done:
    ; Update pointers and counters
    lea r12, [r12 + rbx*4]           ; advance input
    add r13, rdi                     ; advance output
    add r10, rbx                     ; advance element counter
    inc r11                          ; advance block counter

    jmp quantize_loop

quantize_done:
    ; Restore registers
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret

fifteen real4 15.0
fifteen_vec dd 15, 15, 15, 15, 15, 15, 15, 15
zero_vec dd 0, 0, 0, 0, 0, 0, 0, 0
low_nibble db 0Fh, 0Fh, 0Fh, 0Fh, 0Fh, 0Fh, 0Fh, 0Fh, 0Fh, 0Fh, 0Fh, 0Fh, 0Fh, 0Fh, 0Fh, 0Fh

NativeQuantizeQ4_0 ENDP

; Dequantize Q4_0 array to F32
; rcx = input quantized array
; rdx = output float array
; r8 = number of elements
; r9 = scales array
NativeDequantizeQ4_0 PROC
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
    movss xmm3, dword ptr [r15 + r11*4]    ; scale

    ; Dequantize elements
    mov rsi, 0
    mov rdi, 0

dequantize_block_loop:
    cmp rsi, rbx
    jge dequantize_block_done

    ; Load byte and unpack two values
    movzx rax, byte ptr [r12 + rdi]
    mov rdx, rax
    shr rdx, 4          ; high nibble
    and rax, 0Fh        ; low nibble

    ; Convert to float and scale
    vcvtsi2ss xmm0, xmm0, eax
    vcvtsi2ss xmm1, xmm1, edx
    vmulss xmm0, xmm0, xmm3
    vmulss xmm1, xmm1, xmm3

    ; Store results
    movss dword ptr [r13 + rsi*4], xmm0
    cmp rsi, rbx
    jge skip_second
    movss dword ptr [r13 + rsi*4 + 4], xmm1
    inc rsi

skip_second:
    inc rsi
    inc rdi
    jmp dequantize_block_loop

dequantize_block_done:
    ; Update pointers and counters
    add r12, rdi
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

NativeDequantizeQ4_0 ENDP

END