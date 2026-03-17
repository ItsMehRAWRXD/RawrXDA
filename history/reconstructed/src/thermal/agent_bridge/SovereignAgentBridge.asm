; ============================================================================
; Sovereign Agentic Bridge (MASM x64)
; Provides Named Pipe interface for AI Agents to steer the Silicon
; ============================================================================
option casemap:none

; ─── PUBLIC Exports ──────────────────────────────────────────────────────────
PUBLIC AgentBridgeEntry

; include windows.inc ; Removed dependency
; includelib kernel32.lib ; Handled by linker

extern CreateNamedPipeA : PROC
extern ConnectNamedPipe : PROC
extern ReadFile : PROC
extern WriteFile : PROC
extern FlushFileBuffers : PROC
extern DisconnectNamedPipe : PROC
extern CloseHandle : PROC
extern Sleep : PROC
extern OpenFileMappingA : PROC
extern MapViewOfFile : PROC
extern wsprintfA : PROC

.const
    PIPE_ACCESS_DUPLEX equ 3
    PIPE_TYPE_MESSAGE equ 4
    PIPE_READMODE_MESSAGE equ 2
    PIPE_WAIT equ 0
    INVALID_HANDLE_VALUE equ -1
    FILE_MAP_READ equ 4

.data
    ; Pipe Configuration
    szPipeName     db "\\.\pipe\SovereignThermalAgent", 0
    szGreeting     db '{"status":"connected","engine":"Sovereign_v1"}', 0
    szJsonFmt      db '{"status":"online","t0":%d,"t1":%d,"t2":%d,"t3":%d,"t4":%d}', 0
    szMMFName      db "Global\\SOVEREIGN_NVME_TEMPS", 0

    cmdGetStatus   db "GET_STATUS", 0
    cmdSetEvict    db "SET_EVICT", 0

    pipeBuf        db 1024 dup(0)
    outBuf         db 1024 dup(0)
    
    hMMF           dq 0
    pView          dq 0

.code

AgentBridgeEntry PROC FRAME
    sub rsp, 68h ; Increase shadow space for wsprintf
    .allocstack 68h
    .endprolog

    ; Initialize MMF Access
    mov rcx, FILE_MAP_READ
    xor rdx, rdx
    lea r8, szMMFName
    call OpenFileMappingA
    test rax, rax
    jz _loop_entry
    mov hMMF, rax

    mov rcx, rax
    mov rdx, FILE_MAP_READ
    xor r8, r8
    xor r9, r9
    mov qword ptr [rsp+20h], 1024
    call MapViewOfFile
    mov pView, rax

_loop_entry:
_listen_loop:
    mov rcx, offset szPipeName
    mov edx, PIPE_ACCESS_DUPLEX
    mov r8d, PIPE_TYPE_MESSAGE or PIPE_READMODE_MESSAGE or PIPE_WAIT
    mov r9d, 1
    mov dword ptr [rsp+20h], 1024
    mov dword ptr [rsp+28h], 1024
    mov dword ptr [rsp+30h], 0
    mov qword ptr [rsp+38h], 0
    call CreateNamedPipeA
    
    mov rdi, rax ; Use RDI for Pipe Handle (non-volatile)
    cmp rax, INVALID_HANDLE_VALUE
    je _error_restart

    mov rcx, rdi
    xor rdx, rdx
    call ConnectNamedPipe
    
    lea rdx, pipeBuf
    mov r8d, 1024
    lea r9, [rsp+58h]
    mov rcx, rdi
    mov qword ptr [rsp+20h], 0
    call ReadFile

    lea rcx, pipeBuf
    lea rdx, cmdGetStatus
    call _strcmp
    test eax, eax
    jz _handle_status

    jmp _disconnect

_handle_status:
    mov rax, pView
    test rax, rax
    jz _fallback_greet

    mov r10, rax
    mov r8d, [r10 + 16 + 0*4]  ; T0
    mov r9d, [r10 + 16 + 1*4]  ; T1
    mov eax, [r10 + 16 + 2*4]  ; T2
    mov [rsp+20h], rax
    mov eax, [r10 + 16 + 3*4]  ; T3
    mov [rsp+28h], rax
    mov eax, [r10 + 16 + 4*4]  ; T4
    mov [rsp+30h], rax
    
    lea rcx, outBuf
    lea rdx, szJsonFmt
    call wsprintfA
    mov ebx, eax ; Length
    
    mov rcx, rdi
    lea rdx, outBuf
    mov r8d, ebx
    lea r9, [rsp+58h]
    mov qword ptr [rsp+20h], 0
    call WriteFile
    jmp _disconnect

_fallback_greet:
    lea rdx, szGreeting
    mov r8d, sizeof szGreeting
    mov rcx, rdi
    lea r9, [rsp+58h]
    mov qword ptr [rsp+20h], 0
    call WriteFile

_disconnect:
    mov rcx, rdi
    call FlushFileBuffers
    mov rcx, rdi
    call DisconnectNamedPipe
    mov rcx, rdi
    call CloseHandle

    jmp _listen_loop

_error_restart:
    mov ecx, 1000
    call Sleep
    jmp _listen_loop
    ret
AgentBridgeEntry ENDP


; Simple x64 strcmp
_strcmp PROC
    xor rax, rax
_loop:
    mov al, [rcx]
    cmp al, [rdx]
    jne _diff
    test al, al
    jz _equal
    inc rcx
    inc rdx
    jmp _loop
_diff:
    mov eax, 1
    ret
_equal:
    xor eax, eax
    ret
_strcmp ENDP

END

