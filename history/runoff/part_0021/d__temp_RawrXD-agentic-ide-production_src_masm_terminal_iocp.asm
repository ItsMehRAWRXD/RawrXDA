;==============================================================================
; File 3: terminal_iocp.asm - Async IOCP Terminal with VT100 Parsing
;==============================================================================
include windows.inc
include richedit.inc

.code
;==============================================================================
; Initialize IOCP-based PowerShell Terminal
;==============================================================================
Terminal_InitPowerShell PROC
    LOCAL si:STARTUPINFOA
    LOCAL pi:PROCESS_INFORMATION
    LOCAL hStdOutRead:HANDLE
    LOCAL hStdOutWrite:HANDLE
    LOCAL sa:SECURITY_ATTRIBUTES
    
    ; Create IOCP
    invoke CreateIoCompletionPort, 
        INVALID_HANDLE_VALUE, NULL, 0, 0
    mov [hIOCP], rax
    
    ; Create anonymous pipe for stdout
    mov [sa.nLength], SIZEOF SECURITY_ATTRIBUTES
    mov [sa.bInheritHandle], TRUE
    
    invoke CreatePipe, ADDR hStdOutRead, ADDR hStdOutWrite, 
        ADDR sa, 0
    .if rax == FALSE
        LOG_ERROR "Failed to create pipe"
        xor rax, rax
        ret
    .endif
    
    ; Associate pipe with IOCP
    invoke CreateIoCompletionPort, hStdOutRead, [hIOCP],
        1, 0
    
    ; Start PowerShell
    mov [si.cb], SIZEOF STARTUPINFOA
    mov [si.dwFlags], STARTF_USESTDHANDLES
    mov [si.hStdOutput], hStdOutWrite
    mov [si.hStdError], hStdOutWrite
    
    invoke CreateProcessA, NULL, OFFSET szPowerShellCmd,
        NULL, NULL, TRUE, 0, NULL, NULL, 
        ADDR si, ADDR pi
    
    .if rax == FALSE
        LOG_ERROR "Failed to start PowerShell"
        xor rax, rax
        ret
    .endif
    
    mov [hPowerShellProcess], pi.hProcess
    invoke CloseHandle, hStdOutWrite
    mov [hTerminalOutput], hStdOutRead
    
    ; Start IOCP worker thread
    invoke CreateThread, NULL, 0,
        Terminal_IOCPWorker, NULL, 0, NULL
    
    LOG_INFO "PowerShell terminal initialized via IOCP"
    
    mov rax, 1
    ret
Terminal_InitPowerShell ENDP

;==============================================================================
; IOCP Worker Thread
;==============================================================================
Terminal_IOCPWorker PROC lpParam:QWORD
    LOCAL bytesTransferred:DWORD
    LOCAL completionKey:QWORD
    LOCAL pOverlapped:QWORD
    LOCAL buffer[4096]:BYTE
    
@iocpLoop:
    invoke GetQueuedCompletionStatus, [hIOCP],
        ADDR bytesTransferred, ADDR completionKey,
        ADDR pOverlapped, INFINITE
    
    .if rax == FALSE
        LOG_ERROR "IOCP failed"
        jmp @exit
    .endif
    
    .if [bytesTransferred] > 0
        call Terminal_ProcessOutput, 
            ADDR buffer, [bytesTransferred]
    .endif
    
    jmp @iocpLoop
    
@exit:
    xor rax, rax
    ret
Terminal_IOCPWorker ENDP

;==============================================================================
; Process Terminal Output (VT100 Parsing)
;==============================================================================
Terminal_ProcessOutput PROC lpBuffer:QWORD, length:DWORD
    LOCAL i:DWORD
    LOCAL displayPos:DWORD
    
    mov [i], 0
    mov [displayPos], 0
    
@parseLoop:
    mov eax, [i]
    cmp eax, [length]
    jge @append
    
    movzx r8b, BYTE PTR [lpBuffer + rax]
    
    .if r8b == 1Bh      ; ESC
        call Terminal_ParseEscapeSeq, lpBuffer, [i], ADDR [i]
        jmp @parseLoop
    .else
        ; Regular character
        mov BYTE PTR [displayBuffer + displayPos], r8b
        inc [displayPos]
        inc [i]
    .endif
    
    jmp @parseLoop
    
@append:
    mov BYTE PTR [displayBuffer + displayPos], 0
    invoke SendMessage, [hTerminal], EM_SETSEL, -1, -1
    invoke SendMessage, [hTerminal], EM_REPLACESEL, FALSE,
        ADDR displayBuffer
    
    ret
Terminal_ProcessOutput ENDP

;==============================================================================
; Terminate PowerShell
;==============================================================================
Terminal_TerminatePowerShell PROC
    .if [hPowerShellProcess] != NULL
        invoke TerminateProcess, [hPowerShellProcess], 0
        invoke CloseHandle, [hPowerShellProcess]
    .endif
    
    .if [hTerminalOutput] != NULL
        invoke CloseHandle, [hTerminalOutput]
    .endif
    
    .if [hIOCP] != NULL
        invoke CloseHandle, [hIOCP]
    .endif
    
    LOG_INFO "PowerShell terminal terminated"
    
    ret
Terminal_TerminatePowerShell ENDP

;==============================================================================
; Data
;==============================================================================
.data
; Handles
hIOCP                dq ?
hPowerShellProcess   dq ?
hTerminalOutput      dq ?

; Buffers
displayBuffer        db 8192 dup(?)

; Commands
szPowerShellCmd      db 'powershell.exe -NoExit',0

END
