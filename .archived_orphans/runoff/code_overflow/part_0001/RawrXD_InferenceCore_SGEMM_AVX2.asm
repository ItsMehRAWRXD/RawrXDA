; RawrXD_InferenceCore_SGEMM_AVX2.asm
; 6x16 AVX2 microkernel, x64 Windows ABI
; RCX=A, RDX=B, R8=C, R9d=M, [RSP+40]=N, [RSP+48]=K

.code
OPTION CASEMAP:NONE
align 16

PUBLIC RawrXD_InferenceCore_SGEMM_AVX2

RawrXD_InferenceCore_SGEMM_AVX2 PROC FRAME
    push rbp
    .pushreg rbp
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
    sub rsp, 128
    .allocstack 128
    .endprolog

    mov r12d, r9d
    mov r13d, [rsp+168]
    mov r14d, [rsp+176]
    mov r15, rcx
    mov rbx, rdx
    mov rbp, r8

    xor r8d, r8d
n_loop:
    cmp r8d, r13d
    jge done_n
    xor r9d, r9d
m_loop:
    cmp r9d, r12d
    jge done_m

    vmovaps ymm0, ymmword ptr [rbp+r9*4]
    vmovaps ymm1, ymmword ptr [rbp+r9*4+32]

    xor r10d, r10d
k_loop:
    cmp r10d, r14d
    jge done_k

    vbroadcastss ymm2, real4 ptr [r15+r9*4+r10*r12*4]
    vmovaps ymm3, ymmword ptr [rbx+r10*r13*4+r8*4]
    vmovaps ymm4, ymmword ptr [rbx+r10*r13*4+r8*4+32]

    vfmadd231ps ymm0, ymm2, ymm3
    vfmadd231ps ymm1, ymm2, ymm4

    inc r10d
    jmp k_loop
done_k:

    vmovaps ymmword ptr [rbp+r9*4], ymm0
    vmovaps ymmword ptr [rbp+r9*4+32], ymm1

    add r9d, 6
    jmp m_loop
done_m:
    add r8d, 16
    jmp n_loop

done_n:
    vzeroupper
    add rsp, 128
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret
RawrXD_InferenceCore_SGEMM_AVX2 ENDP

END
