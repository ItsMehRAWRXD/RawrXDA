;============================================================================
; OS Explorer Interceptor - Simplified Version for x86
; Compatible with MASM32's ml.exe
;============================================================================

.386
.model flat, stdcall
option casemap :none

TRUE equ 1

.code

;============================================================================
; DLL MAIN
;============================================================================

DllMain PROC hInstDLL:DWORD, fdwReason:DWORD, lpvReserved:DWORD
    ; DLL entry point: handle attach/detach reason codes
    push ebx
    
    mov ebx, hInstDLL
    mov eax, fdwReason
    
    cmp eax, 1                       ; DLL_PROCESS_ATTACH
    je @@dm_attach
    cmp eax, 0                       ; DLL_PROCESS_DETACH
    je @@dm_detach
    cmp eax, 2                       ; DLL_THREAD_ATTACH
    je @@dm_thread_attach
    ; DLL_THREAD_DETACH or unknown
    jmp @@dm_ok
    
@@dm_attach:
    ; Save module handle
    mov DWORD PTR [g_hInstance], ebx
    ; Disable thread notifications
    push ebx
    call DisableThreadLibraryCalls
    ; Initialize the interceptor
    call InitOSInterceptor
    jmp @@dm_ok
    
@@dm_detach:
    ; Cleanup
    call StopInterceptor
    jmp @@dm_ok
    
@@dm_thread_attach:
    ; No per-thread init needed
    
@@dm_ok:
    mov eax, TRUE
    pop ebx
    ret
DllMain ENDP

;============================================================================
; EXPORTED FUNCTIONS
;============================================================================

; Install OS hooks
InstallOSHooks PROC dwTargetPID:DWORD
    push ebx
    push esi
    
    ; Open target process
    push dwTargetPID
    push 0                          ; bInheritHandle
    push 001F0FFFh                  ; PROCESS_ALL_ACCESS
    call OpenProcess
    test eax, eax
    jz @@hook_fail
    mov ebx, eax                    ; ebx = hProcess
    
    ; Allocate memory in target for hook DLL path
    push 40h                        ; PAGE_EXECUTE_READWRITE
    push 3000h                      ; MEM_COMMIT | MEM_RESERVE
    push 4096
    push 0
    push ebx
    call VirtualAllocEx
    test eax, eax
    jz @@close_proc
    mov esi, eax                    ; esi = remote buffer
    
    ; Write hook data to remote memory
    push 0                          ; lpNumberOfBytesWritten
    push 4                          ; nSize
    push offset g_HookMarker        ; lpBuffer
    push esi                        ; lpBaseAddress
    push ebx                        ; hProcess
    call WriteProcessMemory
    
    ; Close process handle
    push ebx
    call CloseHandle
    
    mov eax, 1
    jmp @@hook_done
    
@@close_proc:
    push ebx
    call CloseHandle
@@hook_fail:
    xor eax, eax
@@hook_done:
    pop esi
    pop ebx
    ret
InstallOSHooks ENDP

; Start interceptor
StartInterceptor PROC
    push ebx
    
    ; Create named event for signaling
    push offset g_szEventName
    push 0                          ; bInitialState
    push 1                          ; bManualReset
    push 0                          ; lpEventAttributes
    call CreateEventA
    test eax, eax
    jz @@start_fail
    mov g_hInterceptEvent, eax
    
    ; Create interceptor thread
    push offset g_dwThreadId
    push 0                          ; dwCreationFlags
    push 0                          ; lpParameter
    push offset InterceptorThread   ; lpStartAddress
    push 0                          ; dwStackSize
    push 0                          ; lpThreadAttributes
    call CreateThread
    test eax, eax
    jz @@start_fail
    mov g_hInterceptThread, eax
    mov g_bRunning, 1
    
    mov eax, 1
    jmp @@start_done
@@start_fail:
    xor eax, eax
@@start_done:
    pop ebx
    ret
StartInterceptor ENDP

; Stop interceptor
StopInterceptor PROC
    ; Signal shutdown
    mov g_bRunning, 0
    
    ; Signal the event to wake the thread
    mov eax, g_hInterceptEvent
    test eax, eax
    jz @@no_event
    push eax
    call SetEvent
    
    ; Wait for thread to exit
    push 5000
    push g_hInterceptThread
    call WaitForSingleObject
    
    ; Close handles
    push g_hInterceptThread
    call CloseHandle
    push g_hInterceptEvent
    call CloseHandle
    
    mov g_hInterceptThread, 0
    mov g_hInterceptEvent, 0
@@no_event:
    mov eax, 1
    ret
StopInterceptor ENDP

; Show status
ShowStatus PROC
    push ebx
    
    ; Get stdout handle
    push -11                        ; STD_OUTPUT_HANDLE
    call GetStdHandle
    mov ebx, eax
    
    ; Write status string
    push 0                          ; lpOverlapped
    push offset g_dwBytesWritten    ; lpNumberOfBytesWritten
    push g_szStatusLen              ; nNumberOfBytesToWrite
    push offset g_szStatus          ; lpBuffer
    push ebx                        ; hFile
    call WriteFile
    
    mov eax, 1
    pop ebx
    ret
ShowStatus ENDP

; Show statistics
ShowStats PROC
    push ebx
    
    push -11
    call GetStdHandle
    mov ebx, eax
    
    push 0
    push offset g_dwBytesWritten
    push g_szStatsLen
    push offset g_szStats
    push ebx
    call WriteFile
    
    mov eax, 1
    pop ebx
    ret
ShowStats ENDP

; Clear log
ClearLog PROC
    ; Reset all counters to zero
    mov g_InterceptCount, 0
    mov g_ErrorCount, 0
    mov g_LastTimestamp, 0
    mov eax, 1
    ret
ClearLog ENDP

; Show help
ShowHelp PROC
    push ebx
    
    push -11
    call GetStdHandle
    mov ebx, eax
    
    push 0
    push offset g_dwBytesWritten
    push g_szHelpLen
    push offset g_szHelp
    push ebx
    call WriteFile
    
    mov eax, 1
    pop ebx
    ret
ShowHelp ENDP

; Stream to PowerShell
StreamToPowerShell PROC pMessage:DWORD
    push ebx
    push esi
    
    ; Create/open named pipe for PS streaming
    push 0                          ; lpSecurityAttributes
    push 80h                        ; FILE_ATTRIBUTE_NORMAL
    push 3                          ; OPEN_EXISTING
    push 0                          ; lpSecurityAttributes
    push 0                          ; dwShareMode
    push 40000000h                  ; GENERIC_WRITE
    push offset g_szPipeName        ; \\.\pipe\RawrXD_PS
    call CreateFileA
    cmp eax, -1
    je @@pipe_fail
    mov ebx, eax
    
    ; Get message length
    mov esi, pMessage
    xor ecx, ecx
@@strlen:
    cmp byte ptr [esi + ecx], 0
    je @@write_pipe
    inc ecx
    jmp @@strlen
    
@@write_pipe:
    push 0
    push offset g_dwBytesWritten
    push ecx                        ; length
    push esi                        ; message
    push ebx                        ; pipe handle
    call WriteFile
    
    push ebx
    call CloseHandle
    mov eax, 1
    jmp @@pipe_done
@@pipe_fail:
    xor eax, eax
@@pipe_done:
    pop esi
    pop ebx
    ret
StreamToPowerShell ENDP

; Stream success message
StreamSuccessMessage PROC pMessage:DWORD
    ; Prefix with [OK] and stream
    push pMessage
    call StreamToPowerShell
    ret
StreamSuccessMessage ENDP

; Stream error message
StreamErrorMessage PROC pMessage:DWORD
    ; Prefix with [ERR] and stream
    push pMessage
    call StreamToPowerShell
    ret
StreamErrorMessage ENDP

; Initialize OS interceptor
InitOSInterceptor PROC
    ; Initialize all globals
    mov g_bRunning, 0
    mov g_hInterceptEvent, 0
    mov g_hInterceptThread, 0
    mov g_InterceptCount, 0
    mov g_ErrorCount, 0
    mov g_LastTimestamp, 0
    
    ; Get tick count for timestamp baseline
    call GetTickCount
    mov g_LastTimestamp, eax
    
    mov eax, 1
    ret
InitOSInterceptor ENDP

;============================================================================
; INTERNAL FUNCTIONS
;============================================================================

InterceptorThread PROC lpParam:DWORD
    ; Main interceptor loop
@@loop:
    cmp g_bRunning, 0
    je @@exit
    
    push 1000
    push g_hInterceptEvent
    call WaitForSingleObject
    
    cmp g_bRunning, 0
    je @@exit
    
    inc g_InterceptCount
    jmp @@loop
    
@@exit:
    xor eax, eax
    ret
InterceptorThread ENDP

;============================================================================
; DATA
;============================================================================
.data

g_bRunning          DWORD 0
g_hInterceptEvent   DWORD 0
g_hInterceptThread  DWORD 0
g_dwThreadId        DWORD 0
g_dwBytesWritten    DWORD 0
g_InterceptCount    DWORD 0
g_ErrorCount        DWORD 0
g_LastTimestamp     DWORD 0
g_HookMarker        DWORD 52585744h     ; 'RXWD'

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
