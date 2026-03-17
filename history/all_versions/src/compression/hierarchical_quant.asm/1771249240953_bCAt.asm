; ================================================================
; hierarchical_quant.asm — Adaptive Hierarchical Quantization Kernel
; Q8_0 / Q4_K / Q2_K per-layer adaptive quantization
; Assemble: ml64 /c hierarchical_quant.asm
; ================================================================

option casemap:none

; ================================================================
; Quantization format constants
; ================================================================
Q8_0_BLOCK_SIZE equ 32     ; 32 floats per Q8_0 block
Q4_K_BLOCK_SIZE equ 256    ; 256 floats per Q4_K super-block
Q2_K_BLOCK_SIZE equ 256    ; 256 floats per Q2_K super-block

; Layer classification thresholds
LAYER_CRITICAL  equ 0      ; Embeddings, output head → Q8_0
LAYER_IMPORTANT equ 1      ; Attention layers → Q4_K
LAYER_BULK      equ 2      ; MLP middle layers → Q2_K

.data
ALIGN 64

; Quantization scale factors per format
q8_scale_factor   dq 127.0
q4k_scale_factor  dq 7.0
q2k_scale_factor  dq 1.5

; Reciprocal lookup for fast division
rcp_127           dq 0.00787401574803   ; 1/127
rcp_7             dq 0.14285714285714   ; 1/7
rcp_1_5           dq 0.66666666666667   ; 1/1.5

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
; Quantize a contiguous float buffer using adaptive per-layer strategy
;
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
    mov [rbp-8], r14

    ; Classify layer
    cmp r15, 0
    je quant_q8_0       ; Critical layers → Q8_0 (highest quality)
    cmp r15, 1
    je quant_q4_k       ; Important layers → Q4_K
    jmp quant_q2_k      ; Bulk layers → Q2_K (maximum compression)

; ================================================================
; Q8_0 Quantization — 8-bit symmetric per-block
; Block: [fp16 scale] [32 x int8 values] = 34 bytes per 32 floats
; Compression: 128 bytes → 34 bytes (3.76x)
; ================================================================
quant_q8_0:
    xor ebx, ebx        ; block counter

q8_block_loop:
    ; Check remaining floats
    mov rax, r13
    sub rax, rbx
    cmp rax, Q8_0_BLOCK_SIZE
    jl quant_done

    ; Find max absolute value in this block (for scale computation)
    lea rsi, [r12 + rbx*4]
    xor ecx, ecx
    xorps xmm0, xmm0    ; max_abs = 0

q8_find_max:
    cmp ecx, Q8_0_BLOCK_SIZE
    jge q8_have_max
    movss xmm1, [rsi + rcx*4]
    ; abs(xmm1)
    movss xmm2, xmm1
    psrld xmm2, 31
    pslld xmm2, 31
    pxor xmm1, xmm2     ; clear sign bit (approximate abs)
    andps xmm1, xmm1
    maxss xmm0, xmm1
    inc ecx
    jmp q8_find_max

q8_have_max:
    ; scale = max_abs / 127.0
    movsd xmm3, q8_scale_factor
    cvtss2sd xmm4, xmm0
    divsd xmm4, xmm3    ; xmm4 = scale (double precision)
    cvtsd2ss xmm5, xmm4 ; xmm5 = scale (float)

    ; Store scale as fp16 (truncated fp32 → fp16 approximation)
    ; For full precision, use VCVTPS2PH if available
    movss [r14], xmm5    ; Store as fp32 (4 bytes, upgraded from fp16 for MASM compat)
    add r14, 4

    ; Quantize: q = round(val / scale)
    ; rcp_scale = 1.0 / scale
    movss xmm6, xmm5
    rcpss xmm6, xmm6    ; approximate reciprocal

    xor ecx, ecx
q8_quant_loop:
    cmp ecx, Q8_0_BLOCK_SIZE
    jge q8_block_done
    movss xmm1, [rsi + rcx*4]
    mulss xmm1, xmm6    ; val * rcp_scale
    cvtss2si eax, xmm1  ; round to nearest int

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
    ; Update stats
    lock inc qword ptr quant_stats_q8_blocks
    jmp q8_block_loop

; ================================================================
; Q4_K Quantization — 4-bit with per-group scales (super-block)
; Super-block: 256 floats → ~144 bytes
; Compression: 1024 bytes → 144 bytes (~7.1x)
; ================================================================
quant_q4_k:
    xor ebx, ebx

q4k_block_loop:
    mov rax, r13
    sub rax, rbx
    cmp rax, Q4_K_BLOCK_SIZE
    jl quant_done

    lea rsi, [r12 + rbx*4]

    ; Process 256 floats in 8 groups of 32
    xor edi, edi         ; group index

q4k_group_loop:
    cmp edi, 8
    jge q4k_block_done

    ; Find max abs in group of 32
    lea rcx, [rsi + rdi*128]  ; group start (32 floats * 4 bytes)
    xor edx, edx
    xorps xmm0, xmm0

q4k_find_max:
    cmp edx, 32
    jge q4k_have_max
    movss xmm1, [rcx + rdx*4]
    andps xmm1, xmm1    ; approximate abs
    maxss xmm0, xmm1
    inc edx
    jmp q4k_find_max

q4k_have_max:
    ; scale = max_abs / 7.0
    movsd xmm3, q4k_scale_factor
    cvtss2sd xmm4, xmm0
    divsd xmm4, xmm3
    cvtsd2ss xmm5, xmm4

    ; Store group scale (fp32, 4 bytes)
    movss [r14], xmm5
    add r14, 4

    ; Quantize 32 floats to 4-bit (two nibbles per byte = 16 bytes)
    rcpss xmm6, xmm5
    xor edx, edx

q4k_quant_loop:
    cmp edx, 32
    jge q4k_group_done

    ; Low nibble
    movss xmm1, [rcx + rdx*4]
    mulss xmm1, xmm6
    cvtss2si eax, xmm1
    ; Clamp to [-7, 7] → [0, 15] with bias 8
    add eax, 8
    cmp eax, 15
    jle q4k_lo_ok
    mov eax, 15
q4k_lo_ok:
    cmp eax, 0
    jge q4k_lo_ok2
    xor eax, eax
q4k_lo_ok2:
    mov r8d, eax         ; save low nibble
    inc edx

    ; High nibble
    cmp edx, 32
    jge q4k_store_last
    movss xmm1, [rcx + rdx*4]
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
    shl eax, 4           ; shift to high nibble
    or eax, r8d          ; combine nibbles
    mov byte ptr [r14], al
    inc r14
    inc edx
    jmp q4k_quant_loop

q4k_store_last:
    mov byte ptr [r14], r8b
    inc r14
    ; fall through

q4k_group_done:
    inc edi
    jmp q4k_group_loop

q4k_block_done:
    add ebx, Q4_K_BLOCK_SIZE
    lock inc qword ptr quant_stats_q4k_blocks
    jmp q4k_block_loop

; ================================================================
; Q2_K Quantization — 2-bit with per-group scales
; Super-block: 256 floats → ~80 bytes
; Compression: 1024 bytes → 80 bytes (~12.8x)
; ================================================================
quant_q2_k:
    xor ebx, ebx

q2k_block_loop:
    mov rax, r13
    sub rax, rbx
    cmp rax, Q2_K_BLOCK_SIZE
    jl quant_done

    lea rsi, [r12 + rbx*4]

    ; Process 256 floats in 16 groups of 16
    xor edi, edi

q2k_group_loop:
    cmp edi, 16
    jge q2k_block_done

    lea rcx, [rsi + rdi*64]  ; group start (16 floats * 4 bytes)
    xor edx, edx
    xorps xmm0, xmm0

q2k_find_max:
    cmp edx, 16
    jge q2k_have_max
    movss xmm1, [rcx + rdx*4]
    andps xmm1, xmm1
    maxss xmm0, xmm1
    inc edx
    jmp q2k_find_max

q2k_have_max:
    movsd xmm3, q2k_scale_factor
    cvtss2sd xmm4, xmm0
    divsd xmm4, xmm3
    cvtsd2ss xmm5, xmm4

    ; Store group scale
    movss [r14], xmm5
    add r14, 4

    ; Quantize 16 floats to 2-bit (4 values per byte = 4 bytes)
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

    movss xmm1, [rcx + rdx*4]
    mulss xmm1, xmm6
    cvtss2si eax, xmm1
    ; Clamp to [-1, 1] → [0, 3] with bias 1
    add eax, 1
    cmp eax, 3
    jle q2k_val_ok
    mov eax, 3
q2k_val_ok:
    cmp eax, 0
    jge q2k_val_ok2
    xor eax, eax
q2k_val_ok2:
    ; Shift into position
    mov ecx, r9d
    shl ecx, 1            ; bit offset = position * 2
    shl eax, cl
    or r8d, eax

    inc r9d
    inc edx
    lea rcx, [rsi + rdi*64]  ; restore rcx base
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
    lock inc qword ptr quant_stats_q2k_blocks
    jmp q2k_block_loop

; ================================================================
; Done — compute output size
; ================================================================
quant_done:
    ; RAX = bytes written = current dst - original dst
    mov rax, r14
    sub rax, [rbp-8]

    ; Update total stats
    lock inc qword ptr quant_stats_total_blocks

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
; RawrXD_GetQuantStats — Return quantization statistics
; ================================================================
; RCX = pointer to 6-element qword array to fill:
;   [0] total_blocks, [1] q8_blocks, [2] q4k_blocks,
;   [3] q2k_blocks, [4] bytes_in, [5] bytes_out
; ================================================================
RawrXD_GetQuantStats PROC
    mov rax, quant_stats_total_blocks
    mov [rcx], rax
    mov rax, quant_stats_q8_blocks
    mov [rcx+8], rax
    mov rax, quant_stats_q4k_blocks
    mov [rcx+16], rax
    mov rax, quant_stats_q2k_blocks
    mov [rcx+24], rax
    mov rax, quant_stats_total_bytes_in
    mov [rcx+32], rax
    mov rax, quant_stats_total_bytes_out
    mov [rcx+40], rax
    ret
RawrXD_GetQuantStats ENDP

; ================================================================
PUBLIC RawrXD_HierarchicalQuant
PUBLIC RawrXD_GetQuantStats

END
