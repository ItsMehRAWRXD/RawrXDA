; ggml_backend.asm
; MASM64 dispatcher for GGML tensor operations
; Calls SIMD-optimized routines in tensor_ops.asm

section .text

; GGML_BackendDispatch
; Inputs: operation code, pointers to tensors, sizes
; Output: calls appropriate tensor_ops routine
GGML_BackendDispatch PROC
    ; RCX: op, RDX: A, R8: B, R9: C, [rsp+40]: sizeA, [rsp+48]: sizeB, [rsp+56]: sizeC
    push rbp
    mov rbp, rsp
    push rbx
    push rsi
    push rdi

    mov eax, ecx ; op code
    cmp eax, 0 ; GGML_OP_MATMUL
    je do_matmul
    cmp eax, 1 ; GGML_OP_ADD
    je do_add
    cmp eax, 2 ; GGML_OP_MUL
    je do_mul
    cmp eax, 3 ; GGML_OP_QUANTIZE_Q8_0
    je do_quant8
    cmp eax, 4 ; GGML_OP_QUANTIZE_Q2_K
    je do_quant2k
    cmp eax, 5 ; GGML_OP_DEQUANTIZE_Q8_0
    je do_dequant8
    cmp eax, 6 ; GGML_OP_DEQUANTIZE_Q2_K
    je do_dequant2k
    jmp unknown_op

do_matmul:
    mov rcx, rdx ; A
    mov rdx, r8  ; B
    mov r8, r9   ; C
    mov r9, [rbp+40] ; M
    mov r10, [rbp+48] ; N
    mov r11, [rbp+56] ; K
    sub rsp, 32 ; shadow space
    call ggml_masm_mul_mat
    add rsp, 32
    xor eax, eax
    jmp done

do_add:
    mov rcx, rdx ; A
    mov rdx, r8  ; B
    mov r8, r9   ; C
    mov r9, [rbp+40] ; n
    sub rsp, 32
    call ggml_masm_add
    add rsp, 32
    xor eax, eax
    jmp done

do_mul:
    mov rcx, rdx ; A
    mov rdx, r8  ; B
    mov r8, r9   ; C
    mov r9, [rbp+40] ; n
    sub rsp, 32
    call ggml_masm_mul
    add rsp, 32
    xor eax, eax
    jmp done

do_quant8:
    mov rcx, rdx ; src
    mov rdx, r9  ; dst
    mov r8, [rbp+40] ; n
    sub rsp, 32
    call quantize_q8_0
    add rsp, 32
    xor eax, eax
    jmp done

do_quant2k:
    mov rcx, rdx ; src
    mov rdx, r9  ; dst
    mov r8, [rbp+40] ; n
    sub rsp, 32
    call quantize_q2_k
    add rsp, 32
    xor eax, eax
    jmp done

do_dequant8:
    mov rcx, rdx ; src
    mov rdx, r9  ; dst
    mov r8, [rbp+40] ; n
    sub rsp, 32
    call dequantize_q8_0
    add rsp, 32
    xor eax, eax
    jmp done

do_dequant2k:
    mov rcx, rdx ; src
    mov rdx, r9  ; dst
    mov r8, [rbp+40] ; n
    sub rsp, 32
    call dequantize_q2_k
    add rsp, 32
    xor eax, eax
    jmp done

unknown_op:
    mov eax, -1
    jmp done

done:
    pop rdi
    pop rsi
    pop rbx
    pop rbp
    ret
GGML_BackendDispatch ENDP
