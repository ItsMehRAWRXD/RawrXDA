; Kernel launch stubs (MASM x64)
option casemap:none

; ─── Cross-module symbol resolution ───
INCLUDE rawrxd_master.inc


EXTERN HybridGPU_Init:PROC
EXTERN HybridGPU_MatMul:PROC
EXTERN HybridCPU_MatMul:PROC
EXTERN HybridGPU_Synchronize:PROC

PUBLIC launch_matmul_kernel
PUBLIC launch_vector_add_kernel
PUBLIC launch_element_mul_kernel
PUBLIC gpu_synchronize

.code

; ---------------------------------------------------------------------------
; launch_matmul_kernel
; ---------------------------------------------------------------------------
; Bridges MASM callers to the hybrid Vulkan/AVX-512 matmul implementation.
; Tries the GPU path first, then falls back to AVX-512 CPU math.
; Inputs: RCX=A, RDX=B, R8=C, R9=M, [rsp+40]=N, [rsp+48]=K
; Output: RAX=0 on success, -1 on failure
launch_matmul_kernel PROC
    push rbp
    mov rbp, rsp

    ; Capture stack-based args before we touch RSP (arg5=N, arg6=K)
    mov r12, qword ptr [rbp + 30h]
    mov r13, qword ptr [rbp + 38h]

    push rbx
    push rsi
    push rdi
    push r14
    sub  rsp, 48                     ; shadow space (32) + two stack args

    mov rbx, rcx                     ; A
    mov rsi, rdx                     ; B
    mov rdi, r8                      ; C
    mov r10, r9                      ; M
    mov r14, r13                     ; K cached in callee-saved reg
    mov r11, r12                     ; N cached

    ; GPU path (best-effort)
    call HybridGPU_Init

    mov rcx, rbx
    mov rdx, rsi
    mov r8,  rdi
    mov r9,  r10
    mov qword ptr [rsp + 32], r11    ; N
    mov qword ptr [rsp + 40], r14    ; K
    call HybridGPU_MatMul

    test rax, rax
    je  matmul_success               ; 0 = success

    ; CPU AVX-512 fallback
    mov rcx, rbx
    mov rdx, rsi
    mov r8,  rdi
    mov r9,  r10
    mov qword ptr [rsp + 32], r11    ; N
    mov qword ptr [rsp + 40], r14    ; K
    call HybridCPU_MatMul
    test rax, rax
    jne matmul_failure

matmul_success:
    xor rax, rax
    jmp matmul_epilogue

matmul_failure:
    mov rax, -1

matmul_epilogue:
    add rsp, 48
    pop r14
    pop rdi
    pop rsi
    pop rbx
    pop rbp
    ret
launch_matmul_kernel ENDP

; ---------------------------------------------------------------------------
; launch_vector_add_kernel
; ---------------------------------------------------------------------------
; Element-wise add using AVX-512 with scalar tail handling.
; Inputs: RCX=a, RDX=b, R8=result, R9=size
; Output: RAX=0 on success
launch_vector_add_kernel PROC
    push rbp
    mov rbp, rsp
    push rbx
    push rsi
    push rdi
    sub  rsp, 32                     ; shadow space for any helper calls (none now)

    mov rdi, rcx                     ; a
    mov rsi, rdx                     ; b
    mov rbx, r8                      ; result
    mov r10, r9                      ; size

vec_add_loop:
    cmp r10, 16
    jb  vec_add_tail

    vmovups zmm0, zword ptr [rdi]
    vmovups zmm1, zword ptr [rsi]
    vaddps zmm0, zmm0, zmm1
    vmovups zword ptr [rbx], zmm0

    add rdi, 64
    add rsi, 64
    add rbx, 64
    sub r10, 16
    jmp vec_add_loop

vec_add_tail:
    test r10, r10
    jz   vec_add_done

    mov rcx, r10
vec_add_scalar:
    movss xmm0, dword ptr [rdi]
    addss xmm0, dword ptr [rsi]
    movss dword ptr [rbx], xmm0
    add rdi, 4
    add rsi, 4
    add rbx, 4
    sub rcx, 1
    jnz vec_add_scalar

vec_add_done:
    vzeroupper
    xor rax, rax
    add rsp, 32
    pop rdi
    pop rsi
    pop rbx
    pop rbp
    ret
launch_vector_add_kernel ENDP

; ---------------------------------------------------------------------------
; launch_element_mul_kernel
; ---------------------------------------------------------------------------
; Element-wise multiply using AVX-512 with scalar tail handling.
; Inputs: RCX=a, RDX=b, R8=result, R9=size
; Output: RAX=0 on success
launch_element_mul_kernel PROC
    push rbp
    mov rbp, rsp
    push rbx
    push rsi
    push rdi
    sub  rsp, 32

    mov rdi, rcx                     ; a
    mov rsi, rdx                     ; b
    mov rbx, r8                      ; result
    mov r10, r9                      ; size

vec_mul_loop:
    cmp r10, 16
    jb  vec_mul_tail

    vmovups zmm0, zword ptr [rdi]
    vmovups zmm1, zword ptr [rsi]
    vmulps zmm0, zmm0, zmm1
    vmovups zword ptr [rbx], zmm0

    add rdi, 64
    add rsi, 64
    add rbx, 64
    sub r10, 16
    jmp vec_mul_loop

vec_mul_tail:
    test r10, r10
    jz   vec_mul_done

    mov rcx, r10
vec_mul_scalar:
    movss xmm0, dword ptr [rdi]
    mulss xmm0, dword ptr [rsi]
    movss dword ptr [rbx], xmm0
    add rdi, 4
    add rsi, 4
    add rbx, 4
    sub rcx, 1
    jnz vec_mul_scalar

vec_mul_done:
    vzeroupper
    xor rax, rax
    add rsp, 32
    pop rdi
    pop rsi
    pop rbx
    pop rbp
    ret
launch_element_mul_kernel ENDP

; ---------------------------------------------------------------------------
; gpu_synchronize
; ---------------------------------------------------------------------------
; Flushes any outstanding GPU work (used by async Vulkan path).
; Output: RAX=0 on success, -1 on failure
gpu_synchronize PROC
    sub rsp, 32
    call HybridGPU_Synchronize
    add rsp, 32
    ret
gpu_synchronize ENDP

END
