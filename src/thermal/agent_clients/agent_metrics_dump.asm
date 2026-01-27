; agent_metrics_dump.asm
; Connects once, reads JSON, dumps it to agent_dump.log

option casemap:none
extern CreateFileA : PROC
extern WriteFile : PROC
extern ReadFile : PROC
extern CloseHandle : PROC
extern Sleep : PROC
extern ExitProcess : PROC

.const
    GENERIC_READ  equ 80000000h
    GENERIC_WRITE equ 40000000h
    OPEN_EXISTING equ 3
    CREATE_ALWAYS equ 2
    FILE_ATTRIBUTE_NORMAL equ 80h

    szPipeName    db "\\.\pipe\SovereignThermalAgent", 0
    szGetStatus   db "GET_STATUS", 0
    szLogFile     db "agent_dump.log", 0

.data
    hPipe         dq ?
    hFile         dq ?
    bufOut        db 1024 dup(0)
    bytesTransferred dq 0

.code
main PROC FRAME
    sub rsp, 48h
    .allocstack 48h
    .endprolog

_try:
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
    je _wait

    ; Send GET_STATUS
    mov rcx, hPipe
    lea rdx, szGetStatus
    mov r8d, 11
    lea r9, bytesTransferred
    mov qword ptr [rsp+20h], 0
    call WriteFile

    ; Read Response
    mov rcx, hPipe
    lea rdx, bufOut
    mov r8d, 1024
    lea r9, bytesTransferred
    mov qword ptr [rsp+20h], 0
    call ReadFile
    
    mov rbx, bytesTransferred ; save count

    ; Open log
    mov rcx, offset szLogFile
    mov edx, GENERIC_WRITE
    mov r8d, 0
    mov r9d, 0
    mov dword ptr [rsp+20h], CREATE_ALWAYS
    mov dword ptr [rsp+28h], FILE_ATTRIBUTE_NORMAL
    mov qword ptr [rsp+30h], 0
    call CreateFileA
    
    mov hFile, rax
    
    ; Write log
    mov rcx, hFile
    lea rdx, bufOut
    mov r8d, ebx ; count
    lea r9, bytesTransferred
    mov qword ptr [rsp+20h], 0
    call WriteFile

    mov rcx, hFile
    call CloseHandle
    mov rcx, hPipe
    call CloseHandle

    xor ecx, ecx
    call ExitProcess

_wait:
    mov ecx, 1000
    call Sleep
    jmp _try
main ENDP
END

