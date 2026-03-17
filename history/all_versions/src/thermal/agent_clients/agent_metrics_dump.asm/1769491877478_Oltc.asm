; agent_metrics_dump.asm
; Connects once, reads JSON, dumps it to agent_dump.log

include windows.inc
includelib kernel32.lib

.const
szPipeName    db "\\.\pipe\SovereignThermalAgent", 0
szGetStatus   db "GET_STATUS", 0
szLogFile     db "agent_dump.log", 0
bufOut        db 1024 dup(0)

.data
hPipe         dq ?
hFile         dq ?

.code
main PROC
    sub rsp, 28h

.try:
    invoke CreateFileA, offset szPipeName, GENERIC_READ or GENERIC_WRITE, 0, 0, OPEN_EXISTING, 0, 0
    mov hPipe, rax
    cmp rax, -1
    je .wait

    ; Send GET_STATUS
    invoke WriteFile, hPipe, offset szGetStatus, 11, 0, 0

    ; Read Response
    invoke ReadFile, hPipe, offset bufOut, 1024, 0, 0

    ; Open log
    invoke CreateFileA, offset szLogFile, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0
    mov hFile, rax
    invoke WriteFile, hFile, offset bufOut, 1024, 0, 0
    call CloseHandle
    call CloseHandle

    ret

.wait:
    call Sleep, 1000
    jmp .try
main ENDP
END
