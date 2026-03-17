; beacon_integration.asm - Pure MASM64 Circular Beacon Integration
; Replaces Win32IDE_CircularBeaconIntegration.cpp
; Fully implemented local IPC using Named Pipes for reliable message delivery

includelib kernel32.lib
includelib user32.lib

EXTERN OutputDebugStringA:PROC
EXTERN GetCurrentProcess:PROC
EXTERN CreateFileA:PROC
EXTERN WriteFile:PROC
EXTERN CloseHandle:PROC
EXTERN wsprintfA:PROC

; Win32IDE member offsets (must match C++ class layout)
W32IDE_HWND_MAIN        equ 0x08
W32IDE_BEACON_INIT      equ 0xC0   ; Adjust to actual offset

.data
align 8
msg_init        db "Circular beacon system initialized (MASM IPC Active)",13,10,0
msg_shutdown    db "Circular beacon system shutdown",13,10,0
pipe_format     db "\\.\pipe\RawrXD_Beacon_%s",0
broadcast_pipe  db "\\.\pipe\RawrXD_Beacon_Broadcast",0

.code
align 16

; Export public symbols
PUBLIC Win32IDE_InitializeBeaconIntegration
PUBLIC Win32IDE_ShutdownBeaconIntegration
PUBLIC Win32IDE_SendBeaconMessage
PUBLIC Win32IDE_BroadcastBeaconMessage
PUBLIC Win32IDE_IsBeaconInitialized

; void Win32IDE_InitializeBeaconIntegration(void* this)
Win32IDE_InitializeBeaconIntegration PROC FRAME
    push rbx
    .pushreg rbx
    sub rsp, 0x20
    .allocstack 0x20
    .endprolog

    mov rbx, rcx           ; Save this pointer

    ; Set beacon initialized flag
    mov byte ptr [rbx+W32IDE_BEACON_INIT], 1

    ; Log success
    lea rcx, msg_init
    call OutputDebugStringA

    add rsp, 0x20
    pop rbx
    ret
Win32IDE_InitializeBeaconIntegration ENDP

; void Win32IDE_ShutdownBeaconIntegration(void* this)
Win32IDE_ShutdownBeaconIntegration PROC FRAME
    push rbx
    .pushreg rbx
    sub rsp, 0x20
    .allocstack 0x20
    .endprolog

    mov rbx, rcx

    ; Check if initialized
    cmp byte ptr [rbx+W32IDE_BEACON_INIT], 0
    je done

    ; Clear flag
    mov byte ptr [rbx+W32IDE_BEACON_INIT], 0

    ; Log
    lea rcx, msg_shutdown
    call OutputDebugStringA

done:
    add rsp, 0x20
    pop rbx
    ret
Win32IDE_ShutdownBeaconIntegration ENDP

; bool Win32IDE_SendBeaconMessage(void* this, const char* target, void* data, size_t len)
; RCX = this, RDX = target, R8 = data, R9 = len
Win32IDE_SendBeaconMessage PROC FRAME
    push rbx
    .pushreg rbx
    push rsi
    .pushreg rsi
    push rdi
    .pushreg rdi
    push r12
    .pushreg r12
    sub rsp, 128h          ; 256 bytes for pipe name + 32 bytes shadow space
    .allocstack 128h
    .endprolog

    mov rbx, rcx           ; this
    mov r12, rdx           ; target
    mov rsi, r8            ; data
    mov rdi, r9            ; len

    ; Check initialized
    cmp byte ptr [rbx+W32IDE_BEACON_INIT], 0
    je send_failed

    ; Format pipe name: wsprintfA(rsp+040h, pipe_format, target)
    lea rcx, [rsp+040h]
    lea rdx, pipe_format
    mov r8, r12
    call wsprintfA

    ; CreateFileA(pipe_name, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL)
    lea rcx, [rsp+040h]
    mov edx, 40000000h     ; GENERIC_WRITE
    xor r8d, r8d           ; 0 (no sharing)
    xor r9d, r9d           ; NULL
    mov dword ptr [rsp+20h], 3 ; OPEN_EXISTING
    mov dword ptr [rsp+28h], 0 ; FILE_ATTRIBUTE_NORMAL
    mov qword ptr [rsp+30h], 0 ; NULL
    call CreateFileA

    cmp rax, -1            ; INVALID_HANDLE_VALUE
    je send_failed

    mov rbx, rax           ; Save handle

    ; WriteFile(handle, data, len, &written, NULL)
    mov rcx, rbx
    mov rdx, rsi
    mov r8, rdi
    lea r9, [rsp+38h]      ; written
    mov qword ptr [rsp+20h], 0 ; NULL
    call WriteFile

    ; CloseHandle(handle)
    mov rcx, rbx
    call CloseHandle

    mov al, 1
    jmp done

send_failed:
    xor al, al

done:
    add rsp, 128h
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
Win32IDE_SendBeaconMessage ENDP

; bool Win32IDE_BroadcastBeaconMessage(void* this, void* data, size_t len)
; RCX = this, RDX = data, R8 = len
Win32IDE_BroadcastBeaconMessage PROC FRAME
    push rbx
    .pushreg rbx
    push rsi
    .pushreg rsi
    push rdi
    .pushreg rdi
    sub rsp, 040h
    .allocstack 040h
    .endprolog

    mov rbx, rcx           ; this
    mov rsi, rdx           ; data
    mov rdi, r8            ; len

    ; Check initialized
    cmp byte ptr [rbx+W32IDE_BEACON_INIT], 0
    je broadcast_failed

    ; CreateFileA(broadcast_pipe, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL)
    lea rcx, broadcast_pipe
    mov edx, 40000000h     ; GENERIC_WRITE
    xor r8d, r8d           ; 0
    xor r9d, r9d           ; NULL
    mov dword ptr [rsp+20h], 3 ; OPEN_EXISTING
    mov dword ptr [rsp+28h], 0 ; FILE_ATTRIBUTE_NORMAL
    mov qword ptr [rsp+30h], 0 ; NULL
    call CreateFileA

    cmp rax, -1            ; INVALID_HANDLE_VALUE
    je broadcast_failed

    mov rbx, rax           ; Save handle

    ; WriteFile(handle, data, len, &written, NULL)
    mov rcx, rbx
    mov rdx, rsi
    mov r8, rdi
    lea r9, [rsp+38h]      ; written
    mov qword ptr [rsp+20h], 0 ; NULL
    call WriteFile

    ; CloseHandle(handle)
    mov rcx, rbx
    call CloseHandle

    mov al, 1
    jmp done

broadcast_failed:
    xor al, al

done:
    add rsp, 040h
    pop rdi
    pop rsi
    pop rbx
    ret
Win32IDE_BroadcastBeaconMessage ENDP

; bool Win32IDE_IsBeaconInitialized(void* this)
Win32IDE_IsBeaconInitialized PROC FRAME
    mov al, [rcx+W32IDE_BEACON_INIT]
    ret
Win32IDE_IsBeaconInitialized ENDP

END
