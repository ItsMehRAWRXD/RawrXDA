OPTION CASEMAP:NONE
includelib kernel32.lib
includelib ntdll.lib

EXTERN CreateFileA : PROC
EXTERN CreateFileMappingA : PROC
EXTERN MapViewOfFile : PROC
EXTERN GetFileSizeEx : PROC
EXTERN CloseHandle : PROC
EXTERN UnmapViewOfFile : PROC
EXTERN WriteConsoleA : PROC
EXTERN GetStdHandle : PROC

.const
 GGUF_MAGIC         EQU 046554747h
 GGUF_VERSION       EQU 3
 TYPE_F32           EQU 0
 TYPE_Q4_0          EQU 2
 TYPE_Q2_K          EQU 14
 ROPE_THETA         EQU 10000.0

TransformerCtx STRUCT
    pFileBase          QWORD ?
    cbFileSize         QWORD ?
    pTensorIndex       QWORD ?
    Architecture       DWORD ?
    nLayers            DWORD ?
    nHeads             DWORD ?
    nEmbed             DWORD ?
    nCtx              DWORD ?
TransformerCtx ENDS

.code
ALIGN 16

Dequantize_Q2_K_Block PROC FRAME
    push rbx
    push r12
    push r13
    push r14
    push r15
    sub rsp, 32
    .endprolog
    
    mov r12, rcx
    mov r13, rdx
    xor r15, r15
    
@loop:
    cmp r15, 256
    jae @done
    
    mov rax, r15
    shr rax, 2
    movzx ebx, BYTE PTR [r12+12+rax]
    
    mov rcx, r15
    and rcx, 3
    shl rcx, 1
    shr ebx, cl
    and ebx, 3
    
    vcvtsi2ss xmm0, xmm0, ebx
    
    mov eax, 03FC00000h
    vmovd xmm1, eax
    vsubss xmm0, xmm0, xmm1
    
    mov eax, 03DFCD35Bh
    vmovd xmm1, eax
    vmulss xmm0, xmm0, xmm1
    
    vmovss DWORD PTR [r13+r15*4], xmm0
    
    inc r15
    jmp @loop
    
@done:
    add rsp, 32
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
Dequantize_Q2_K_Block ENDP

LayerNorm PROC FRAME
    push rbx
    .endprolog
    mov rbx, r9
    
    vxorps xmm0, xmm0, xmm0
    mov rax, rcx
    
@sum_sq:
    vmovss xmm1, DWORD PTR [rax]
    vmulss xmm1, xmm1, xmm1
    vaddss xmm0, xmm0, xmm1
    add rax, 4
    dec rbx
    jnz @sum_sq
    
    vcvtsi2ss xmm1, xmm1, r9
    vdivss xmm0, xmm0, xmm1
    
    mov eax, 03727C5ACh
    vmovd xmm2, eax
    vaddss xmm0, xmm0, xmm2
    
    vsqrtss xmm0, xmm0, xmm0
    
    mov eax, 03F800000h
    vmovd xmm2, eax
    vdivss xmm0, xmm2, xmm0
    
    pop rbx
    ret
LayerNorm ENDP

PUBLIC Titan_RunInference
Titan_RunInference PROC FRAME
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    push r13
    .endprolog
    
    mov rbx, rcx
    
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret
Titan_RunInference ENDP

PUBLIC GGUF_LoadFile
GGUF_LoadFile PROC FRAME
    push rbx
    push rsi
    push rdi
    sub rsp, 48               ; Shadow space for calls + alignment
    .endprolog
    
    mov rbx, rcx              ; pszPath
    mov rsi, rdx              ; pCtx
    
    ; 1. CreateFileA
    ; rcx=path, rdx=GENERIC_READ(0x80000000), r8=SHARE_READ(1)
    ; r9=NULL, stack=[OPEN_EXISTING(3), ATTR_NORMAL(0x80), 0]
    mov rcx, rbx
    mov edx, 80000000h        ; GENERIC_READ
    mov r8d, 1                ; FILE_SHARE_READ
    xor r9, r9                ; Security
    mov QWORD PTR [rsp+32], 3 ; OPEN_EXISTING
    mov QWORD PTR [rsp+40], 0 ; Flags/Template
    mov QWORD PTR [rsp+48], 0
    call CreateFileA
    
    cmp rax, -1               ; INVALID_HANDLE_VALUE
    je @fail
    
    mov rdi, rax              ; hFile
    
    ; 2. GetFileSizeEx
    lea rdx, [rsi].TransformerCtx.cbFileSize
    mov rcx, rdi
    call GetFileSizeEx
    
    ; 3. CreateFileMappingA
    ; rcx=hFile, rdx=0, r8=PAGE_READONLY(2), r9=0, stack=[0, 0]
    mov rcx, rdi
    xor rdx, rdx
    mov r8d, 2                ; PAGE_READONLY
    xor r9, r9
    mov QWORD PTR [rsp+32], 0
    mov QWORD PTR [rsp+40], 0
    call CreateFileMappingA
    
    cmp rax, 0
    je @fail_close
    
    mov rbx, rax              ; hMap
    
    ; 4. MapViewOfFile
    ; rcx=hMap, rdx=FILE_MAP_READ(4), r8=0, r9=0, stack=[0]
    mov rcx, rbx
    mov edx, 4                ; FILE_MAP_READ
    xor r8, r8
    xor r9, r9
    mov QWORD PTR [rsp+32], 0
    call MapViewOfFile
    
    cmp rax, 0
    je @fail_close_map
    
    mov [rsi].TransformerCtx.pFileBase, rax
    
    ; 5. Verify GGUF Magic "GGUF" = 0x46554747
    mov ecx, DWORD PTR [rax]
    cmp ecx, GGUF_MAGIC
    jne @fail_unmap
    
    ; Success - we keep the mapping open, but can close handles?
    ; Usually we keep handles to be clean, but for this test leak is fine or store them.
    ; Closing file/map handles does not invalidate view.
    mov rcx, rbx
    call CloseHandle
    mov rcx, rdi
    call CloseHandle
    
    mov eax, 1
    jmp @done
    
@fail_unmap:
    mov rcx, [rsi].TransformerCtx.pFileBase
    call UnmapViewOfFile
@fail_close_map:
    mov rcx, rbx
    call CloseHandle
@fail_close:
    mov rcx, rdi
    call CloseHandle
@fail:
    xor eax, eax
@done:
    add rsp, 48
    pop rdi
    pop rsi
    pop rbx
    ret
GGUF_LoadFile ENDP

PUBLIC MatMul_Q2_K
MatMul_Q2_K PROC FRAME
    .endprolog
    ret
MatMul_Q2_K ENDP

END
