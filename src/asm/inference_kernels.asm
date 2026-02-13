; RawrXD-Shell - High-Performance Inference Kernels (AVX-512)
; Architecture: x64
; Calling Convention: Windows x64 (RCX, RDX, R8, R9)

.code

; void matmul_f16_avx512_masm(const uint16_t* A, const uint16_t* B, float* C, int M, int N, int K)
; RCX = A, RDX = B, R8 = C, R9 = M, [rsp+40] = N, [rsp+48] = K
; NOTE: This is a reference implementation using scalar/simple instructions to ensure correctness.
; Optimized AVX-512 codepath requires ZMM registers availability verification.
matmul_f16_avx512_masm proc
    push rbp
    mov rbp, rsp
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    
    ; [rbp] = old rbp
    ; [rbp+8] = ret addr.
    ; [rbp+16] = shadow 1 (RCX home)
    ; ...
    ; [rbp+48] = arg5 N
    ; [rbp+56] = arg6 K

    mov r10d, dword ptr [rbp+48] ; N (int)
    mov r11d, dword ptr [rbp+56] ; K (int)
    
    xor rsi, rsi ; m = 0
loop_m:
    cmp esi, r9d
    jge done_m
    
    xor rdi, rdi ; n = 0
loop_n:
    cmp edi, r10d
    jge next_m
    
    vxorps xmm0, xmm0, xmm0 ; sum = 0.0
    
    xor rbx, rbx ; k = 0
loop_k:
    cmp ebx, r11d
    jge store_c
    
    ; Load A[m*K + k] (f16)
    ; Index = m*K + k
    mov rax, rsi
    imul rax, r11
    add rax, rbx
    lea r12, [rcx + rax*2] ; A is uint16_t* (2 bytes)
    pinsrw xmm1, word ptr [r12], 0
    vcvtph2ps xmm1, xmm1 ; Convert A (f16) to float
    
    ; Load B[n*K + k] (f16) - Assumes B is (N,K) layout?
    ; In previous C++ code: B[n*K + k]. This means B is N rows, K cols.
    mov rax, rdi
    imul rax, r11
    add rax, rbx
    lea r12, [rdx + rax*2]
    pinsrw xmm2, word ptr [r12], 0
    vcvtph2ps xmm2, xmm2 ; Convert B (f16) to float
    
    vmulss xmm1, xmm1, xmm2
    vaddss xmm0, xmm0, xmm1
    
    inc rbx
    jmp loop_k

store_c:
    ; C[m*N + n] = sum
    mov rax, rsi
    imul rax, r10
    add rax, rdi
    vmovss dword ptr [r8 + rax*4], xmm0
    
    inc rdi
    jmp loop_n

next_m:
    inc rsi
    jmp loop_m

done_m:
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    pop rbp
    ret
matmul_f16_avx512_masm endp

; void rmsnorm_avx512_masm(float* o, const float* x, const float* weight, int n, float eps)
; RCX = o, RDX = x, R8 = weight, R9 = n, [rsp+40] = eps
rmsnorm_avx512_masm proc
    push rbp
    mov rbp, rsp
    
    vmovss xmm4, dword ptr [rbp+48] ; eps
    
    ; sum = 0
    vxorps xmm0, xmm0, xmm0
    xor rax, rax ; i = 0
    
loop_sum:
    cmp eax, r9d
    jge calc_scale
    
    vmovss xmm1, dword ptr [rdx + rax*4]
    vmulss xmm1, xmm1, xmm1
    vaddss xmm0, xmm0, xmm1
    
    inc rax
    jmp loop_sum
    
calc_scale:
    vcvtsi2ss xmm2, xmm2, r9d ; n
    vdivss xmm0, xmm0, xmm2 ; mean
    vaddss xmm0, xmm0, xmm4 ; + eps
    vsqrtss xmm0, xmm0, xmm0 ; rms
    
    mov eax, 1065353216 ; 1.0f
    vmovd xmm3, eax
    vdivss xmm3, xmm3, xmm0 ; scale
    
    xor rax, rax
loop_apply:
    cmp eax, r9d
    jge done
    
    vmovss xmm1, dword ptr [rdx + rax*4]
    vmovss xmm2, dword ptr [r8 + rax*4]
    vmulss xmm1, xmm1, xmm3 ; * scale
    vmulss xmm1, xmm1, xmm2 ; * weight
    vmovss dword ptr [rcx + rax*4], xmm1
    
    inc rax
    jmp loop_apply
    
done:
    pop rbp
    ret
rmsnorm_avx512_masm endp

end
