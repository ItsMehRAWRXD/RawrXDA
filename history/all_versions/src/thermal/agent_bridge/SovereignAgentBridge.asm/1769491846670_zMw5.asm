; ============================================================================
; Sovereign Agentic Bridge (MASM x64)
; Provides Named Pipe interface for AI Agents to steer the Silicon
; ============================================================================
option casemap:none

include windows.inc
includelib kernel32.lib

.data
    ; Pipe Configuration
    szPipeName     db "\\.\pipe\SovereignThermalAgent", 0
    szGreeting     db '{"status":"connected","engine":"Sovereign_v1"}', 0
    szStatusHeader db '{"drive_id":%d,"temp":%d,"wear":%d,"status":"%s"}', 0
    
    ; Command Strings
    cmdGetStatus   db "GET_STATUS", 0
    cmdSetEvict    db "SET_EVICT", 0 ; Followed by drive ID

    ; Buffer space
    pipeBuf        db 1024 dup(0)
    outBuf         db 1024 dup(0)

.code

; ----------------------------------------------------------------------------
; AgentBridgeEntry: The main listener loop
; ----------------------------------------------------------------------------
AgentBridgeEntry PROC FRAME
    sub rsp, 40h ; Shadow space

_listen_loop:
    ; Create the Named Pipe
    ; CreateNamedPipeA(name, access, type, max_inst, out_buf, in_buf, timeout, attr)
    mov rcx, offset szPipeName
    mov edx, PIPE_ACCESS_DUPLEX
    mov r8d, PIPE_TYPE_MESSAGE or PIPE_READMODE_MESSAGE or PIPE_WAIT
    mov r9d, 1                  ; Max instances
    mov dword ptr [rsp+20h], 1024 ; Out buffer
    mov dword ptr [rsp+28h], 1024 ; In buffer
    mov dword ptr [rsp+30h], 0    ; Default timeout
    mov qword ptr [rsp+38h], 0    ; Default security
    call CreateNamedPipeA
    
    mov rbx, rax                ; Save pipe handle
    cmp rax, INVALID_HANDLE_VALUE
    je _error_restart

    ; Wait for Agent connection
    mov rcx, rbx
    xor rdx, rdx
    call ConnectNamedPipe
    
    ; Process Agent Command
    lea rdx, pipeBuf
    mov r8d, 1024
    lea r9, [rsp+48h] ; bytesRead
    mov rcx, rbx
    mov qword ptr [rsp+20h], 0
    call ReadFile

    ; --- Command Processing Logic ---
    ; Simple string compare for "GET_STATUS"
    lea rcx, pipeBuf
    lea rdx, cmdGetStatus
    call _strcmp
    test eax, eax
    jz _handle_status

    jmp _disconnect

_handle_status:
    ; 1. Pull data from Governor MMF (Shared pointers)
    ; 2. Format JSON string using user-mode-safe logic
    ; 3. Write response back to Pipe
    lea rdx, szGreeting
    mov r8d, sizeof szGreeting
    mov rcx, rbx
    lea r9, [rsp+48h]
    mov qword ptr [rsp+20h], 0
    call WriteFile

_disconnect:
    mov rcx, rbx
    call FlushFileBuffers
    mov rcx, rbx
    call DisconnectNamedPipe
    mov rcx, rbx
    call CloseHandle

    jmp _listen_loop

_error_restart:
    mov ecx, 1000
    call Sleep
    jmp _listen_loop

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
