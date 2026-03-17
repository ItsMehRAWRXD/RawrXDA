; ================================================================
; hierarchical_quant.asm — Adaptive Hierarchical Quantization Kernel
; Q8_0 / Q4_K / Q2_K per-layer adaptive quantization
; Assemble: ml64 /c hierarchical_quant.asm
; ================================================================

option casemap:none

; ─── Cross-module symbol resolution ───
INCLUDE rawrxd_master.inc


; ================================================================
; Quantization format constants
; ================================================================
Q8_0_BLOCK_SIZE equ 32     ; 32 floats per Q8_0 block
Q4_K_BLOCK_SIZE equ 256    ; 256 floats per Q4_K super-block
Q2_K_BLOCK_SIZE equ 256    ; 256 floats per Q2_K super-block

; Layer classification thresholds
LAYER_CRITICAL  equ 0      ; Embeddings, output head -> Q8_0
LAYER_IMPORTANT equ 1      ; Attention layers -> Q4_K
LAYER_BULK      equ 2      ; MLP middle layers -> Q2_K

.data
ALIGN 16

; Quantization scale factors per format
q8_scale_factor   dq 127.0
q4k_scale_factor  dq 7.0
q2k_scale_factor  dq 1.5

; Statistics accumulators
quant_stats_total_blocks   dq 0
quant_stats_q8_blocks      dq 0
quant_stats_q4k_blocks     dq 0
quant_stats_q2k_blocks     dq 0
quant_stats_total_bytes_in dq 0
quant_stats_total_bytes_out dq 0

.code
ALIGN 16

; ================================================================
; RawrXD_HierarchicalQuant
; ================================================================
; RCX = source float buffer pointer
; RDX = number of floats
; R8  = destination quantized buffer pointer
; R9  = layer_depth (0 = embedding/output, 1 = attention, 2+ = MLP bulk)
;
; Returns: RAX = number of bytes written to destination
; ================================================================
RawrXD_HierarchicalQuant PROC FRAME
    push rbp
    .pushreg rbp
    mov rbp, rsp
    push rbx
    .pushreg rbx
    push r12
    .pushreg r12
    push r13
    .pushreg r13
    push r14
    .pushreg r14
    push r15
    .pushreg r15
    push rsi
    .pushreg rsi
    push rdi
    .pushreg rdi
    sub rsp, 96
    .allocstack 96
    .endprolog

    mov r12, rcx        ; src pointer
    mov r13, rdx        ; float count
    mov r14, r8         ; dst pointer
    mov r15, r9         ; layer depth

    ; Save original dst for byte count calculation
    mov qword ptr [rbp-8], r14

    ; Classify layer
    cmp r15, 0
    je quant_q8_0
    cmp r15, 1
    je quant_q4_k
    jmp quant_q2_k

; ================================================================
; Q8_0 Quantization
; ================================================================
quant_q8_0:
    xor ebx, ebx

q8_block_loop:
    mov rax, r13
    sub rax, rbx
    cmp rax, Q8_0_BLOCK_SIZE
    jl quant_done

    ; rsi = src + block_offset * 4
    mov rax, rbx
    shl rax, 2
    lea rsi, [r12 + rax]
    xor ecx, ecx
    xorps xmm0, xmm0

q8_find_max:
    cmp ecx, Q8_0_BLOCK_SIZE
    jge q8_have_max
    movsxd rax, ecx
    movss xmm1, dword ptr [rsi + rax*4]
    ; Approximate abs: clear sign bit
    movd eax, xmm1
    and eax, 7FFFFFFFh
    movd xmm1, eax
    maxss xmm0, xmm1
    inc ecx
    jmp q8_find_max

q8_have_max:
    ; scale = max_abs / 127.0
    cvtss2sd xmm4, xmm0
    movsd xmm3, qword ptr [q8_scale_factor]
    divsd xmm4, xmm3
    cvtsd2ss xmm5, xmm4

    ; Store scale as fp32
    movss dword ptr [r14], xmm5
    add r14, 4

    ; rcp_scale = 1.0 / scale
    movss xmm6, xmm5
    rcpss xmm6, xmm6

    xor ecx, ecx
q8_quant_loop:
    cmp ecx, Q8_0_BLOCK_SIZE
    jge q8_block_done
    movsxd rax, ecx
    movss xmm1, dword ptr [rsi + rax*4]
    mulss xmm1, xmm6
    cvtss2si eax, xmm1

    ; Clamp to [-127, 127]
    cmp eax, 127
    jle q8_clamp_lo
    mov eax, 127
q8_clamp_lo:
    cmp eax, -127
    jge q8_store
    mov eax, -127
q8_store:
    mov byte ptr [r14], al
    inc r14
    inc ecx
    jmp q8_quant_loop

q8_block_done:
    add ebx, Q8_0_BLOCK_SIZE
    lock inc qword ptr [quant_stats_q8_blocks]
    jmp q8_block_loop

; ================================================================
; Q4_K Quantization
; ================================================================
quant_q4_k:
    xor ebx, ebx

q4k_block_loop:
    mov rax, r13
    sub rax, rbx
    cmp rax, Q4_K_BLOCK_SIZE
    jl quant_done

    ; rsi = src + block_offset * 4
    mov rax, rbx
    shl rax, 2
    lea rsi, [r12 + rax]

    ; Process 256 floats in 8 groups of 32
    xor edi, edi

q4k_group_loop:
    cmp edi, 8
    jge q4k_block_done

    ; group start = rsi + edi * 128 (32 floats * 4 bytes)
    ; Can't use lea with scale 128, compute manually
    movsxd rax, edi
    shl rax, 7           ; rax = edi * 128
    lea rcx, [rsi + rax]
    ; Save group base on stack
    mov qword ptr [rbp-16], rcx

    xor edx, edx
    xorps xmm0, xmm0

q4k_find_max:
    cmp edx, 32
    jge q4k_have_max
    mov rcx, qword ptr [rbp-16]
    movsxd rax, edx
    movss xmm1, dword ptr [rcx + rax*4]
    ; abs
    movd eax, xmm1
    and eax, 7FFFFFFFh
    movd xmm1, eax
    maxss xmm0, xmm1
    inc edx
    jmp q4k_find_max

q4k_have_max:
    ; scale = max_abs / 7.0
    cvtss2sd xmm4, xmm0
    movsd xmm3, qword ptr [q4k_scale_factor]
    divsd xmm4, xmm3
    cvtsd2ss xmm5, xmm4

    ; Store group scale
    movss dword ptr [r14], xmm5
    add r14, 4

    ; rcp_scale
    rcpss xmm6, xmm5
    xor edx, edx

q4k_quant_loop:
    cmp edx, 32
    jge q4k_group_done

    ; Low nibble
    mov rcx, qword ptr [rbp-16]
    movsxd rax, edx
    movss xmm1, dword ptr [rcx + rax*4]
    mulss xmm1, xmm6
    cvtss2si eax, xmm1
    add eax, 8
    cmp eax, 15
    jle q4k_lo_ok
    mov eax, 15
q4k_lo_ok:
    cmp eax, 0
    jge q4k_lo_ok2
    xor eax, eax
q4k_lo_ok2:
    mov r8d, eax
    inc edx

    ; High nibble
    cmp edx, 32
    jge q4k_store_last
    mov rcx, qword ptr [rbp-16]
    movsxd rax, edx
    movss xmm1, dword ptr [rcx + rax*4]
    mulss xmm1, xmm6
    cvtss2si eax, xmm1
    add eax, 8
    cmp eax, 15
    jle q4k_hi_ok
    mov eax, 15
q4k_hi_ok:
    cmp eax, 0
    jge q4k_hi_ok2
    xor eax, eax
q4k_hi_ok2:
    shl eax, 4
    or eax, r8d
    mov byte ptr [r14], al
    inc r14
    inc edx
    jmp q4k_quant_loop

q4k_store_last:
    mov byte ptr [r14], r8b
    inc r14

q4k_group_done:
    inc edi
    jmp q4k_group_loop

q4k_block_done:
    add ebx, Q4_K_BLOCK_SIZE
    lock inc qword ptr [quant_stats_q4k_blocks]
    jmp q4k_block_loop

; ================================================================
; Q2_K Quantization
; ================================================================
quant_q2_k:
    xor ebx, ebx

q2k_block_loop:
    mov rax, r13
    sub rax, rbx
    cmp rax, Q2_K_BLOCK_SIZE
    jl quant_done

    ; rsi = src + block_offset * 4
    mov rax, rbx
    shl rax, 2
    lea rsi, [r12 + rax]

    ; Process 256 floats in 16 groups of 16
    xor edi, edi

q2k_group_loop:
    cmp edi, 16
    jge q2k_block_done

    ; group start = rsi + edi * 64 (16 floats * 4 bytes)
    ; Can't use lea with scale 64, compute manually
    movsxd rax, edi
    shl rax, 6           ; rax = edi * 64
    lea rcx, [rsi + rax]
    ; Save group base on stack
    mov qword ptr [rbp-16], rcx

    xor edx, edx
    xorps xmm0, xmm0

q2k_find_max:
    cmp edx, 16
    jge q2k_have_max
    mov rcx, qword ptr [rbp-16]
    movsxd rax, edx
    movss xmm1, dword ptr [rcx + rax*4]
    ; abs
    movd eax, xmm1
    and eax, 7FFFFFFFh
    movd xmm1, eax
    maxss xmm0, xmm1
    inc edx
    jmp q2k_find_max

q2k_have_max:
    cvtss2sd xmm4, xmm0
    movsd xmm3, qword ptr [q2k_scale_factor]
    divsd xmm4, xmm3
    cvtsd2ss xmm5, xmm4

    ; Store group scale
    movss dword ptr [r14], xmm5
    add r14, 4

    ; rcp_scale
    rcpss xmm6, xmm5
    xor edx, edx

q2k_quant_loop:
    cmp edx, 16
    jge q2k_group_done

    ; Pack 4 x 2-bit values into one byte
    xor r8d, r8d         ; accumulator byte
    xor r9d, r9d         ; bit position

q2k_pack4:
    cmp r9d, 4
    jge q2k_store_byte
    cmp edx, 16
    jge q2k_store_byte

    mov rcx, qword ptr [rbp-16]
    movsxd rax, edx
    movss xmm1, dword ptr [rcx + rax*4]
    mulss xmm1, xmm6
    cvtss2si eax, xmm1
    ; Clamp to [-1, 1] -> [0, 3] with bias 1
    add eax, 1
    cmp eax, 3
    jle q2k_val_ok
    mov eax, 3
q2k_val_ok:
    cmp eax, 0
    jge q2k_val_ok2
    xor eax, eax
q2k_val_ok2:
    ; Shift into position: bit_offset = r9d * 2
    mov ecx, r9d
    shl ecx, 1
    shl eax, cl
    or r8d, eax

    inc r9d
    inc edx
    jmp q2k_pack4

q2k_store_byte:
    mov byte ptr [r14], r8b
    inc r14
    jmp q2k_quant_loop

q2k_group_done:
    inc edi
    jmp q2k_group_loop

q2k_block_done:
    add ebx, Q2_K_BLOCK_SIZE
    lock inc qword ptr [quant_stats_q2k_blocks]
    jmp q2k_block_loop

; ================================================================
; Done
; ================================================================
quant_done:
    mov rax, r14
    sub rax, qword ptr [rbp-8]

    lock inc qword ptr [quant_stats_total_blocks]

    add rsp, 96
    pop rdi
    pop rsi
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret

RawrXD_HierarchicalQuant ENDP

; ================================================================
; RawrXD_GetQuantStats
; ================================================================
; RCX = pointer to 6-element qword array
; ================================================================
RawrXD_GetQuantStats PROC
    mov rax, qword ptr [quant_stats_total_blocks]
    mov qword ptr [rcx], rax
    mov rax, qword ptr [quant_stats_q8_blocks]
    mov qword ptr [rcx+8], rax
    mov rax, qword ptr [quant_stats_q4k_blocks]
    mov qword ptr [rcx+16], rax
    mov rax, qword ptr [quant_stats_q2k_blocks]
    mov qword ptr [rcx+24], rax
    mov rax, qword ptr [quant_stats_total_bytes_in]
    mov qword ptr [rcx+32], rax
    mov rax, qword ptr [quant_stats_total_bytes_out]
    mov qword ptr [rcx+40], rax
    ret
RawrXD_GetQuantStats ENDP

; ================================================================
PUBLIC RawrXD_HierarchicalQuant
PUBLIC RawrXD_GetQuantStats

END
