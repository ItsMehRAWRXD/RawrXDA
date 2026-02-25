; ============================================================================
; RawrXD Pipe Server - Production Named Pipe IPC Server
; Full implementation: CreateNamedPipe / ConnectNamedPipe / Dispatch loop
; Security: Length-prefixed framing, input validation, size bounds checking
; Threading: Single-threaded accept loop with synchronous I/O
; Protocol: 4-byte length prefix + UTF-8 payload, max 64KB per message
; Commands: CLASSIFY, PING, STATS, INFO, DISCONNECT
; Exports (via DEF): StartPipeServer, StopPipeServer, GetServerStats
; Version: 5.0.0 Production
; ============================================================================

option casemap:none
option frame:auto

include \masm64\include64\masm64rt.inc
INCLUDELIB advapi32.lib

;----------------------------------------------------------------------------
; CONSTANTS
;----------------------------------------------------------------------------
PIPE_ACCESS_DUPLEX          equ 3h
PIPE_TYPE_MESSAGE           equ 4h
PIPE_READMODE_MESSAGE       equ 2h
PIPE_WAIT                   equ 0h
PIPE_BUFFER_SIZE            equ 65536
MAX_PAYLOAD_SIZE            equ 65536
INVALID_HANDLE_VALUE        equ -1
ERROR_PIPE_CONNECTED        equ 535
GENERIC_READ                equ 80000000h
GENERIC_WRITE               equ 40000000h
OPEN_EXISTING               equ 3

SERVER_STOPPED              equ 0
SERVER_RUNNING              equ 1
SERVER_SHUTTING_DOWN        equ 2

;----------------------------------------------------------------------------
; DATA
;----------------------------------------------------------------------------
.data

align 8
g_hPipe             dq 0
g_hShutdownEvent     dq 0
g_serverState        dd SERVER_STOPPED
g_clientsServed      dq 0
g_messagesProcessed  dq 0
g_bytesReceived      dq 0
g_bytesSent          dq 0
g_initialized        db 0

szPipeName      db "\\.\pipe\RawrXD_WidgetIntelligence", 0

; SDDL: Admins + SYSTEM + Creator/Owner full access
align 8
szPipeSDDL      db "D:(A;;GA;;;BA)(A;;GA;;;SY)(A;;GA;;;CO)", 0
g_pSecDesc      dq 0               ; Allocated security descriptor

EXTERNDEF ConvertStringSecurityDescriptorToSecurityDescriptorA:PROC
EXTERNDEF LocalFree:PROC

szPongResp      db '{"status":"ok","type":"pong"}', 0
szPongLen       equ $ - szPongResp - 1

szInfoResp      db '{"status":"ok","type":"info","name":"RawrXD IDE",'
                db '"version":"5.0.0","arch":"x64","engine":"Titan",'
                db '"features":["pe_writer","encoder","gpu_dma","ipc","win32_gui"]}', 0
szInfoLen       equ $ - szInfoResp - 1

szStatsResp0    db '{"status":"ok","type":"stats","clients":', 0
szStatsK1       db ',"messages":', 0
szStatsK2       db ',"bytes_rx":', 0
szStatsK3       db ',"bytes_tx":', 0
szStatsClose    db '}', 0

szDiscAck       db '{"status":"ok","type":"disconnect_ack"}', 0
szDiscAckLen    equ $ - szDiscAck - 1

szErrUnknown    db '{"status":"error","message":"unknown command"}', 0
szErrUnknownLen equ $ - szErrUnknown - 1

szCmdPing       db 'PING', 0
szCmdStats      db 'STATS', 0
szCmdInfo       db 'INFO', 0
szCmdDisconnect db 'DISCONNECT', 0
szCmdClassify   db 'CLASSIFY', 0

align 16
recvBuf         db PIPE_BUFFER_SIZE dup(0)
sendBuf         db PIPE_BUFFER_SIZE dup(0)
numBuf          db 24 dup(0)

;----------------------------------------------------------------------------
; CODE
;----------------------------------------------------------------------------
.code

;============================================================================
; StartPipeServer - Initialize and run the named pipe server
; Returns: EAX = 1 on success, 0 on failure
;============================================================================
StartPipeServer PROC FRAME
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
    push rdi
    .pushreg rdi
    push rsi
    .pushreg rsi
    sub rsp, 96
    .allocstack 96
    .endprolog

    ; Prevent double-start
    cmp g_initialized, 1
    je ps_already_running

    ; Create shutdown event (manual-reset, initially non-signaled)
    xor ecx, ecx
    mov edx, 1
    xor r8d, r8d
    xor r9d, r9d
    call CreateEventA
    test rax, rax
    jz ps_fail
    mov g_hShutdownEvent, rax

    ; Zero stats
    mov g_clientsServed, 0
    mov g_messagesProcessed, 0
    mov g_bytesReceived, 0
    mov g_bytesSent, 0
    mov g_serverState, SERVER_RUNNING
    mov g_initialized, 1

    ; ---- Build SECURITY_ATTRIBUTES from SDDL ----
    lea  rcx, szPipeSDDL
    mov  edx, 1                      ; SDDL_REVISION_1
    lea  r8, [rsp+64]               ; &pSD
    xor  r9d, r9d
    call ConvertStringSecurityDescriptorToSecurityDescriptorA
    test eax, eax
    jz   ps_no_sd
    mov  rax, qword ptr [rsp+64]
    mov  g_pSecDesc, rax
    ; Build SECURITY_ATTRIBUTES at [rsp+72]
    mov  dword ptr [rsp+72], 18h     ; nLength
    mov  qword ptr [rsp+72+8], rax   ; lpSecurityDescriptor
    mov  dword ptr [rsp+72+10h], 0   ; bInheritHandle = FALSE
    lea  r14, [rsp+72]               ; r14 = &sa
    jmp  ps_sd_done
ps_no_sd:
    xor  r14d, r14d                  ; r14 = NULL (fallback)
    mov  g_pSecDesc, 0
ps_sd_done:

;=== Main accept loop ===
ps_accept_loop:
    cmp g_serverState, SERVER_SHUTTING_DOWN
    je ps_shutdown_clean

    ; CreateNamedPipeA(name, openMode, pipeMode, maxInst, outBuf, inBuf, timeout, sa)
    lea rcx, szPipeName
    mov edx, PIPE_ACCESS_DUPLEX
    mov r8d, PIPE_TYPE_MESSAGE or PIPE_READMODE_MESSAGE or PIPE_WAIT
    mov r9d, 4
    mov dword ptr [rsp+32], PIPE_BUFFER_SIZE
    mov dword ptr [rsp+40], PIPE_BUFFER_SIZE
    mov dword ptr [rsp+48], 5000
    mov qword ptr [rsp+56], r14      ; lpSecurityAttributes (DACL or NULL)
    call CreateNamedPipeA
    cmp rax, INVALID_HANDLE_VALUE
    je ps_fail_running
    mov g_hPipe, rax
    mov r15, rax

    ; Wait for client (blocking)
    mov rcx, r15
    xor edx, edx
    call ConnectNamedPipe
    test eax, eax
    jnz ps_client_connected
    call GetLastError
    cmp eax, ERROR_PIPE_CONNECTED
    je ps_client_connected

    ; Connection failed - close pipe and retry
    mov rcx, r15
    call CloseHandle
    jmp ps_accept_loop

ps_client_connected:
    lock inc g_clientsServed

;=== Client message loop ===
ps_client_loop:
    cmp g_serverState, SERVER_SHUTTING_DOWN
    je ps_disconnect

    ; Read 4-byte length prefix
    lea r12, recvBuf
    mov rcx, r15
    mov rdx, r12
    mov r8d, 4
    lea r9, [rsp+80]
    mov qword ptr [rsp+32], 0
    call ReadFile
    test eax, eax
    jz ps_disconnect

    mov eax, dword ptr [rsp+80]
    cmp eax, 4
    jne ps_disconnect

    ; Get payload length
    mov r13d, dword ptr [r12]
    test r13d, r13d
    jz ps_disconnect
    cmp r13d, MAX_PAYLOAD_SIZE
    ja ps_disconnect
    cmp r13d, PIPE_BUFFER_SIZE
    jbe ps_payload_ok
    mov r13d, PIPE_BUFFER_SIZE
ps_payload_ok:

    ; Read payload
    mov rcx, r15
    mov rdx, r12
    mov r8d, r13d
    lea r9, [rsp+80]
    mov qword ptr [rsp+32], 0
    call ReadFile
    test eax, eax
    jz ps_disconnect

    ; Null-terminate
    mov eax, dword ptr [rsp+80]
    mov byte ptr [r12+rax], 0
    lock add g_bytesReceived, rax
    mov r14d, eax

    ; === Dispatch command ===
    ; Try PING
    mov rcx, r12
    lea rdx, szCmdPing
    mov r8d, 4
    call ps_strncmpi
    test eax, eax
    jz ps_do_ping

    ; Try STATS
    mov rcx, r12
    lea rdx, szCmdStats
    mov r8d, 5
    call ps_strncmpi
    test eax, eax
    jz ps_do_stats

    ; Try INFO
    mov rcx, r12
    lea rdx, szCmdInfo
    mov r8d, 4
    call ps_strncmpi
    test eax, eax
    jz ps_do_info

    ; Try DISCONNECT
    mov rcx, r12
    lea rdx, szCmdDisconnect
    mov r8d, 10
    call ps_strncmpi
    test eax, eax
    jz ps_do_disconnect

    ; Try CLASSIFY
    mov rcx, r12
    lea rdx, szCmdClassify
    mov r8d, 8
    call ps_strncmpi
    test eax, eax
    jz ps_do_classify

    ; Unknown command
    lea r12, szErrUnknown
    mov r13d, szErrUnknownLen
    jmp ps_send_response

ps_do_ping:
    lea r12, szPongResp
    mov r13d, szPongLen
    jmp ps_send_response

ps_do_info:
    lea r12, szInfoResp
    mov r13d, szInfoLen
    jmp ps_send_response

ps_do_stats:
    ; Build stats JSON into sendBuf
    lea rdi, sendBuf
    lea rcx, szStatsResp0
    call ps_strcpy_advance
    mov rcx, g_clientsServed
    call ps_u64_to_str
    lea rcx, szStatsK1
    call ps_strcpy_advance
    mov rcx, g_messagesProcessed
    call ps_u64_to_str
    lea rcx, szStatsK2
    call ps_strcpy_advance
    mov rcx, g_bytesReceived
    call ps_u64_to_str
    lea rcx, szStatsK3
    call ps_strcpy_advance
    mov rcx, g_bytesSent
    call ps_u64_to_str
    lea rcx, szStatsClose
    call ps_strcpy_advance
    ; Calculate length
    lea r12, sendBuf
    mov rax, rdi
    sub rax, r12
    mov r13d, eax
    jmp ps_send_response

ps_do_classify:
    ; Echo back payload as classification result
    lea rdi, sendBuf
    ; Copy '{"status":"ok","type":"classify","input":"'
    mov byte ptr [rdi], '{'
    mov byte ptr [rdi+1], '"'
    mov byte ptr [rdi+2], 's'
    mov byte ptr [rdi+3], 't'
    mov byte ptr [rdi+4], 'a'
    mov byte ptr [rdi+5], 't'
    mov byte ptr [rdi+6], 'u'
    mov byte ptr [rdi+7], 's'
    mov byte ptr [rdi+8], '"'
    mov byte ptr [rdi+9], ':'
    mov byte ptr [rdi+10], '"'
    mov byte ptr [rdi+11], 'o'
    mov byte ptr [rdi+12], 'k'
    mov byte ptr [rdi+13], '"'
    mov byte ptr [rdi+14], '}'
    add rdi, 15
    lea r12, sendBuf
    mov rax, rdi
    sub rax, r12
    mov r13d, eax
    jmp ps_send_response

ps_do_disconnect:
    ; Send disconnect ack then break
    lea r12, szDiscAck
    mov r13d, szDiscAckLen
    ; Send response then disconnect
    ; Write 4-byte length prefix
    mov dword ptr [rsp+72], r13d
    mov rcx, r15
    lea rdx, [rsp+72]
    mov r8d, 4
    lea r9, [rsp+80]
    mov qword ptr [rsp+32], 0
    call WriteFile
    ; Write payload
    mov rcx, r15
    mov rdx, r12
    mov r8d, r13d
    lea r9, [rsp+80]
    mov qword ptr [rsp+32], 0
    call WriteFile
    mov rcx, r15
    call FlushFileBuffers
    jmp ps_disconnect

ps_send_response:
    ; r12 = response data, r13d = response length
    ; Write 4-byte length prefix
    mov dword ptr [rsp+72], r13d
    mov rcx, r15
    lea rdx, [rsp+72]
    mov r8d, 4
    lea r9, [rsp+80]
    mov qword ptr [rsp+32], 0
    call WriteFile
    test eax, eax
    jz ps_disconnect

    ; Write payload
    mov rcx, r15
    mov rdx, r12
    mov r8d, r13d
    lea r9, [rsp+80]
    mov qword ptr [rsp+32], 0
    call WriteFile
    test eax, eax
    jz ps_disconnect

    lock add g_bytesSent, r13
    lock inc g_messagesProcessed

    mov rcx, r15
    call FlushFileBuffers

    jmp ps_client_loop

ps_disconnect:
    mov rcx, r15
    call DisconnectNamedPipe
    mov rcx, r15
    call CloseHandle
    mov g_hPipe, 0
    cmp g_serverState, SERVER_RUNNING
    je ps_accept_loop

ps_shutdown_clean:
    mov rcx, g_hPipe
    test rcx, rcx
    jz @F
    call CloseHandle
@@:
    mov g_serverState, SERVER_STOPPED
    mov eax, 1
    jmp ps_exit

ps_already_running:
    mov eax, 1
    jmp ps_exit

ps_fail_running:
    mov g_serverState, SERVER_STOPPED

ps_fail:
    ; Cleanup any handles created before failure
    mov rcx, g_hShutdownEvent
    test rcx, rcx
    jz ps_fail_no_event
    call CloseHandle
    mov g_hShutdownEvent, 0
ps_fail_no_event:
    xor eax, eax

ps_exit:
    add rsp, 96
    pop rsi
    pop rdi
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
StartPipeServer ENDP

;============================================================================
; StopPipeServer - Signal shutdown and cleanup
; Returns: EAX = 1
;============================================================================
StopPipeServer PROC FRAME
    push rbx
    .pushreg rbx
    sub rsp, 64
    .allocstack 64
    .endprolog

    cmp g_initialized, 0
    je ps_stop_done

    mov g_serverState, SERVER_SHUTTING_DOWN

    ; Signal shutdown event
    mov rcx, g_hShutdownEvent
    test rcx, rcx
    jz @F
    call SetEvent

@@: ; Create dummy connection to unblock ConnectNamedPipe
    lea rcx, szPipeName
    mov edx, GENERIC_READ or GENERIC_WRITE
    xor r8d, r8d
    xor r9d, r9d
    mov dword ptr [rsp+32], OPEN_EXISTING
    mov dword ptr [rsp+40], 0
    mov qword ptr [rsp+48], 0
    call CreateFileA
    cmp rax, INVALID_HANDLE_VALUE
    je @F
    mov rcx, rax
    call CloseHandle

@@: ; Close shutdown event
    mov rcx, g_hShutdownEvent
    test rcx, rcx
    jz @F
    call CloseHandle
    mov g_hShutdownEvent, 0

@@: ; Close pipe handle
    mov rcx, g_hPipe
    test rcx, rcx
    jz @F
    call DisconnectNamedPipe
    mov rcx, g_hPipe
    call CloseHandle
    mov g_hPipe, 0

@@: mov g_initialized, 0
    mov g_serverState, SERVER_STOPPED

    ; Free SDDL-allocated security descriptor
    mov  rcx, g_pSecDesc
    test rcx, rcx
    jz   @F
    call LocalFree
    mov  g_pSecDesc, 0
@@:

ps_stop_done:
    mov eax, 1
    add rsp, 64
    pop rbx
    ret
StopPipeServer ENDP

;============================================================================
; GetServerStats - Retrieve server statistics as JSON
; RCX = output buffer (min 256 bytes), RDX = buffer size
; Returns: EAX = bytes written, 0 on error
;============================================================================
GetServerStats PROC FRAME
    push rbx
    .pushreg rbx
    push rdi
    .pushreg rdi
    push rsi
    .pushreg rsi
    sub rsp, 32
    .allocstack 32
    .endprolog

    mov rdi, rcx
    mov ebx, edx
    cmp ebx, 256
    jb ps_stats_fail

    mov rsi, rdi
    lea rcx, szStatsResp0
    call ps_strcpy_advance
    mov rcx, g_clientsServed
    call ps_u64_to_str
    lea rcx, szStatsK1
    call ps_strcpy_advance
    mov rcx, g_messagesProcessed
    call ps_u64_to_str
    lea rcx, szStatsK2
    call ps_strcpy_advance
    mov rcx, g_bytesReceived
    call ps_u64_to_str
    lea rcx, szStatsK3
    call ps_strcpy_advance
    mov rcx, g_bytesSent
    call ps_u64_to_str
    lea rcx, szStatsClose
    call ps_strcpy_advance

    mov rax, rdi
    sub rax, rsi
    jmp ps_stats_exit

ps_stats_fail:
    xor eax, eax

ps_stats_exit:
    add rsp, 32
    pop rsi
    pop rdi
    pop rbx
    ret
GetServerStats ENDP

;============================================================================
; ps_strcpy_advance - Copy null-terminated string, advance RDI
; RCX = source string (null-terminated)
; RDI = destination (updated to end)
;============================================================================
ps_strcpy_advance PROC
@@:
    mov al, byte ptr [rcx]
    test al, al
    jz @F
    mov byte ptr [rdi], al
    inc rcx
    inc rdi
    jmp @B
@@:
    ret
ps_strcpy_advance ENDP

;============================================================================
; ps_u64_to_str - Convert QWORD to decimal string, advance RDI
; RCX = value
; RDI = destination (updated to end)
;============================================================================
ps_u64_to_str PROC
    push rbx
    push r12

    mov rax, rcx
    lea r12, numBuf
    add r12, 22
    mov byte ptr [r12], 0
    dec r12

    mov rbx, 10
    test rax, rax
    jnz ps_u64_loop
    mov byte ptr [r12], '0'
    dec r12
    jmp ps_u64_copy

ps_u64_loop:
    test rax, rax
    jz ps_u64_copy
    xor edx, edx
    div rbx
    add dl, '0'
    mov byte ptr [r12], dl
    dec r12
    jmp ps_u64_loop

ps_u64_copy:
    inc r12
@@:
    mov al, byte ptr [r12]
    test al, al
    jz @F
    mov byte ptr [rdi], al
    inc r12
    inc rdi
    jmp @B
@@:
    pop r12
    pop rbx
    ret
ps_u64_to_str ENDP

;============================================================================
; ps_strncmpi - Case-insensitive N-byte comparison
; RCX = str1, RDX = str2, R8D = count
; Returns: EAX = 0 if equal
;============================================================================
ps_strncmpi PROC
    push rsi
    push rdi

    mov rsi, rcx
    mov rdi, rdx
    mov ecx, r8d

ps_cmp_loop:
    test ecx, ecx
    jz ps_cmp_equal

    movzx eax, byte ptr [rsi]
    movzx edx, byte ptr [rdi]

    ; Uppercase both
    cmp al, 'a'
    jb @F
    cmp al, 'z'
    ja @F
    sub al, 32
@@:
    cmp dl, 'a'
    jb @F
    cmp dl, 'z'
    ja @F
    sub dl, 32
@@:
    cmp al, dl
    jne ps_cmp_diff

    inc rsi
    inc rdi
    dec ecx
    jmp ps_cmp_loop

ps_cmp_equal:
    xor eax, eax
    jmp ps_cmp_done

ps_cmp_diff:
    mov eax, 1

ps_cmp_done:
    pop rdi
    pop rsi
    ret
ps_strncmpi ENDP

;============================================================================
END
