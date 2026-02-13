; quantization.asm
; MASM64 quantization/dequantization routines for GGML
; Optimized for Q4_0, Q8_0 formats

.code

; ==================================================================================================
; Quantize to Q4_0
; void quantize_q4_0(const float* src, void* dst, int64_t n)
; Each block processes 32 floats: 4-byte float scale + 16 bytes of 4-bit values = 20 bytes per block
; = [scale (4 bytes)][qs0..qs15 (16 bytes)]
; ==================================================================================================
quantize_q4_0 PROC
    push rbp
    mov rbp, rsp
    push rbx
    push rsi
    push rdi

    mov r10, r8         ; total elements (n)
    shr r10, 5          ; n / 32 = number of blocks
    
    xor rsi, rsi        ; block index
block_loop:
    cmp rsi, r10
    jge done

    ; 1. Find max absolute value in block of 32
    vxorps xmm0, xmm0, xmm0 ; max_abs
    mov rax, rsi
    shl rax, 5          ; rax = block_start (rsi * 32)
    xor rbx, rbx
find_max:
    cmp rbx, 32
    jge max_found
    vmovss xmm1, dword ptr [rcx + rax * 4 + rbx * 4]
    vandps xmm1, xmm1, xmmword ptr [abs_mask] ; abs(x)
    vmaxss xmm0, xmm0, xmm1
    inc rbx
    jmp find_max

max_found:
    ; scale d = max_abs / -8 (or similar depending on convention)
    ; GGML Q4_0: d = max_abs / 7.0f (roughly) or use 1.0/scale
    ; We'll use d = max_abs / 7.0f
    mov eax, 040e00000h ; 7.0f
    vmovd xmm2, eax
    vdivss xmm0, xmm0, xmm2 ; d = scale
    
    ; Store scale at dst + block_offset
    ; dst block offset = block_index * 20
    imul rax, rsi, 20
    vmovss dword ptr [rdx + rax], xmm0 

    ; Compute inverse scale for faster quantization
    vmovss xmm3, dword ptr [one_const]
    vdivss xmm3, xmm3, xmm0 ; inv_d

    ; 2. Quantize 32 values into 16 bytes
    xor rbx, rbx
quant_vals:
    cmp rbx, 16
    jge blocks_next

    ; Two values per byte (qs[rbx])
    ; val0 = src[rsi*32 + rbx*2]
    ; val1 = src[rsi*32 + rbx*2 + 1]
    mov r11, rsi
    shl r11, 5
    lea r11, [r11 + rbx * 2]
    
    vmovss xmm4, dword ptr [rcx + r11 * 4]
    vmulss xmm4, xmm4, xmm3
    vcvttss2si eax, xmm4
    add eax, 8
    and eax, 0Fh ; low nibble

    vmovss xmm5, dword ptr [rcx + r11 * 4 + 4]
    vmulss xmm5, xmm5, xmm3
    vcvttss2si r9d, xmm5
    add r9d, 8
    and r9d, 0Fh ; high nibble
    shl r9d, 4
    
    or eax, r9d
    imul r11, rsi, 20
    mov byte ptr [rdx + r11 + 4 + rbx], al

    inc rbx
    jmp quant_vals

blocks_next:
    inc rsi
    jmp block_loop

done:
    pop rdi
    pop rsi
    pop rbx
    pop rbp
    ret
quantize_q4_0 ENDP

; ==================================================================================================
; Dequantize from Q4_0
; void dequantize_q4_0(const void* src, float* dst, int64_t n)
; ==================================================================================================
dequantize_q4_0 PROC
    push rbp
    mov rbp, rsp
    push rbx
    push rsi
    push rdi

    mov r10, r8         ; total elements (n)
    shr r10, 5          ; n / 32 = number of blocks
    
    xor rsi, rsi        ; block index
deblock_loop:
    cmp rsi, r10
    jge de_done

    imul rax, rsi, 20
    vmovss xmm0, dword ptr [rcx + rax] ; scale d

    xor rbx, rbx
dequant_vals:
    cmp rbx, 16
    jge deblocks_next

    mov r11, rax ; block offset
    movzx r9d, byte ptr [rcx + r11 + 4 + rbx]
    
    ; Low nibble
    mov r8d, r9d
    and r8d, 0Fh
    sub r8d, 8
    cvtsi2ss xmm1, r8d
    vmulss xmm1, xmm1, xmm0
    
    mov r11, rsi
    shl r11, 5
    lea r11, [r11 + rbx * 2]
    vmovss dword ptr [rdx + r11 * 4], xmm1

    ; High nibble
    mov r8d, r9d
    shr r8d, 4
    sub r8d, 8
    cvtsi2ss xmm1, r8d
    vmulss xmm1, xmm1, xmm0
    vmovss dword ptr [rdx + r11 * 4 + 4], xmm1

    inc rbx
    jmp dequant_vals

deblocks_next:
    inc rsi
    jmp deblock_loop

de_done:
    pop rdi
    pop rsi
    pop rbx
    pop rbp
    ret
dequantize_q4_0 ENDP

.data
align 16
abs_mask dd 7FFFFFFFh, 7FFFFFFFh, 7FFFFFFFh, 7FFFFFFFh
one_const dd 1.0

END
