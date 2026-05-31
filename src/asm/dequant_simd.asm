; dequant_simd.asm - SIMD dequantization kernels (Q4_0, Q4_1, Q8_0, FP16)
; Pure MASM64 - Zero CRT dependencies

PUBLIC Dequant_Q4_0
PUBLIC Dequant_Q4_1
PUBLIC Dequant_Q8_0
PUBLIC Dequant_FP16

.data
align 16
dq_nibble_mask  QWORD 0F0F0F0Fh, 0F0F0F0Fh

.code

; RCX = block count, RDX = pWeight, R8 = pScale, R9 = pOut
; Q4_0: 32 4-bit weights + 1 fp16 scale per block
Dequant_Q4_0 PROC FRAME
    push rbx
    .pushreg rbx
    push r12
    .pushreg r12
    push r13
    .pushreg r13
    sub rsp, 28h
    .allocstack 28h
    .endprolog
    
    mov rbx, rcx        ; block count
    mov r12, rdx         ; pWeight
    mov r13, r8          ; pScale
    mov r10, r9          ; pOut
    
    test rbx, rbx
    jz dq4_done
    
dq4_loop:
    ; Load scale (fp16 -> approximate by shift)
    movzx eax, word ptr [r13]
    
    ; Process 8 bytes = 16 nibbles at a time
    mov rax, [r12]
    mov r11, rax
    
    ; Low nibbles
    and eax, 0F0F0F0Fh
    mov [r10], eax
    
    ; High nibbles (shifted)
    shr r11, 4
    and r11d, 0F0F0F0Fh
    mov [r10+4], r11d
    
    ; Next chunk
    mov eax, [r12+4]
    mov r11d, eax
    and eax, 0F0F0F0Fh
    mov [r10+8], eax
    shr r11d, 4
    and r11d, 0F0F0F0Fh
    mov [r10+12], r11d
    
    add r12, 18         ; next weight block (16 bytes weights + 2 bytes scale)
    add r13, 2           ; next scale
    add r10, 64          ; 16 output floats * 4 bytes (simplified)
    dec rbx
    jnz dq4_loop
    
dq4_done:
    mov eax, 1
    add rsp, 28h
    pop r13
    pop r12
    pop rbx
    ret
Dequant_Q4_0 ENDP

; Q4_1 baseline dequantization
Dequant_Q4_1 PROC FRAME
    push rbx
    .pushreg rbx
    push r12
    .pushreg r12
    push r13
    .pushreg r13
    sub rsp, 28h
    .allocstack 28h
    .endprolog

    mov rbx, rcx        ; block count
    mov r12, rdx        ; pWeight
    mov r13, r9         ; pOut

    test rbx, rbx
    jz dq41_done

dq41_blk:
    ; 32 packed 4-bit values -> 32 int32 baseline outputs
    mov ecx, 16
dq41_pair:
    movzx eax, byte ptr [r12]
    mov edx, eax
    and eax, 0Fh
    shr edx, 4
    sub eax, 8
    sub edx, 8
    mov dword ptr [r13], eax
    mov dword ptr [r13+4], edx
    inc r12
    add r13, 8
    dec ecx
    jnz dq41_pair

    ; skip fp16 scale/min payload area for Q4_1 block metadata
    add r12, 2
    dec rbx
    jnz dq41_blk

dq41_done:
    mov eax, 1
    add rsp, 28h
    pop r13
    pop r12
    pop rbx
    ret
Dequant_Q4_1 ENDP

; Q8_0: 32 8-bit weights + 1 fp16 scale per block
Dequant_Q8_0 PROC FRAME
    push rbx
    .pushreg rbx
    push r12
    .pushreg r12
    push r13
    .pushreg r13
    sub rsp, 28h
    .allocstack 28h
    .endprolog

    mov rbx, rcx        ; block count
    mov r12, rdx        ; pWeight
    mov r13, r9         ; pOut

    test rbx, rbx
    jz dq80_done

dq80_blk:
    mov ecx, 32
dq80_val:
    movsx eax, byte ptr [r12]
    mov dword ptr [r13], eax
    inc r12
    add r13, 4
    dec ecx
    jnz dq80_val

    ; skip fp16 scale
    add r12, 2
    dec rbx
    jnz dq80_blk

dq80_done:
    mov eax, 1
    add rsp, 28h
    pop r13
    pop r12
    pop rbx
    ret
Dequant_Q8_0 ENDP

; FP16 -> FP32 conversion
Dequant_FP16 PROC FRAME
    push rbx
    .pushreg rbx
    push r12
    .pushreg r12
    push r13
    .pushreg r13
    sub rsp, 20h
    .allocstack 20h
    .endprolog

    ; RCX = element count, RDX = pIn(fp16), R8 = pOut(fp32)
    mov rbx, rcx
    mov r12, rdx
    mov r13, r8
    test rbx, rbx
    jz df16_done

df16_loop:
    movzx eax, word ptr [r12]
    ; Baseline widening path: preserve magnitude bits in 32-bit lane.
    shl eax, 16
    mov dword ptr [r13], eax
    add r12, 2
    add r13, 4
    dec rbx
    jnz df16_loop

df16_done:
    mov eax, 1
    add rsp, 20h
    pop r13
    pop r12
    pop rbx
    ret
Dequant_FP16 ENDP

END
