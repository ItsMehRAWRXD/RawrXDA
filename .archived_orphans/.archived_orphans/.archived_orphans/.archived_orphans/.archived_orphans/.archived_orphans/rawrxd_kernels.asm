; rawrxd_kernels.asm - MASM64 layer (SGEMM AVX2 + MemPatch)
; Build: ml64 /c /Fo rawrxd_kernels.obj rawrxd_kernels.asm

EXTERN VirtualProtect:PROC

.code
OPTION CASEMAP:NONE
align 16

PUBLIC RawrXD_SGEMM_AVX2
PUBLIC RawrXD_MemPatch

; RCX=A, RDX=B, R8=C, R9=M, [RSP+28h]=N, [RSP+30h]=K (at entry)
; After prolog: N at [rsp+90h], K at [rsp+98h]
RawrXD_SGEMM_AVX2 PROC FRAME
    push rbx
    .pushreg rbx
    push rdi
    .pushreg rdi
    push rsi
    .pushreg rsi
    push r12
    .pushreg r12
    push r13
    .pushreg r13
    sub rsp, 40h
    .allocstack 40h
    .endprolog

    mov rbx, rcx
    mov rdi, rdx
    mov rsi, r8
    mov r10, r9
    mov r11, [rsp+90h]
    mov r12, [rsp+98h]

    xor rax, rax
m_loop:
    cmp rax, r10
    jge m_done
    xor rcx, rcx
n_loop:
    cmp rcx, r11
    jge n_done

    vxorps ymm0, ymm0, ymm0
    vxorps ymm1, ymm1, ymm1
    vxorps ymm2, ymm2, ymm2
    vxorps ymm3, ymm3, ymm3
    vxorps ymm4, ymm4, ymm4
    vxorps ymm5, ymm5, ymm5

    mov rdx, r12
k_loop:
    test rdx, rdx
    jz k_done

    vmovups ymm15, ymmword ptr [rdi + (rdx-1)*r11*4 + rcx*4]
    vbroadcastss ymm6, real4 ptr [rbx + (rax+0)*r12*4 + (rdx-1)*4]
    vbroadcastss ymm7, real4 ptr [rbx + (rax+1)*r12*4 + (rdx-1)*4]
    vbroadcastss ymm8, real4 ptr [rbx + (rax+2)*r12*4 + (rdx-1)*4]
    vbroadcastss ymm9, real4 ptr [rbx + (rax+3)*r12*4 + (rdx-1)*4]
    vbroadcastss ymm10, real4 ptr [rbx + (rax+4)*r12*4 + (rdx-1)*4]
    vbroadcastss ymm11, real4 ptr [rbx + (rax+5)*r12*4 + (rdx-1)*4]

    vfmadd231ps ymm0, ymm6, ymm15
    vfmadd231ps ymm1, ymm7, ymm15
    vfmadd231ps ymm2, ymm8, ymm15
    vfmadd231ps ymm3, ymm9, ymm15
    vfmadd231ps ymm4, ymm10, ymm15
    vfmadd231ps ymm5, ymm11, ymm15

    dec rdx
    jmp k_loop
k_done:

    vmovups ymmword ptr [rsi + (rax+0)*r11*4 + rcx*4], ymm0
    vmovups ymmword ptr [rsi + (rax+1)*r11*4 + rcx*4], ymm1
    vmovups ymmword ptr [rsi + (rax+2)*r11*4 + rcx*4], ymm2
    vmovups ymmword ptr [rsi + (rax+3)*r11*4 + rcx*4], ymm3
    vmovups ymmword ptr [rsi + (rax+4)*r11*4 + rcx*4], ymm4
    vmovups ymmword ptr [rsi + (rax+5)*r11*4 + rcx*4], ymm5

    add rcx, 16
    jmp n_loop
n_done:
    add rax, 6
    jmp m_loop
m_done:

    vzeroupper
    add rsp, 40h
    pop r13
    pop r12
    pop rsi
    pop rdi
    pop rbx
    ret
RawrXD_SGEMM_AVX2 ENDP

; RCX=target, RDX=bytes, R8=len
RawrXD_MemPatch PROC FRAME
    push rbx
    .pushreg rbx
    push rsi
    .pushreg rsi
    push rdi
    .pushreg rdi
    sub rsp, 30h
    .allocstack 30h
    .endprolog

    mov rbx, rcx
    mov rsi, rdx
    mov rdi, rcx
    mov rcx, r8
    mov r12, r8

    mov rcx, rbx
    mov rdx, r12
    mov r8d, 40h
    lea r9, [rsp+28h]
    call VirtualProtect

    mov rcx, r12
    rep movsb

    mov rcx, rbx
    mov rdx, r12
    mov r8d, [rsp+28h]
    lea r9, [rsp+20h]
    call VirtualProtect

    mov al, 1
    add rsp, 30h
    pop rdi
    pop rsi
    pop rbx
    ret
RawrXD_MemPatch ENDP

END
