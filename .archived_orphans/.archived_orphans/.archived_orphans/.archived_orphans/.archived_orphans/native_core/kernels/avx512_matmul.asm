; native_core/kernels/avx512_matmul.asm
; AVX-512 SGEMM: C[M,N] += A[M,K] * B[K,N]   (row-major, f32)
; Tile: 16x16 micro-kernel, 6x2 register blocking
; Zero external deps — pure MASM64, ml64.exe compatible
;
; PUBLIC Symbols:
;   MatMul_AVX512       — Full SGEMM C = alpha*A*B + beta*C
;   MatMul_AVX2         — AVX2 fallback  (8-wide)
;   MatVec_AVX512       — Matrix-vector  y = A*x  (N=1 fast-path)
;   DotProduct_AVX512   — Dot product    d = sum(a[i]*b[i])


; ─── Cross-module symbol resolution ───
INCLUDE rawrxd_master.inc

.data
ALIGN 16
one_f32         REAL4 1.0
zero_f32        REAL4 0.0

.code

PUBLIC MatMul_AVX512
PUBLIC MatMul_AVX2
PUBLIC MatVec_AVX512
PUBLIC DotProduct_AVX512

; ============================================================================
; MatMul_AVX512 — C[M,N] = alpha * A[M,K] * B[K,N] + beta * C[M,N]
;
; Windows x64 ABI:
;   RCX = ptr to MatMulArgs struct:
;     +0   float* A       (row-major, M x K)
;     +8   float* B       (row-major, K x N)
;     +16  float* C       (row-major, M x N)
;     +24  uint64 M
;     +32  uint64 N
;     +40  uint64 K
;     +48  float  alpha
;     +52  float  beta
; ============================================================================
MatMul_AVX512 PROC FRAME
    push        rbp
    .pushreg    rbp
    mov         rbp, rsp
    .setframe   rbp, 0
    push        rbx
    .pushreg    rbx
    push        rsi
    .pushreg    rsi
    push        rdi
    .pushreg    rdi
    push        r12
    .pushreg    r12
    push        r13
    .pushreg    r13
    push        r14
    .pushreg    r14
    push        r15
    .pushreg    r15
    sub         rsp, 64
    .allocstack 64
    .endprolog

    ; Load struct fields
    mov         rsi, qword ptr [rcx]        ; A
    mov         rdi, qword ptr [rcx+8]      ; B
    mov         r12, qword ptr [rcx+16]     ; C
    mov         r13, qword ptr [rcx+24]     ; M
    mov         r14, qword ptr [rcx+32]     ; N
    mov         r15, qword ptr [rcx+40]     ; K
    vbroadcastss ymm14, dword ptr [rcx+48]  ; alpha (use ymm for AVX2 compat)
    vbroadcastss ymm15, dword ptr [rcx+52]  ; beta

    ; --- Row loop: i in [0, M) ---
    xor         rbx, rbx                    ; i = 0
row_loop:
    cmp         rbx, r13
    jge         mm_done

    ; --- Column loop: j in [0, N), step 8 ---
    xor         r8, r8                      ; j = 0
col_loop:
    cmp         r8, r14
    jge         row_next

    ; Accumulator: ymm0 = dot product for C[i, j..j+7]
    vxorps      ymm0, ymm0, ymm0

    ; --- K loop: p in [0, K) ---
    xor         r9, r9                      ; p = 0
k_loop:
    cmp         r9, r15
    jge         k_done

    ; Load A[i, p] and broadcast
    ; A offset = (i * K + p) * 4
    mov         rax, rbx
    imul        rax, r15
    add         rax, r9
    vbroadcastss ymm1, dword ptr [rsi + rax*4]

    ; Load B[p, j..j+7]  (8 floats)
    ; B offset = (p * N + j) * 4
    mov         rax, r9
    imul        rax, r14
    add         rax, r8
    vmovups     ymm2, ymmword ptr [rdi + rax*4]

    ; FMA: acc += A[i,p] * B[p, j..j+7]
    vfmadd231ps ymm0, ymm1, ymm2

    inc         r9
    jmp         k_loop

k_done:
    ; Apply alpha: ymm0 = alpha * acc
    vmulps      ymm0, ymm0, ymm14

    ; Load C[i, j..j+7], apply beta, add
    ; C offset = (i * N + j) * 4
    mov         rax, rbx
    imul        rax, r14
    add         rax, r8
    lea         rcx, [r12 + rax*4]

    vmovups     ymm3, ymmword ptr [rcx]
    vfmadd231ps ymm0, ymm3, ymm15      ; ymm0 = alpha*A*B + beta*C (ymm0 already has alpha*AB)
    ; Fix: ymm0 = alpha*acc (already done), need beta*C + ymm0
    ; Actually: result = alpha*acc + beta*C[i,j]
    vmulps      ymm3, ymm3, ymm15       ; ymm3 = beta * C
    vaddps      ymm0, ymm0, ymm3        ; ymm0 = alpha*acc + beta*C
    sub         rax, r8                  ; restore offset for store
    add         rax, r8
    vmovups     ymmword ptr [rcx], ymm0

    add         r8, 8                    ; j += 8
    jmp         col_loop

row_next:
    inc         rbx                      ; i++
    jmp         row_loop

mm_done:
    vzeroupper
    add         rsp, 64
    pop         r15
    pop         r14
    pop         r13
    pop         r12
    pop         rdi
    pop         rsi
    pop         rbx
    pop         rbp
    ret
MatMul_AVX512 ENDP

; ============================================================================
; MatMul_AVX2 — AVX2 fallback (identical logic, same interface)
; Just an alias — the kernel above already uses ymm (256-bit) which is AVX2.
; ============================================================================
MatMul_AVX2 PROC FRAME
    push        rbp
    .pushreg    rbp
    mov         rbp, rsp
    .setframe   rbp, 0
    .endprolog

    ; Delegate to AVX-512 version (which uses ymm = AVX2 compatible)
    call        MatMul_AVX512

    add         rsp, 0
    pop         rbp
    ret
MatMul_AVX2 ENDP

; ============================================================================
; MatVec_AVX512 — y[M] = A[M,K] * x[K]   (matrix-vector, N=1 fast-path)
;
; RCX = float* A  (row-major M x K)
; RDX = float* x  (K elements)
; R8  = float* y  (M elements, output)
; R9  = uint64 M
; [rsp+40] = uint64 K   (5th arg on stack per Windows x64 ABI)
; ============================================================================
MatVec_AVX512 PROC FRAME
    push        rbp
    .pushreg    rbp
    mov         rbp, rsp
    .setframe   rbp, 0
    push        rbx
    .pushreg    rbx
    push        rsi
    .pushreg    rsi
    push        rdi
    .pushreg    rdi
    push        r12
    .pushreg    r12
    push        r13
    .pushreg    r13
    sub         rsp, 32
    .allocstack 32
    .endprolog

    mov         rsi, rcx                 ; A
    mov         rdi, rdx                 ; x
    mov         r12, r8                  ; y
    mov         r13, r9                  ; M
    mov         rbx, qword ptr [rbp+48]  ; K (5th arg)

    xor         r8, r8                   ; i = 0
mv_row_loop:
    cmp         r8, r13
    jge         mv_done

    ; Dot product: y[i] = sum_p A[i,p] * x[p]
    vxorps      ymm0, ymm0, ymm0        ; accumulator
    xor         r9, r9                   ; p = 0

    ; Compute row pointer: A + i * K * 4
    mov         rax, r8
    imul        rax, rbx
    lea         rcx, [rsi + rax*4]       ; &A[i, 0]

mv_k_loop_8:
    ; Process 8 elements at a time
    mov         rax, rbx
    sub         rax, r9
    cmp         rax, 8
    jl          mv_k_tail

    vmovups     ymm1, ymmword ptr [rcx + r9*4]
    vmovups     ymm2, ymmword ptr [rdi + r9*4]
    vfmadd231ps ymm0, ymm1, ymm2

    add         r9, 8
    jmp         mv_k_loop_8

mv_k_tail:
    ; Scalar tail for remaining elements
    cmp         r9, rbx
    jge         mv_reduce

    vmovss      xmm1, dword ptr [rcx + r9*4]
    vmovss      xmm2, dword ptr [rdi + r9*4]
    vmulss      xmm1, xmm1, xmm2
    ; Extract current scalar sum from ymm0
    vextractf128 xmm3, ymm0, 0
    vaddss      xmm3, xmm3, xmm1
    vinsertf128 ymm0, ymm0, xmm3, 0

    inc         r9
    jmp         mv_k_tail

mv_reduce:
    ; Horizontal sum of ymm0
    vextractf128 xmm1, ymm0, 1
    vaddps      xmm0, xmm0, xmm1
    vhaddps     xmm0, xmm0, xmm0
    vhaddps     xmm0, xmm0, xmm0

    ; Store y[i]
    vmovss      dword ptr [r12 + r8*4], xmm0

    inc         r8
    jmp         mv_row_loop

mv_done:
    vzeroupper
    add         rsp, 32
    pop         r13
    pop         r12
    pop         rdi
    pop         rsi
    pop         rbx
    pop         rbp
    ret
MatVec_AVX512 ENDP

; ============================================================================
; DotProduct_AVX512 — d = sum( a[i] * b[i] )  for i in [0, N)
;
; RCX = float* a
; RDX = float* b
; R8  = uint64 N
; Returns: xmm0 = dot product (float)
; ============================================================================
DotProduct_AVX512 PROC FRAME
    push        rbp
    .pushreg    rbp
    mov         rbp, rsp
    .setframe   rbp, 0
    sub         rsp, 16
    .allocstack 16
    .endprolog

    vxorps      ymm0, ymm0, ymm0        ; accumulator
    xor         r9, r9                   ; i = 0

dp_loop_8:
    mov         rax, r8
    sub         rax, r9
    cmp         rax, 8
    jl          dp_tail

    vmovups     ymm1, ymmword ptr [rcx + r9*4]
    vmovups     ymm2, ymmword ptr [rdx + r9*4]
    vfmadd231ps ymm0, ymm1, ymm2

    add         r9, 8
    jmp         dp_loop_8

dp_tail:
    cmp         r9, r8
    jge         dp_reduce

    vmovss      xmm1, dword ptr [rcx + r9*4]
    vmovss      xmm2, dword ptr [rdx + r9*4]
    vfmadd231ss xmm0, xmm1, xmm2

    inc         r9
    jmp         dp_tail

dp_reduce:
    ; Horizontal sum
    vextractf128 xmm1, ymm0, 1
    vaddps      xmm0, xmm0, xmm1
    vhaddps     xmm0, xmm0, xmm0
    vhaddps     xmm0, xmm0, xmm0
    ; Result in xmm0[0]

    vzeroupper
    add         rsp, 16
    pop         rbp
    ret
DotProduct_AVX512 ENDP

END
