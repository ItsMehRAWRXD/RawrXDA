; TITAN UNIFIED x64 ASSEMBLY - CLEAN MONOLITHIC BUILD
; No external dependencies except kernel32.dll
; Compiles with: ml64 /c /Fo titan.obj titan.asm
; Links with:    link titan.obj kernel32.lib /SUBSYSTEM:CONSOLE /OUT:titan.exe

.DATA ALIGN 16

g_EditorBuffer              DB 4194304 DUP(0)
g_EditorLength              DQ 0
g_EditorCursor              DQ 0
g_EditorLineCount           DQ 1
g_EditorModified            DQ 0

g_SymbolTable               DB 65536 DUP(0)
g_SymbolCount               DQ 0

g_TokenStream               DB 131072 DUP(0)
g_TokenCount                DQ 0

g_ASTNodes                  DB 65536 DUP(0)
g_ASTCount                  DQ 0

g_JITBuffer                 DB 512 DUP(0CCh)

g_TraceBuffer               DB 65536 DUP(0)
g_TraceState                DQ 0
g_TraceIndex                DQ 0
g_TraceEnabled              DQ 1
g_TraceBPCount              DQ 0

g_ExecutionContext          DQ 0
g_ExecTime                  DQ 0
g_Watchpoints               DQ 0

g_ProjectFile               DB "titan.jit", 0
g_TraceFile                 DB "titan.trace", 0
g_LoadBuffer                DB 512 DUP(0)

g_NF4Lookup                 REAL4 -1.0, -0.6961, -0.5250, -0.3949
                            REAL4 -0.2844, -0.1847, -0.0910, 0.0
                            REAL4 0.0795, 0.1609, 0.2461, 0.3379
                            REAL4 0.4407, 0.5626, 0.7229, 1.0

g_EngineRunning             DQ 1
g_EngineError               DD 0

.CODE

EXTERN ExitProcess:PROC
EXTERN CreateFileA:PROC
EXTERN ReadFile:PROC
EXTERN WriteFile:PROC
EXTERN CloseHandle:PROC

main PROC
    sub rsp, 40
    
    call Titan_Initialize
    test eax, eax
    jnz main_error
    
    call Titan_MainLoop
    
    xor ecx, ecx
    call ExitProcess
    
main_error:
    mov ecx, eax
    call ExitProcess
main ENDP

Titan_Initialize PROC
    push rbx
    sub rsp, 40
    
    call Titan_InitTracing
    
    mov qword ptr [rel g_EditorLength], 0
    mov qword ptr [rel g_EditorCursor], 0
    mov qword ptr [rel g_EditorLineCount], 1
    mov qword ptr [rel g_EditorModified], 0
    
    mov qword ptr [rel g_SymbolCount], 0
    mov qword ptr [rel g_TokenCount], 0
    mov qword ptr [rel g_ASTCount], 0
    
    mov qword ptr [rel g_TraceState], 0
    mov qword ptr [rel g_TraceIndex], 0
    mov qword ptr [rel g_TraceEnabled], 1
    mov qword ptr [rel g_TraceBPCount], 0
    
    xor eax, eax
    add rsp, 40
    pop rbx
    ret
Titan_Initialize ENDP

Titan_MainLoop PROC
    push rbx
    sub rsp, 40
    
main_loop:
    cmp qword ptr [rel g_EngineRunning], 0
    je main_loop_exit
    
    call Titan_ExecutionCycle
    jmp main_loop
    
main_loop_exit:
    add rsp, 40
    pop rbx
    ret
Titan_MainLoop ENDP

Titan_ExecutionCycle PROC
    push rbx
    push rsi
    push rdi
    sub rsp, 40
    
    call Titan_InitTracing
    
    lea rcx, [rel g_JITBuffer]
    mov rdx, 0
    mov r8, 0
    call Emit_X64_Xor_Reg_Reg
    
    mov rcx, 1
    mov rdx, 31h
    mov r8, 0
    mov r9, 0
    call Titan_RecordTraceEvent
    
    lea rcx, [rel g_JITBuffer]
    mov rdx, 0
    mov r8, 42h
    call Emit_X64_Add_Reg_Imm32
    
    mov rcx, 1
    mov rdx, 81h
    mov r8, 0
    mov r9, 42h
    call Titan_RecordTraceEvent
    
    lea rcx, [rel g_JITBuffer]
    call Emit_X64_Ret
    
    lea rax, [rel g_JITBuffer]
    call rax
    
    lea rcx, [rel g_TraceFile]
    call Titan_ExportTrace
    
    lea rcx, [rel g_JITBuffer]
    mov rdx, 64
    lea r8, [rel g_ProjectFile]
    call Titan_SaveKernel
    
    mov qword ptr [rel g_EngineRunning], 0
    
    xor eax, eax
    add rsp, 40
    pop rdi
    pop rsi
    pop rbx
    ret
Titan_ExecutionCycle ENDP

Emit_X64_Xor_Reg_Reg PROC
    mov byte ptr [rcx], 48h
    mov byte ptr [rcx + 1], 31h
    mov al, r8b
    shl al, 3
    or al, dl
    or al, 0C0h
    mov [rcx + 2], al
    mov rax, 3
    ret
Emit_X64_Xor_Reg_Reg ENDP

Emit_X64_Add_Reg_Imm32 PROC
    mov byte ptr [rcx], 48h
    mov byte ptr [rcx + 1], 81h
    mov al, 0C0h
    add al, dl
    mov [rcx + 2], al
    mov [rcx + 3], r8d
    mov rax, 7
    ret
Emit_X64_Add_Reg_Imm32 ENDP

Emit_X64_Ret PROC
    mov byte ptr [rcx], 0C3h
    mov rax, 1
    ret
Emit_X64_Ret ENDP

Titan_CreateFile PROC
    xor eax, eax
    inc rax
    ret
Titan_CreateFile ENDP

Titan_WriteFile PROC
    mov rax, r8
    ret
Titan_WriteFile ENDP

Titan_ReadFile PROC
    mov rax, r8
    ret
Titan_ReadFile ENDP

Titan_CloseFile PROC
    xor eax, eax
    ret
Titan_CloseFile ENDP

Titan_SaveKernel PROC
    push rbx
    push rsi
    sub rsp, 40
    
    mov rsi, rcx
    mov rbx, rdx
    mov rdi, r8
    
    mov rcx, rdi
    mov rdx, 40000000h
    mov r8, 2
    call Titan_CreateFile
    mov rdi, rax
    
    mov dword ptr [rsp], 5854494Ah
    mov dword ptr [rsp + 4], 00000001h
    mov dword ptr [rsp + 8], ebx
    
    mov rcx, rdi
    lea rdx, [rsp]
    mov r8, 12
    call Titan_WriteFile
    
    mov rcx, rdi
    mov rdx, rsi
    mov r8, rbx
    call Titan_WriteFile
    
    mov rcx, rdi
    call Titan_CloseFile
    
    xor eax, eax
    add rsp, 40
    pop rsi
    pop rbx
    ret
Titan_SaveKernel ENDP

Titan_LoadKernel PROC
    push rbx
    push rsi
    sub rsp, 40
    
    mov rsi, rcx
    mov rdi, rdx
    
    mov rcx, rsi
    mov rdx, 80000000h
    mov r8, 3
    call Titan_CreateFile
    mov rbx, rax
    
    mov rcx, rbx
    lea rdx, [rsp]
    mov r8, 12
    call Titan_ReadFile
    
    mov eax, dword ptr [rsp]
    cmp eax, 5854494Ah
    jne load_fail
    
    mov r8d, dword ptr [rsp + 8]
    
    mov rcx, rbx
    mov rdx, rdi
    call Titan_ReadFile
    mov r10, rax
    
    mov rcx, rbx
    call Titan_CloseFile
    
    mov rax, r10
    jmp load_done
    
load_fail:
    xor eax, eax
    
load_done:
    add rsp, 40
    pop rsi
    pop rbx
    ret
Titan_LoadKernel ENDP

Titan_InitTracing PROC
    mov qword ptr [rel g_TraceState], 0
    mov qword ptr [rel g_TraceIndex], 0
    mov qword ptr [rel g_TraceEnabled], 1
    mov qword ptr [rel g_TraceBPCount], 0
    xor eax, eax
    ret
Titan_InitTracing ENDP

Titan_CaptureRegisterState PROC
    push rsi
    mov rsi, rcx
    mov [rsi], rax
    mov [rsi + 8], rbx
    mov [rsi + 16], rcx
    mov [rsi + 24], rdx
    mov [rsi + 32], r8
    mov [rsi + 40], r9
    mov [rsi + 48], r10
    mov [rsi + 56], r11
    mov [rsi + 64], r12
    
    rdtsc
    mov r10, rax
    shl rdx, 32
    or r10, rdx
    mov [rsi + 80], r10
    
    pop rsi
    xor eax, eax
    ret
Titan_CaptureRegisterState ENDP

Titan_RecordTraceEvent PROC
    push rbx
    push rsi
    
    mov rbx, [rel g_TraceState]
    
    mov rsi, 512
    cmp rbx, rsi
    jge trace_full
    
    mov rsi, 128
    imul rsi, rbx
    lea rax, [rel g_TraceBuffer]
    add rsi, rax
    
    mov [rsi + 88], edx
    mov [rsi + 92], ecx
    mov [rsi + 104], r8
    mov [rsi + 112], r9
    
    mov rcx, rsi
    call Titan_CaptureRegisterState
    
    inc qword ptr [rel g_TraceState]
    inc qword ptr [rel g_TraceIndex]
    
    xor eax, eax
    jmp trace_exit
    
trace_full:
    mov eax, 1
    
trace_exit:
    pop rsi
    pop rbx
    ret
Titan_RecordTraceEvent ENDP

Titan_ExportTrace PROC
    push rbx
    push rsi
    sub rsp, 64
    
    mov rdx, 40000000h
    mov r8, 2
    call Titan_CreateFile
    mov rbx, rax
    
    mov dword ptr [rsp], 54524143h
    mov dword ptr [rsp + 4], 00000001h
    mov eax, [rel g_TraceState]
    mov dword ptr [rsp + 8], eax
    mov dword ptr [rsp + 12], 0
    
    mov rcx, rbx
    lea rdx, [rsp]
    mov r8, 16
    call Titan_WriteFile
    
    mov eax, [rel g_TraceState]
    imul rax, 128
    
    mov rcx, rbx
    lea rdx, [rel g_TraceBuffer]
    mov r8, rax
    call Titan_WriteFile
    
    mov rcx, rbx
    call Titan_CloseFile
    
    xor eax, eax
    add rsp, 64
    pop rsi
    pop rbx
    ret
Titan_ExportTrace ENDP

Titan_NF4_Decompress PROC
    push rbx
    push rsi
    push rdi
    
    mov rsi, rcx
    mov rdi, rdx
    mov r10, r8
    lea r9, [rel g_NF4Lookup]
    
nf4_loop:
    test r10, r10
    jz nf4_done
    
    movzx eax, byte ptr [rsi]
    mov edx, eax
    and eax, 0Fh
    shr edx, 4
    
    movss xmm0, real4 ptr [r9 + rax*4]
    movss real4 ptr [rdi], xmm0
    
    movss xmm1, real4 ptr [r9 + rdx*4]
    movss real4 ptr [rdi + 4], xmm1
    
    inc rsi
    add rdi, 8
    dec r10
    jnz nf4_loop
    
nf4_done:
    pop rdi
    pop rsi
    pop rbx
    ret
Titan_NF4_Decompress ENDP

Titan_AVX512_Copy PROC
    test r8, r8
    jz copy_exit
    
copy_loop:
    movdqa xmm0, [rcx]
    movdqa [rdx], xmm0
    movdqa xmm1, [rcx + 16]
    movdqa [rdx + 16], xmm1
    movdqa xmm2, [rcx + 32]
    movdqa [rdx + 32], xmm2
    movdqa xmm3, [rcx + 48]
    movdqa [rdx + 48], xmm3
    
    add rcx, 64
    add rdx, 64
    dec r8
    jnz copy_loop
    
copy_exit:
    ret
Titan_AVX512_Copy ENDP

Titan_AVX512_Xor PROC
    test r8, r8
    jz xor_exit
    
xor_loop:
    movdqa xmm0, [rcx]
    pxor xmm0, [rdx]
    movdqa [rdx], xmm0
    
    movdqa xmm1, [rcx + 16]
    pxor xmm1, [rdx + 16]
    movdqa [rdx + 16], xmm1
    
    movdqa xmm2, [rcx + 32]
    pxor xmm2, [rdx + 32]
    movdqa [rdx + 32], xmm2
    
    movdqa xmm3, [rcx + 48]
    pxor xmm3, [rdx + 48]
    movdqa [rdx + 48], xmm3
    
    add rcx, 64
    add rdx, 64
    dec r8
    jnz xor_loop
    
xor_exit:
    ret
Titan_AVX512_Xor ENDP

END main
