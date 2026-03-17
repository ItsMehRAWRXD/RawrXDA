; agent_client_evict.asm
; Sends SET_EVICT <drive_id> command to SovereignThermalAgent pipe

option casemap:none
extern CreateFileA : PROC
extern WriteFile : PROC
extern CloseHandle : PROC
extern Sleep : PROC
extern wsprintfA : PROC
extern ExitProcess : PROC

.const
    GENERIC_READ  equ 80000000h
    GENERIC_WRITE equ 40000000h
    OPEN_EXISTING equ 3
    szPipeName     db "\\.\pipe\SovereignThermalAgent", 0
    szSetEvictFmt  db "SET_EVICT %d", 0
    drive_id       dd 2             ; <=== Customize this as needed

.data
    hPipe          dq ?
    bufCommand     db 64 dup(0)
    bytesWritten   dq 0

.code
main PROC FRAME
    sub rsp, 48h
    .allocstack 48h
    .endprolog

_retry:
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
    je _fail_wait

    ; Format command
    lea rcx, bufCommand
    lea rdx, szSetEvictFmt
    mov r8d, drive_id
    call wsprintfA
    mov ebx, eax ; save length

    ; Send to pipe
    mov rcx, hPipe
    lea rdx, bufCommand
    mov r8d, ebx
    lea r9, bytesWritten
    mov qword ptr [rsp+20h], 0
    call WriteFile

    mov rcx, hPipe
    call CloseHandle
    
    xor ecx, ecx
    call ExitProcess

_fail_wait:
    mov ecx, 500
    call Sleep
    jmp _retry
    ret
main ENDP
END


