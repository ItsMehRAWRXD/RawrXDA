; Clean MASM64 baseline for Titan DMA/Kernel exports
OPTION CASEMAP:NONE

PUBLIC Titan_ExecuteComputeKernel
PUBLIC Titan_PerformCopy
PUBLIC Titan_PerformDMA
PUBLIC Titan_InitializeDMA
PUBLIC Titan_ShutdownDMA
PUBLIC Titan_GetDMAStats

TITAN_SUCCESS             EQU 0
TITAN_ERROR_INVALID_PARAM EQU 80000001h

KERNEL_TYPE_NF4           EQU 0
KERNEL_TYPE_PREFETCH      EQU 1
KERNEL_TYPE_COPY          EQU 2

.data
ALIGN 8
g_TotalBytesCopied   QWORD 0
g_TotalOps           QWORD 0
g_FailedOps          QWORD 0
g_Initialized        BYTE  0

.code

Kernel_Copy PROC PRIVATE
    ; RCX=src, RDX=dst, R8=size
    test rcx, rcx
    jz   kc_bad
    test rdx, rdx
    jz   kc_bad
    test r8, r8
    jz   kc_bad

    push rsi
    push rdi
    mov rsi, rcx
    mov rdi, rdx
    mov rcx, r8
    cld
    rep movsb
    pop rdi
    pop rsi

    add g_TotalBytesCopied, r8
    inc g_TotalOps
    xor eax, eax
    ret
kc_bad:
    inc g_FailedOps
    mov eax, TITAN_ERROR_INVALID_PARAM
    ret
Kernel_Copy ENDP

Kernel_Prefetch PROC PRIVATE
    ; RCX=addr, RDX=size
    test rcx, rcx
    jz   kp_done
    test rdx, rdx
    jz   kp_done
kp_loop:
    prefetcht0 [rcx]
    add rcx, 64
    sub rdx, 64
    ja  kp_loop
kp_done:
    xor eax, eax
    ret
Kernel_Prefetch ENDP

Kernel_NF4_Decompress PROC PRIVATE
    ; RCX=src, RDX=dst, R8=blocks (Q4_0-like: 2-byte scale + 16-byte packed quants)
    ; Output: 32 WORD values per block. This baseline unpacks signed nibbles (q-8).
    test rcx, rcx
    jz   kn_bad
    test rdx, rdx
    jz   kn_bad
    test r8, r8
    jz   kn_bad

    push rbx
    push rsi
    push rdi

    mov rsi, rcx                ; src blocks
    mov rdi, rdx                ; dst words
    xor r9, r9                  ; block index

kn_block_loop:
    cmp r9, r8
    jae kn_done

    ; Block layout: +0..1 = scale (ignored in this baseline), +2..17 = 16 packed bytes
    mov rbx, r9
    imul rbx, 18
    lea r10, [rsi + rbx + 2]    ; packed nibble data for this block

    ; Destination: 32 WORDs per block
    mov rbx, r9
    shl rbx, 5                  ; *32
    lea r11, [rdi + rbx*2]      ; WORD destination

    xor ecx, ecx                ; i = 0..15
kn_byte_loop:
    cmp ecx, 16
    jae kn_next_block

    movzx eax, BYTE PTR [r10 + rcx]

    ; low nibble -> signed value [-8,7]
    mov edx, eax
    and edx, 0Fh
    sub edx, 8
    mov WORD PTR [r11 + rcx*4], dx

    ; high nibble -> signed value [-8,7]
    shr eax, 4
    and eax, 0Fh
    sub eax, 8
    mov WORD PTR [r11 + rcx*4 + 2], ax

    inc ecx
    jmp kn_byte_loop

kn_next_block:
    inc r9
    jmp kn_block_loop

kn_done:
    pop rdi
    pop rsi
    pop rbx

    add g_TotalBytesCopied, r8
    inc g_TotalOps
    xor eax, eax
    ret
kn_bad:
    inc g_FailedOps
    mov eax, TITAN_ERROR_INVALID_PARAM
    ret
Kernel_NF4_Decompress ENDP

Titan_ExecuteComputeKernel PROC
    ; ECX=kernelType, RDX=params, R8=cmdBuf(unused), R9=outTimeUs
    test rdx, rdx
    jz   te_bad

    ; params layout (best-effort): +0 src, +8 dst, +16 size
    mov r10, rdx
    mov rcx, QWORD PTR [r10]
    mov rdx, QWORD PTR [r10+8]
    mov r8,  QWORD PTR [r10+16]

    cmp ecx, KERNEL_TYPE_NF4
    je  te_nf4
    cmp ecx, KERNEL_TYPE_PREFETCH
    je  te_pref
    cmp ecx, KERNEL_TYPE_COPY
    je  te_copy
    jmp te_bad

te_nf4:
    call Kernel_NF4_Decompress
    jmp te_done
te_pref:
    call Kernel_Prefetch
    jmp te_done
te_copy:
    call Kernel_Copy
    jmp te_done

te_bad:
    inc g_FailedOps
    mov eax, TITAN_ERROR_INVALID_PARAM

te_done:
    test r9, r9
    jz   te_ret
    mov QWORD PTR [r9], 0
te_ret:
    ret
Titan_ExecuteComputeKernel ENDP

Titan_PerformCopy PROC
    ; ECX=direction(unused), RDX=src, R8=dst, R9=size
    mov rcx, rdx
    mov rdx, r8
    mov r8,  r9
    call Kernel_Copy
    ret
Titan_PerformCopy ENDP

Titan_PerformDMA PROC
    ; ECX=dmaType(unused), RDX=request*, R8=event(unused), R9=timeout(unused)
    test rdx, rdx
    jz   td_bad

    ; request layout: +8 src, +16 dst, +24 size, +48 status
    mov r10, rdx
    mov rcx, QWORD PTR [r10+8]
    mov rdx, QWORD PTR [r10+16]
    mov r8,  QWORD PTR [r10+24]
    call Kernel_Copy
    test eax, eax
    jnz  td_err

    mov DWORD PTR [r10+48], 1
    xor eax, eax
    ret

td_err:
    mov DWORD PTR [r10+48], 3
    ret

td_bad:
    inc g_FailedOps
    mov eax, TITAN_ERROR_INVALID_PARAM
    ret
Titan_PerformDMA ENDP

Titan_InitializeDMA PROC
    mov g_Initialized, 1
    xor eax, eax
    ret
Titan_InitializeDMA ENDP

Titan_ShutdownDMA PROC
    mov g_Initialized, 0
    xor eax, eax
    ret
Titan_ShutdownDMA ENDP

Titan_GetDMAStats PROC
    ; RCX=outStats ptr (expects at least 24 bytes)
    test rcx, rcx
    jz   tgs_bad
    mov rax, g_TotalBytesCopied
    mov QWORD PTR [rcx], rax
    mov rax, g_TotalOps
    mov QWORD PTR [rcx+8], rax
    mov rax, g_FailedOps
    mov QWORD PTR [rcx+16], rax
    xor eax, eax
    ret

tgs_bad:
    mov eax, TITAN_ERROR_INVALID_PARAM
    ret
Titan_GetDMAStats ENDP

END