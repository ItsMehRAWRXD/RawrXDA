; KV Cache Management for Transformer Inference
; MASM x64 assembly code
; Implements rolling buffer logic for efficient memory usage
; Handles key and value caches for multi-head attention
; Uses AVX-512 for copying/moving data

; Assumptions:
; - Float32 data
; - d_k and d_v are multiples of 16 (for AVX-512 efficiency, 512bit = 16 floats)
; - Memory for caches is pre-allocated by caller
; - Roll by half max_seq_len when full

KV_Cache STRUCT
    num_heads   DWORD ?
    d_k         DWORD ?
    d_v         DWORD ?
    max_seq_len DWORD ?
    current_len DWORD ?
    k_cache     QWORD ?  ; pointer to array[num_heads] of QWORD pointers to K data
    v_cache     QWORD ?  ; pointer to array[num_heads] of QWORD pointers to V data
KV_Cache ENDS

.code

; Initialize KV Cache
; Parameters:
;   rcx: pointer to KV_Cache struct
;   rdx: num_heads
;   r8: d_k
;   r9: d_v
;   [rsp+40]: max_seq_len
KV_Cache_Init PROC
    mov [rcx + KV_Cache.num_heads], edx
    mov [rcx + KV_Cache.d_k], r8d
    mov [rcx + KV_Cache.d_v], r9d
    mov rax, [rsp + 40]
    mov [rcx + KV_Cache.max_seq_len], eax
    mov [rcx + KV_Cache.current_len], 0
    ; k_cache and v_cache should be set by caller
    ret
KV_Cache_Init ENDP

; Append new KV pairs to cache
; Parameters:
;   rcx: pointer to KV_Cache struct
;   rdx: pointer to array[num_heads] of QWORD pointers to new K vectors
;   r8: pointer to array[num_heads] of QWORD pointers to new V vectors
KV_Cache_Append PROC
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15

    mov rbx, rcx  ; cache
    mov rsi, rdx  ; k_data
    mov rdi, r8   ; v_data

    mov eax, [rbx + KV_Cache.current_len]
    cmp eax, [rbx + KV_Cache.max_seq_len]
    jne no_roll
    mov rcx, rbx
    call KV_Cache_Roll
    mov eax, [rbx + KV_Cache.current_len]

no_roll:
    mov r12d, [rbx + KV_Cache.num_heads]
    mov r13, [rbx + KV_Cache.k_cache]
    mov r14, [rbx + KV_Cache.v_cache]
    xor r15, r15  ; head index

append_loop:
    ; Append K
    mov rcx, [r13 + r15*8]  ; K matrix for head
    mov rdx, [rsi + r15*8]  ; new K vector
    mov r8d, [rbx + KV_Cache.d_k]
    imul r8, 4
    mov r9d, eax  ; current_len
    imul r9, r8   ; offset in bytes
    add rcx, r9
    mov r10, r8   ; bytes per vector
    shr r10, 6    ; number of 64-byte blocks
    test r10, r10
    jz append_k_done
append_k_copy:
    vmovdqu64 zmm0, [rdx]
    vmovdqu64 [rcx], zmm0
    add rdx, 64
    add rcx, 64
    dec r10
    jnz append_k_copy
append_k_done:

    ; Append V
    mov rcx, [r14 + r15*8]  ; V matrix for head
    mov rdx, [rdi + r15*8]  ; new V vector
    mov r8d, [rbx + KV_Cache.d_v]
    imul r8, 4
    mov r9d, eax
    imul r9, r8
    add rcx, r9
    mov r10, r8
    shr r10, 6
    test r10, r10
    jz append_v_done
append_v_copy:
    vmovdqu64 zmm0, [rdx]
    vmovdqu64 [rcx], zmm0
    add rdx, 64
    add rcx, 64
    dec r10
    jnz append_v_copy
append_v_done:

    inc r15
    cmp r15, r12
    jl append_loop

    inc [rbx + KV_Cache.current_len]

    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
KV_Cache_Append ENDP

; Get cached KV for attention computation
; Copies KV data from start position for len entries
; Parameters:
;   rcx: pointer to KV_Cache struct
;   rdx: start position
;   r8: length
;   r9: pointer to array[num_heads] of QWORD pointers to output K buffers
;   [rsp+40]: pointer to array[num_heads] of QWORD pointers to output V buffers
KV_Cache_Get PROC
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15

    mov rbx, rcx  ; cache
    mov esi, edx  ; start
    mov edi, r8d  ; len
    mov r12, r9   ; k_out
    mov r13, [rsp + 72]  ; v_out (after pushed regs)

    mov r14d, [rbx + KV_Cache.num_heads]
    mov r15, [rbx + KV_Cache.k_cache]
    mov rcx, [rbx + KV_Cache.v_cache]  ; use rcx for v_cache
    xor rdx, rdx  ; head index

get_loop:
    ; Get K
    mov rax, [r15 + rdx*8]  ; K matrix
    mov r8, [r12 + rdx*8]   ; output buffer
    mov r9d, [rbx + KV_Cache.d_k]
    imul r9, 4
    mov r10d, esi
    imul r10, r9  ; start offset
    add rax, r10
    mov r11d, edi
    imul r11, r9  ; total bytes
    mov r10, r11
    shr r10, 6    ; blocks
    test r10, r10
    jz get_k_done
get_k_copy:
    vmovdqu64 zmm0, [rax]
    vmovdqu64 [r8], zmm0
    add rax, 64
    add r8, 64
    dec r10
    jnz get_k_copy
get_k_done:

    ; Get V
    mov rax, [rcx + rdx*8]  ; V matrix
    mov r8, [r13 + rdx*8]   ; output buffer
    mov r9d, [rbx + KV_Cache.d_v]
    imul r9, 4
    mov r10d, esi
    imul r10, r9
    add rax, r10
    mov r11d, edi
    imul r11, r9
    mov r10, r11
    shr r10, 6
    test r10, r10
    jz get_v_done
get_v_copy:
    vmovdqu64 zmm0, [rax]
    vmovdqu64 [r8], zmm0
    add rax, 64
    add r8, 64
    dec r10
    jnz get_v_copy
get_v_done:

    inc rdx
    cmp rdx, r14
    jl get_loop

    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
KV_Cache_Get ENDP

; Roll the buffer when it reaches capacity
; Rolls by copying the second half to the first half
; Parameters:
;   rcx: pointer to KV_Cache struct
KV_Cache_Roll PROC
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15

    mov rbx, rcx  ; cache
    mov eax, [rbx + KV_Cache.max_seq_len]
    shr eax, 1    ; roll_amount = max_seq_len / 2
    mov [rbx + KV_Cache.current_len], eax

    mov r12d, [rbx + KV_Cache.num_heads]
    mov r13, [rbx + KV_Cache.k_cache]
    mov r14, [rbx + KV_Cache.v_cache]
    xor r15, r15  ; head index

roll_loop:
    ; Roll K
    mov rsi, [r13 + r15*8]  ; K matrix
    mov rdi, rsi            ; dst = src
    mov r8d, [rbx + KV_Cache.d_k]
    imul r8, 4
    mov r9d, eax            ; roll_amount
    imul r9, r8             ; src offset
    add rsi, r9
    mov r10d, [rbx + KV_Cache.max_seq_len]
    sub r10d, eax           ; num to copy
    imul r10, r8            ; bytes
    mov r11, r10
    shr r11, 6              ; blocks
    test r11, r11
    jz roll_k_done
roll_k_copy:
    vmovdqu64 zmm0, [rsi]
    vmovdqu64 [rdi], zmm0
    add rsi, 64
    add rdi, 64
    dec r11
    jnz roll_k_copy
roll_k_done:

    ; Roll V
    mov rsi, [r14 + r15*8]  ; V matrix
    mov rdi, rsi
    mov r8d, [rbx + KV_Cache.d_v]
    imul r8, 4
    mov r9d, eax
    imul r9, r8
    add rsi, r9
    mov r10d, [rbx + KV_Cache.max_seq_len]
    sub r10d, eax
    imul r10, r8
    mov r11, r10
    shr r11, 6
    test r11, r11
    jz roll_v_done
roll_v_copy:
    vmovdqu64 zmm0, [rsi]
    vmovdqu64 [rdi], zmm0
    add rsi, 64
    add rdi, 64
    dec r11
    jnz roll_v_copy
roll_v_done:

    inc r15
    cmp r15, r12
    jl roll_loop

    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
KV_Cache_Roll ENDP

end