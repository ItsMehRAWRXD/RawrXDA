; Filename: gemm_kernel.asm
; Architecture: x86-64 (Windows x64 calling convention)
; Assembler: NASM
;
; Function: gemm_4x4
; Signature (C++ equivalent): void gemm_4x4(const int* A, const int* B, int* C);
;
; Windows x64 calling convention:
;   RCX: Address of matrix A (4x4, row-major)
;   RDX: Address of matrix B (4x4, row-major)
;   R8:  Address of matrix C (4x4, result)
;
; This uses simple integer arithmetic and NO vectorization (SSE/AVX) for simplicity.
; Every instruction is visible, no compiler magic, no SIMD, no placeholder.

section .text
global gemm_4x4

gemm_4x4:
    push    rbp
    mov     rbp, rsp
    push    rbx
    push    r12
    push    r13
    push    r14
    push    r15

    ; Save input pointers (Windows x64 calling convention)
    mov     r12, rcx    ; r12 = A
    mov     r13, rdx    ; r13 = B
    mov     r14, r8     ; r14 = C

    xor     r15, r15    ; i = 0 (Row index for A and C)

.loop_i:
    cmp     r15, 4
    jge     .end_i

    xor     rbx, rbx    ; j = 0 (Column index for B and C)

.loop_j:
    cmp     rbx, 4
    jge     .end_j

    xor     r10, r10    ; sum = 0 (accumulator for C[i][j])
    xor     r11, r11    ; k = 0 (inner index)

.loop_k:
    cmp     r11, 4
    jge     .end_k

    ; Calculate A[i][k] address: r12 + (i * 4 + k) * 4 bytes
    mov     rax, r15
    shl     rax, 4      ; rax = i * 16 (4 ints * 4 bytes)
    mov     rcx, r11
    shl     rcx, 2      ; rcx = k * 4
    add     rax, rcx    ; rax = i * 16 + k * 4
    mov     edi, [r12 + rax]  ; Load A[i][k] into EDI

    ; Calculate B[k][j] address: r13 + (k * 4 + j) * 4 bytes
    mov     rax, r11
    shl     rax, 4      ; rax = k * 16
    mov     rcx, rbx
    shl     rcx, 2      ; rcx = j * 4
    add     rax, rcx    ; rax = k * 16 + j * 4
    mov     esi, [r13 + rax]  ; Load B[k][j] into ESI

    ; Multiply A[i][k] * B[k][j]
    imul    edi, esi    ; EDI = A[i][k] * B[k][j]

    ; Accumulate: sum += A[i][k] * B[k][j]
    add     r10d, edi   ; Explicit scalar add

    inc     r11         ; k++
    jmp     .loop_k

.end_k:
    ; Store sum into C[i][j]
    mov     rax, r15
    shl     rax, 4      ; rax = i * 16
    mov     rcx, rbx
    shl     rcx, 2      ; rcx = j * 4
    add     rax, rcx    ; rax = i * 16 + j * 4
    mov     [r14 + rax], r10d  ; C[i][j] = sum

    inc     rbx         ; j++
    jmp     .loop_j

.end_j:
    inc     r15         ; i++
    jmp     .loop_i

.end_i:
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rbx
    mov     rsp, rbp
    pop     rbp
    ret
