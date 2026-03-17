; TITAN x64 MASM PRODUCTION BUILD
; Compile: ml64 /c /Fo titan_clean.obj /W0 titan_clean.asm
; Link: link titan_clean.obj kernel32.lib /SUBSYSTEM:CONSOLE /OUT:titan_clean.exe

.DATA

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
g_JITOldProtect             DD 0
g_LoadOldProtect            DD 0
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
EXTERN GetLastError:PROC
EXTERN VirtualProtect:PROC

GENERIC_READ                EQU 80000000h
GENERIC_WRITE               EQU 40000000h
FILE_SHARE_READ             EQU 00000001h
CREATE_ALWAYS               EQU 00000002h
OPEN_EXISTING               EQU 00000003h
FILE_ATTRIBUTE_NORMAL       EQU 00000080h
INVALID_HANDLE_VALUE        EQU -1
PAGE_EXECUTE_READWRITE      EQU 00000040h
KERNEL_HEADER_SIZE          EQU 12
TRACE_HEADER_SIZE           EQU 16

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
    sub rsp, 32
    call Titan_InitTracing
    mov qword ptr g_EditorLength, 0
    mov qword ptr g_EditorCursor, 0
    mov qword ptr g_EditorLineCount, 1
    mov qword ptr g_EditorModified, 0
    mov qword ptr g_SymbolCount, 0
    mov qword ptr g_TokenCount, 0
    mov qword ptr g_ASTCount, 0
    mov qword ptr g_TraceState, 0
    mov qword ptr g_TraceIndex, 0
    mov qword ptr g_TraceEnabled, 1
    mov qword ptr g_TraceBPCount, 0
    xor eax, eax
    add rsp, 32
    pop rbx
    ret
Titan_Initialize ENDP

Titan_MainLoop PROC
    push rbx
    sub rsp, 32
main_loop:
    cmp qword ptr g_EngineRunning, 0
    je main_loop_exit
    call Titan_ExecutionCycle
    test eax, eax
    jz main_loop
    mov qword ptr g_EngineRunning, 0
    jmp main_loop
main_loop_exit:
    add rsp, 32
    pop rbx
    ret
Titan_MainLoop ENDP

Titan_ExecutionCycle PROC
    push rbx
    push rsi
    push rdi
    sub rsp, 32
    call Titan_InitTracing
    lea rbx, g_JITBuffer
    mov rcx, rbx
    mov rdx, 0
    mov r8, 0
    call Emit_X64_Xor_Reg_Reg
    add rbx, rax
    mov rcx, 1
    mov rdx, 31h
    mov r8, 0
    mov r9, 0
    call Titan_RecordTraceEvent
    mov rcx, rbx
    mov rdx, 0
    mov r8, 42h
    call Emit_X64_Add_Reg_Imm32
    add rbx, rax
    mov rcx, 1
    mov rdx, 81h
    mov r8, 0
    mov r9, 42h
    call Titan_RecordTraceEvent
    mov rcx, rbx
    call Emit_X64_Ret
    add rbx, rax
    lea rcx, g_JITBuffer
    mov rdx, 512
    mov r8d, PAGE_EXECUTE_READWRITE
    lea r9, g_JITOldProtect
    sub rsp, 32
    call VirtualProtect
    add rsp, 32
    test eax, eax
    jz exec_fail
    lea rax, g_JITBuffer
    call rax
    cmp eax, 42h
    jne exec_result_fail
    lea rcx, g_TraceFile
    call Titan_ExportTrace
    lea rcx, g_JITBuffer
    mov rdx, rbx
    sub rdx, rcx
    mov rsi, rdx
    lea r8, g_ProjectFile
    call Titan_SaveKernel
    test eax, eax
    jnz exec_fail

    ; Reload persisted kernel and execute it as validation
    lea rcx, g_ProjectFile
    lea rdx, g_LoadBuffer
    call Titan_LoadKernel
    test rax, rax
    jz exec_fail
    cmp rax, rsi
    jne exec_fail

    lea rcx, g_LoadBuffer
    mov rdx, 512
    mov r8d, PAGE_EXECUTE_READWRITE
    lea r9, g_LoadOldProtect
    call VirtualProtect
    test eax, eax
    jz exec_fail

    lea rax, g_LoadBuffer
    call rax
    cmp eax, 42h
    jne reload_result_fail
    mov qword ptr g_ExecutionContext, rax

    mov qword ptr g_EngineRunning, 0
    xor eax, eax
    add rsp, 32
    pop rdi
    pop rsi
    pop rbx
    ret
exec_result_fail:
    mov dword ptr g_EngineError, 0E0010001h
    mov qword ptr g_EngineRunning, 0
    mov eax, 1
    add rsp, 32
    pop rdi
    pop rsi
    pop rbx
    ret
reload_result_fail:
    mov dword ptr g_EngineError, 0E0010002h
    mov qword ptr g_EngineRunning, 0
    mov eax, 1
    add rsp, 32
    pop rdi
    pop rsi
    pop rbx
    ret
exec_fail:
    call GetLastError
    mov dword ptr g_EngineError, eax
    mov qword ptr g_EngineRunning, 0
    mov eax, 1
    add rsp, 32
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
    ; rcx = filename, rdx = desiredAccess, r8 = creationDisposition
    ; returns HANDLE or INVALID_HANDLE_VALUE
    sub rsp, 56
    mov r10, r8
    mov r8d, FILE_SHARE_READ
    xor r9d, r9d
    mov qword ptr [rsp + 32], r10
    mov qword ptr [rsp + 40], FILE_ATTRIBUTE_NORMAL
    mov qword ptr [rsp + 48], 0
    call CreateFileA
    cmp rax, INVALID_HANDLE_VALUE
    jne create_ok
    call GetLastError
    mov dword ptr g_EngineError, eax
create_ok:
    add rsp, 56
    ret
Titan_CreateFile ENDP

Titan_WriteFile PROC
    ; rcx = handle, rdx = buffer, r8 = size
    ; returns bytes written (0 on failure)
    push rbx
    push rsi
    push rdi
    sub rsp, 48
    mov rsi, rcx
    mov rdi, rdx
    mov r11, r8
    xor rbx, rbx
write_loop:
    test r11, r11
    jz write_done
    mov r10d, 0FFFFFFFFh
    cmp r11, r10
    ja write_chunk_ready
    mov r10, r11
write_chunk_ready:
    mov rcx, rsi
    mov rdx, rdi
    mov r8, r10
    mov dword ptr [rsp + 40], 0
    lea r9, [rsp + 40]
    mov qword ptr [rsp + 32], 0
    call WriteFile
    test eax, eax
    jz write_fail
    mov eax, dword ptr [rsp + 40]
    test eax, eax
    jz write_fail
    add rbx, rax
    add rdi, rax
    sub r11, rax
    jmp write_loop
write_done:
    mov rax, rbx
    add rsp, 48
    pop rdi
    pop rsi
    pop rbx
    ret
write_fail:
    call GetLastError
    mov dword ptr g_EngineError, eax
    xor eax, eax
    add rsp, 48
    pop rdi
    pop rsi
    pop rbx
    ret
Titan_WriteFile ENDP

Titan_ReadFile PROC
    ; rcx = handle, rdx = buffer, r8 = size
    ; returns bytes read (0 on failure)
    push rbx
    push rsi
    push rdi
    sub rsp, 48
    mov rsi, rcx
    mov rdi, rdx
    mov r11, r8
    xor rbx, rbx
read_loop:
    test r11, r11
    jz read_done
    mov r10d, 0FFFFFFFFh
    cmp r11, r10
    ja read_chunk_ready
    mov r10, r11
read_chunk_ready:
    mov rcx, rsi
    mov rdx, rdi
    mov r8, r10
    mov dword ptr [rsp + 40], 0
    lea r9, [rsp + 40]
    mov qword ptr [rsp + 32], 0
    call ReadFile
    test eax, eax
    jz read_fail
    mov eax, dword ptr [rsp + 40]
    test eax, eax
    jz read_done
    add rbx, rax
    add rdi, rax
    sub r11, rax
    jmp read_loop
read_done:
    mov rax, rbx
    add rsp, 48
    pop rdi
    pop rsi
    pop rbx
    ret
read_fail:
    call GetLastError
    mov dword ptr g_EngineError, eax
    xor eax, eax
    add rsp, 48
    pop rdi
    pop rsi
    pop rbx
    ret
Titan_ReadFile ENDP

Titan_CloseFile PROC
    ; rcx = handle
    ; returns 0 on success, 1 on failure
    sub rsp, 40
    call CloseHandle
    test eax, eax
    jz close_fail
    xor eax, eax
    add rsp, 40
    ret
close_fail:
    call GetLastError
    mov dword ptr g_EngineError, eax
    mov eax, 1
    add rsp, 40
    ret
Titan_CloseFile ENDP

Titan_SaveKernel PROC
    push rbx
    push rsi
    push rdi
    sub rsp, 32
    mov rsi, rcx
    mov rbx, rdx
    mov rdi, r8
    mov rcx, rdi
    mov rdx, GENERIC_WRITE
    mov r8, CREATE_ALWAYS
    call Titan_CreateFile
    mov rdi, rax
    cmp rdi, INVALID_HANDLE_VALUE
    je save_fail
    mov dword ptr [rsp], 5854494Ah
    mov dword ptr [rsp + 4], 1
    mov dword ptr [rsp + 8], ebx
    mov rcx, rdi
    lea rdx, [rsp]
    mov r8, KERNEL_HEADER_SIZE
    call Titan_WriteFile
    cmp eax, KERNEL_HEADER_SIZE
    jne save_fail_close
    mov rcx, rdi
    mov rdx, rsi
    mov r8, rbx
    call Titan_WriteFile
    cmp rax, rbx
    jne save_fail_close
    mov rcx, rdi
    call Titan_CloseFile
    xor eax, eax
    add rsp, 32
    pop rdi
    pop rsi
    pop rbx
    ret
save_fail_close:
    mov rcx, rdi
    call Titan_CloseFile
save_fail:
    mov eax, 1
    add rsp, 32
    pop rdi
    pop rsi
    pop rbx
    ret
Titan_SaveKernel ENDP

Titan_LoadKernel PROC
    push rbx
    push rsi
    push rdi
    sub rsp, 32
    mov rsi, rcx
    mov rdi, rdx
    mov rcx, rsi
    mov rdx, GENERIC_READ
    mov r8, OPEN_EXISTING
    call Titan_CreateFile
    mov rbx, rax
    cmp rbx, INVALID_HANDLE_VALUE
    je load_fail
    mov rcx, rbx
    lea rdx, [rsp]
    mov r8, KERNEL_HEADER_SIZE
    call Titan_ReadFile
    cmp eax, KERNEL_HEADER_SIZE
    jne load_fail_close
    mov eax, dword ptr [rsp]
    cmp eax, 5854494Ah
    jne load_fail_close
    mov eax, dword ptr [rsp + 8]
    mov dword ptr [rsp + 16], eax
    mov r8d, eax
    mov rcx, rbx
    mov rdx, rdi
    call Titan_ReadFile
    mov r10, rax
    mov eax, dword ptr [rsp + 16]
    cmp r10d, eax
    jne load_fail_close
    mov rcx, rbx
    call Titan_CloseFile
    mov rax, r10
    jmp load_done
load_fail_close:
    mov rcx, rbx
    call Titan_CloseFile
load_fail:
    xor eax, eax
load_done:
    add rsp, 32
    pop rdi
    pop rsi
    pop rbx
    ret
Titan_LoadKernel ENDP

Titan_InitTracing PROC
    mov qword ptr g_TraceState, 0
    mov qword ptr g_TraceIndex, 0
    mov qword ptr g_TraceEnabled, 1
    mov qword ptr g_TraceBPCount, 0
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
    sub rsp, 40
    cmp qword ptr g_TraceEnabled, 0
    je trace_disabled
    mov rbx, g_TraceState
    mov rsi, 512
    cmp rbx, rsi
    jge trace_full
    mov rsi, 128
    imul rsi, rbx
    lea rax, g_TraceBuffer
    add rsi, rax
    mov [rsi + 88], edx
    mov [rsi + 92], ecx
    mov [rsi + 104], r8
    mov [rsi + 112], r9
    mov rcx, rsi
    call Titan_CaptureRegisterState
    inc qword ptr g_TraceState
    inc qword ptr g_TraceIndex
    xor eax, eax
    jmp trace_exit
trace_full:
    mov eax, 1
    jmp trace_exit
trace_disabled:
    xor eax, eax
trace_exit:
    add rsp, 40
    pop rsi
    pop rbx
    ret
Titan_RecordTraceEvent ENDP

Titan_ExportTrace PROC
    push rbx
    push rsi
    sub rsp, 56
    mov rdx, GENERIC_WRITE
    mov r8, CREATE_ALWAYS
    call Titan_CreateFile
    mov rbx, rax
    cmp rbx, INVALID_HANDLE_VALUE
    je trace_export_fail
    mov dword ptr [rsp], 54524143h
    mov dword ptr [rsp + 4], 1
    mov eax, dword ptr g_TraceState
    mov dword ptr [rsp + 8], eax
    mov dword ptr [rsp + 12], 0
    mov rcx, rbx
    lea rdx, [rsp]
    mov r8, TRACE_HEADER_SIZE
    call Titan_WriteFile
    cmp eax, TRACE_HEADER_SIZE
    jne trace_export_fail_close
    mov eax, dword ptr g_TraceState
    imul rax, 128
    mov rsi, rax
    mov rcx, rbx
    lea rdx, g_TraceBuffer
    mov r8, rax
    call Titan_WriteFile
    cmp rax, rsi
    jne trace_export_fail_close
    mov rcx, rbx
    call Titan_CloseFile
    xor eax, eax
    add rsp, 56
    pop rsi
    pop rbx
    ret
trace_export_fail_close:
    mov rcx, rbx
    call Titan_CloseFile
trace_export_fail:
    mov eax, 1
    add rsp, 56
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
    lea r9, g_NF4Lookup
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
    movdqu xmm0, [rcx]
    movdqu [rdx], xmm0
    movdqu xmm1, [rcx + 16]
    movdqu [rdx + 16], xmm1
    movdqu xmm2, [rcx + 32]
    movdqu [rdx + 32], xmm2
    movdqu xmm3, [rcx + 48]
    movdqu [rdx + 48], xmm3
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
    movdqu xmm0, [rcx]
    pxor xmm0, [rdx]
    movdqu [rdx], xmm0
    movdqu xmm1, [rcx + 16]
    pxor xmm1, [rdx + 16]
    movdqu [rdx + 16], xmm1
    movdqu xmm2, [rcx + 32]
    pxor xmm2, [rdx + 32]
    movdqu [rdx + 32], xmm2
    movdqu xmm3, [rcx + 48]
    pxor xmm3, [rdx + 48]
    movdqu [rdx + 48], xmm3
    add rcx, 64
    add rdx, 64
    dec r8
    jnz xor_loop
xor_exit:
    ret
Titan_AVX512_Xor ENDP

END
