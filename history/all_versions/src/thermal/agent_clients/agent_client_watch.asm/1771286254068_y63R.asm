; agent_client_watch.asm
; Loops and watches the Sovereign pipe for real-time status reports

option casemap:none
extern CreateFileA : PROC
extern WriteFile : PROC
extern ReadFile : PROC
extern CloseHandle : PROC
extern Sleep : PROC
extern GetStdHandle : PROC
extern WriteConsoleA : PROC
extern ExitProcess : PROC

.const
    GENERIC_READ  equ 80000000h
    GENERIC_WRITE equ 40000000h
    OPEN_EXISTING equ 3
    STD_OUTPUT_HANDLE equ -11
    szPipeName     db "\\.\pipe\SovereignThermalAgent", 0
    szGetStatus    db "GET_STATUS", 0

.data
    hPipe          dq 0
    bufRequest     db 64 dup(0)
    bufResponse    db 1024 dup(0)
    bytesRead      dq 0
    bytesWritten   dq 0

.code
main PROC FRAME
    sub rsp, 58h ; 88 bytes. 88+8 = 96 (aligned).
    .allocstack 58h
    .endprolog

_loop_connect:
    ; CreateFileA
    mov rcx, offset szPipeName
    mov edx, GENERIC_READ or GENERIC_WRITE
    mov r8d, 0
    mov r9d, 0
    mov dword ptr [rsp+20h], OPEN_EXISTING
    mov dword ptr [rsp+28h], 0
    mov qword ptr [rsp+30h], 0
    call CreateFileA
    
    mov hPipe, rax
    cmp rax, -1
    je _sleep_retry

    ; Prepare GET_STATUS
    lea rcx, bufRequest
    lea rdx, szGetStatus
_cpy:
    mov al, [rdx]
    mov [rcx], al
    test al, al
    jz _do_write
    inc rcx
    inc rdx
    jmp _cpy

_do_write:
    ; WriteFile
    mov rcx, hPipe
    lea rdx, bufRequest
    mov r8d, 11
    lea r9, bytesWritten
    mov qword ptr [rsp+20h], 0
    call WriteFile

    ; Read Response
    mov rcx, hPipe
    lea rdx, bufResponse
    mov r8d, 1024
    lea r9, bytesRead
    mov qword ptr [rsp+20h], 0
    call ReadFile
    
    ; Get stdout
    mov rcx, STD_OUTPUT_HANDLE
    call GetStdHandle
    mov rbx, rax ; save stdout handle in rbx

    ; Print
    mov rcx, rbx
    lea rdx, bufResponse
    mov r8d, dword ptr [bytesRead] ; usage of r8d logic
    lea r9, bytesWritten
    mov qword ptr [rsp+20h], 0
    call WriteConsoleA

    mov rcx, hPipe
    call CloseHandle
    
    mov ecx, 1000
    call Sleep
    jmp _loop_connect

_sleep_retry:
    mov ecx, 500
    call Sleep
    jmp _loop_connect
    ret

main ENDP
END

