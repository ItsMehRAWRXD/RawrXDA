; agent_client_evict.asm
; Sends SET_EVICT <drive_id> command to SovereignThermalAgent pipe

include windows.inc
includelib kernel32.lib

.const
szPipeName     db "\\.\pipe\SovereignThermalAgent", 0
szSetEvictFmt  db "SET_EVICT %d", 0
bufCommand     db 64 dup(0)
drive_id       dd 2             ; <=== Customize this as needed

.data
hPipe          dq ?

.code
main PROC
    sub rsp, 28h

.retry:
    invoke CreateFileA, offset szPipeName, GENERIC_READ or GENERIC_WRITE, 0, 0, OPEN_EXISTING, 0, 0
    mov hPipe, rax
    cmp rax, -1
    je .fail_wait

    ; Format command
    invoke wsprintfA, offset bufCommand, offset szSetEvictFmt, drive_id

    ; Send to pipe
    lea rcx, hPipe
    lea rdx, bufCommand
    mov r8d, 64
    lea r9, [rsp+20h]
    mov qword ptr [rsp+18h], 0
    call WriteFile

    call CloseHandle
    ret

.fail_wait:
    call Sleep, 500
    jmp .retry
main ENDP
END
