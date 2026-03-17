;============================================================================
; OS Explorer Interceptor - Simplified Version
; Compatible with Visual Studio's ml64.exe
;============================================================================

TRUE equ 1


; ─── Cross-module symbol resolution ───
INCLUDE rawrxd_master.inc

.code

;============================================================================
; DLL MAIN
;============================================================================

DllMain PROC hInstDLL:QWORD, fdwReason:DWORD, lpvReserved:QWORD
    mov rax, TRUE
    ret
DllMain ENDP

;============================================================================
; EXPORTED FUNCTIONS
;============================================================================

; Install OS hooks
InstallOSHooks PROC dwTargetPID:DWORD
    push rbx
    push rsi
    sub rsp, 40
    
    ; Open target process
    mov ecx, 001F0FFFh              ; PROCESS_ALL_ACCESS
    xor edx, edx                    ; bInheritHandle = FALSE
    mov r8d, dwTargetPID
    call OpenProcess
    test rax, rax
    jz @@hook_fail
    mov rbx, rax                    ; rbx = hProcess
    
    ; Allocate memory in target for hook data
    mov rcx, rbx                    ; hProcess
    xor rdx, rdx                    ; lpAddress = NULL
    mov r8d, 4096                   ; dwSize
    mov r9d, 3000h                  ; MEM_COMMIT | MEM_RESERVE
    mov dword ptr [rsp + 32], 40h   ; PAGE_EXECUTE_READWRITE
    call VirtualAllocEx
    test rax, rax
    jz @@close_proc
    mov rsi, rax                    ; rsi = remote buffer
    
    ; Write hook marker to remote memory
    mov rcx, rbx                    ; hProcess
    mov rdx, rsi                    ; lpBaseAddress
    lea r8, g_HookMarker            ; lpBuffer
    mov r9d, 8                      ; nSize
    xor rax, rax
    mov [rsp + 32], rax             ; lpNumberOfBytesWritten
    call WriteProcessMemory
    
    ; Close process handle
    mov rcx, rbx
    call CloseHandle
    
    mov rax, 1
    jmp @@hook_done
    
@@close_proc:
    mov rcx, rbx
    call CloseHandle
@@hook_fail:
    xor rax, rax
@@hook_done:
    add rsp, 40
    pop rsi
    pop rbx
    ret
InstallOSHooks ENDP

; Start interceptor
StartInterceptor PROC
    push rbx
    sub rsp, 48
    
    ; Create named event for signaling
    xor ecx, ecx                    ; lpEventAttributes
    mov edx, 1                      ; bManualReset
    xor r8d, r8d                    ; bInitialState
    lea r9, g_szEventName
    call CreateEventA
    test rax, rax
    jz @@start_fail
    mov g_hInterceptEvent, rax
    
    ; Create interceptor thread
    xor ecx, ecx                    ; lpThreadAttributes
    xor edx, edx                    ; dwStackSize
    lea r8, InterceptorThread       ; lpStartAddress
    xor r9, r9                      ; lpParameter
    xor rax, rax
    mov [rsp + 32], rax             ; dwCreationFlags
    lea rax, g_dwThreadId
    mov [rsp + 40], rax             ; lpThreadId
    call CreateThread
    test rax, rax
    jz @@start_fail
    mov g_hInterceptThread, rax
    mov g_bRunning, 1
    
    mov rax, 1
    jmp @@start_done
@@start_fail:
    xor rax, rax
@@start_done:
    add rsp, 48
    pop rbx
    ret
StartInterceptor ENDP

; Stop interceptor
StopInterceptor PROC
    sub rsp, 40
    
    ; Signal shutdown
    mov g_bRunning, 0
    
    ; Signal event to wake thread
    mov rcx, g_hInterceptEvent
    test rcx, rcx
    jz @@no_event
    call SetEvent
    
    ; Wait for thread
    mov rcx, g_hInterceptThread
    mov edx, 5000
    call WaitForSingleObject
    
    ; Close handles
    mov rcx, g_hInterceptThread
    call CloseHandle
    mov rcx, g_hInterceptEvent
    call CloseHandle
    
    mov g_hInterceptThread, 0
    mov g_hInterceptEvent, 0
@@no_event:
    mov rax, 1
    add rsp, 40
    ret
StopInterceptor ENDP

; Show status
ShowStatus PROC
    sub rsp, 48
    
    mov ecx, -11                    ; STD_OUTPUT_HANDLE
    call GetStdHandle
    
    mov rcx, rax                    ; hFile
    lea rdx, g_szStatus             ; lpBuffer
    mov r8d, g_szStatusLen          ; nNumberOfBytesToWrite
    lea r9, g_dwBytesWritten        ; lpNumberOfBytesWritten
    mov qword ptr [rsp + 32], 0     ; lpOverlapped
    call WriteFile
    
    mov rax, 1
    add rsp, 48
    ret
ShowStatus ENDP

; Show statistics
ShowStats PROC
    sub rsp, 48
    
    mov ecx, -11
    call GetStdHandle
    
    mov rcx, rax
    lea rdx, g_szStats
    mov r8d, g_szStatsLen
    lea r9, g_dwBytesWritten
    mov qword ptr [rsp + 32], 0
    call WriteFile
    
    mov rax, 1
    add rsp, 48
    ret
ShowStats ENDP

; Clear log
ClearLog PROC
    ; Reset all counters
    mov g_InterceptCount, 0
    mov g_ErrorCount, 0
    mov g_LastTimestamp, 0
    mov rax, 1
    ret
ClearLog ENDP

; Show help
ShowHelp PROC
    sub rsp, 48
    
    mov ecx, -11
    call GetStdHandle
    
    mov rcx, rax
    lea rdx, g_szHelp
    mov r8d, g_szHelpLen
    lea r9, g_dwBytesWritten
    mov qword ptr [rsp + 32], 0
    call WriteFile
    
    mov rax, 1
    add rsp, 48
    ret
ShowHelp ENDP

; Stream to PowerShell
StreamToPowerShell PROC pMessage:QWORD
    push rbx
    push rsi
    sub rsp, 56
    
    mov rsi, rcx                    ; rsi = pMessage
    
    ; Open named pipe for PS streaming
    lea rcx, g_szPipeName           ; \\.\pipe\RawrXD_PS
    mov edx, 40000000h              ; GENERIC_WRITE
    xor r8d, r8d                    ; dwShareMode
    xor r9, r9                      ; lpSecurityAttributes
    mov dword ptr [rsp + 32], 3     ; OPEN_EXISTING
    mov dword ptr [rsp + 40], 80h   ; FILE_ATTRIBUTE_NORMAL
    mov qword ptr [rsp + 48], 0     ; hTemplateFile
    call CreateFileA
    cmp rax, -1
    je @@pipe_fail
    mov rbx, rax                    ; rbx = hPipe
    
    ; Calculate message length
    mov rcx, rsi
    xor rdx, rdx
@@strlen:
    cmp byte ptr [rsi + rdx], 0
    je @@write_pipe
    inc rdx
    jmp @@strlen
    
@@write_pipe:
    mov rcx, rbx                    ; hFile
    mov r8, rdx                     ; nNumberOfBytesToWrite
    mov rdx, rsi                    ; lpBuffer
    lea r9, g_dwBytesWritten
    mov qword ptr [rsp + 32], 0
    call WriteFile
    
    mov rcx, rbx
    call CloseHandle
    mov rax, 1
    jmp @@pipe_done
@@pipe_fail:
    xor rax, rax
@@pipe_done:
    add rsp, 56
    pop rsi
    pop rbx
    ret
StreamToPowerShell ENDP

; Stream success message
StreamSuccessMessage PROC pMessage:QWORD
    sub rsp, 32
    ; Delegate to StreamToPowerShell
    call StreamToPowerShell
    add rsp, 32
    ret
StreamSuccessMessage ENDP

; Stream error message
StreamErrorMessage PROC pMessage:QWORD
    sub rsp, 32
    ; Delegate to StreamToPowerShell
    call StreamToPowerShell
    add rsp, 32
    ret
StreamErrorMessage ENDP

; Initialize OS interceptor
InitOSInterceptor PROC
    sub rsp, 32
    
    ; Initialize globals to zero
    mov g_bRunning, 0
    mov g_hInterceptEvent, 0
    mov g_hInterceptThread, 0
    mov g_InterceptCount, 0
    mov g_ErrorCount, 0
    mov g_LastTimestamp, 0
    
    ; Get tick count for timestamp baseline
    call GetTickCount64
    mov g_LastTimestamp, rax
    
    mov rax, 1
    add rsp, 32
    ret
InitOSInterceptor ENDP

;============================================================================
; INTERNAL FUNCTIONS
;============================================================================

InterceptorThread PROC lpParam:QWORD
    sub rsp, 32
@@loop:
    cmp g_bRunning, 0
    je @@exit
    
    mov rcx, g_hInterceptEvent
    mov edx, 1000
    call WaitForSingleObject
    
    cmp g_bRunning, 0
    je @@exit
    
    lock inc g_InterceptCount
    jmp @@loop
    
@@exit:
    xor rax, rax
    add rsp, 32
    ret
InterceptorThread ENDP

;============================================================================
; DATA
;============================================================================
.data

g_bRunning          DWORD 0
g_hInterceptEvent   QWORD 0
g_hInterceptThread  QWORD 0
g_dwThreadId        DWORD 0
g_dwBytesWritten    DWORD 0
g_InterceptCount    DWORD 0
g_ErrorCount        DWORD 0
g_LastTimestamp     QWORD 0
g_HookMarker        QWORD 5258574452415752h     ; 'RAWRXDWR'

g_szEventName       BYTE "Global\RawrXDInterceptor", 0
g_szPipeName        BYTE "\\.\pipe\RawrXD_PS", 0

g_szStatus          BYTE "[RawrXD Interceptor] Status: Active", 13, 10, 0
g_szStatusLen       DWORD 37
g_szStats           BYTE "[RawrXD] Intercepts: 0  Errors: 0", 13, 10, 0
g_szStatsLen        DWORD 36
g_szHelp            BYTE "RawrXD OS Interceptor Commands:", 13, 10
                    BYTE "  install <pid> - Install hooks", 13, 10
                    BYTE "  start         - Start interceptor", 13, 10
                    BYTE "  stop          - Stop interceptor", 13, 10
                    BYTE "  status        - Show status", 13, 10
                    BYTE "  stats         - Show statistics", 13, 10
                    BYTE "  clear         - Clear log", 13, 10
                    BYTE "  help          - Show this help", 13, 10, 0
g_szHelpLen         DWORD 260
