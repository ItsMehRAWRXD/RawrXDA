; =============================================================================
; avx512_matmul.asm — Real AVX-512 FP32 Matrix Multiplication Kernel
; C = A * B, where A is MxK, B is KxN, C is MxN
; =============================================================================
; Export:
;   void avx512_matmul_f32(const float* A, const float* B, float* C,
;                          int64_t M, int64_t N, int64_t K);
; =============================================================================

include rawrxd_win64.inc

; ---------------------------------------------------------------------------
; Tile sizes (AVX-512: 16 floats per ZMM register)
; ---------------------------------------------------------------------------
TILE_M                  EQU 8
TILE_N                  EQU 16
TILE_K                  EQU 4

; ---------------------------------------------------------------------------
; Data
; ---------------------------------------------------------------------------
.data
align 8
sz_matmul_start         db "[AVX512] matmul start", 0

; ---------------------------------------------------------------------------
; Code
; ---------------------------------------------------------------------------
.code

; =============================================================================
; avx512_matmul_f32
;   RCX = const float* A
;   RDX = const float* B
;   R8  = float* C
;   R9  = int64_t M
;   [RSP+40] = int64_t N
;   [RSP+48] = int64_t K
; =============================================================================
align 16
PUBLIC avx512_matmul_f32
avx512_matmul_f32 PROC FRAME
    push rbx
    .pushreg rbx
    push rsi
    .pushreg rsi
    push rdi
    .pushreg rdi
    push r12
    .pushreg r12
    push r13
    .pushreg r13
    push r14
    .pushreg r14
    push r15
    .pushreg r15
    sub rsp, 72
    .allocstack 72
    .endprolog

    mov rbx, rcx                        ; RBX = A
    mov rsi, rdx                        ; RSI = B
    mov rdi, r8                         ; RDI = C
    mov r12, r9                         ; R12 = M
    mov r13, QWORD PTR [rsp+72+40]      ; R13 = N
    mov r14, QWORD PTR [rsp+72+48]      ; R14 = K

    ; Log dimensions
    lea rcx, sz_matmul_start
    call OutputDebugStringA

    ; Outer loop over M (rows of A / C)
    xor r15, r15                        ; R15 = m

_loop_m:
    cmp r15, r12
    jge _done_m

    ; Inner loop over N (columns of B / C)
    xor rax, rax                        ; RAX = n

_loop_n:
    cmp rax, r13
    jge _done_n

    ; Compute C[m,n] = dot(A[m,:], B[:,n])
    ; Initialize accumulator zmm0 = 0
    vxorps zmm0, zmm0, zmm0

    ; Inner loop over K
    xor rcx, rcx                        ; RCX = k

_loop_k:
    cmp rcx, r14
    jge _done_k

    ; Load A[m, k] — broadcast to all 16 lanes
    mov rdx, r15
    imul rdx, r14
    add rdx, rcx
    vbroadcastss zmm1, REAL4 PTR [rbx + rdx*4]

    ; Load B[k, n..n+15] — 16 floats
    mov rdx, rcx
    imul rdx, r13
    add rdx, rax
    vmovups zmm2, ZMMWORD PTR [rsi + rdx*4]

    ; FMA: zmm0 += zmm1 * zmm2
    vfmadd231ps zmm0, zmm1, zmm2

    inc rcx
    jmp _loop_k

_done_k:
    ; Store C[m, n..n+15]
    mov rdx, r15
    imul rdx, r13
    add rdx, rax
    vmovups ZMMWORD PTR [rdi + rdx*4], zmm0

    add rax, 16
    jmp _loop_n

_done_n:
    inc r15
    jmp _loop_m

_done_m:
    vzeroupper

    add rsp, 72
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
avx512_matmul_f32 ENDP

END
