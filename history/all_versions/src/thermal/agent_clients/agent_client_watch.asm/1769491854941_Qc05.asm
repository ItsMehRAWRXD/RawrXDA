; agent_client_watch.asm
; Loops and watches the Sovereign pipe for real-time status reports

include windows.inc
includelib kernel32.lib

.const
szPipeName     db "\\.\pipe\SovereignThermalAgent", 0
szGetStatus    db "GET_STATUS", 0
hPipe          dq 0

.data
bufRequest     db 64 dup(0)
bufResponse    db 1024 dup(0)

.code
main PROC
    sub rsp, 28h

.loop_connect:
    invoke CreateFileA, offset szPipeName, GENERIC_READ or GENERIC_WRITE, 0, 0, OPEN_EXISTING, 0, 0
    mov hPipe, rax
    cmp rax, -1
    je .sleep_retry

    ; Send GET_STATUS
    lea rcx, bufRequest
    lea rdx, szGetStatus
.sendloop:
    mov al, [rdx]
    mov [rcx], al
    test al, al
    jz .send
    inc rcx
    inc rdx
    jmp .sendloop

.send:
    lea rcx, hPipe
    lea rdx, bufRequest
    mov r8d, 11
    lea r9, [rsp+20h]
    mov qword ptr [rsp+18h], 0
    call WriteFile

    ; Read response
    lea rcx, hPipe
    lea rdx, bufResponse
    mov r8d, 1024
    lea r9, [rsp+20h]
    mov qword ptr [rsp+18h], 0
    call ReadFile

    ; Print response
    invoke GetStdHandle, STD_OUTPUT_HANDLE
    mov rcx, rax
    lea rdx, bufResponse
    call WriteConsoleA

    call CloseHandle
    call Sleep, 1000
    jmp .loop_connect

.sleep_retry:
    call Sleep, 500
    jmp .loop_connect
main ENDP
END
