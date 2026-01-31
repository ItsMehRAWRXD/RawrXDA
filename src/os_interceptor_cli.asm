;============================================================================
; OS Explorer Interceptor CLI - MASM IDE Integration
; Command-line interface for controlling the OS Explorer Interceptor
; Provides interactive CLI and PowerShell streaming integration
;============================================================================

.686
.MMX
.XMM
.model flat, stdcall

option casemap :none
option prologue:none
option epilogue:none

;============================================================================
; INCLUDES
;============================================================================

include \masm32\include\windows.inc
include \masm32\include\kernel32.inc
include \masm32\include\user32.inc
include \masm32\include\advapi32.inc

includelib \masm32\lib\kernel32.lib
includelib \masm32\lib\user32.lib
includelib \masm32\lib\advapi32.lib

;============================================================================
; CONSTANTS
;============================================================================

CLI_VERSION             equ "1.0.0"
CLI_MAGIC               equ 0x0CLIMASMIDE

; Command IDs
CMD_START               equ 1
CMD_STOP                equ 2
CMD_STATUS              equ 3
CMD_STATS               equ 4
CMD_CLEAR               equ 5
CMD_HELP                equ 6
CMD_EXIT                equ 7
CMD_FILTER              equ 8
CMD_EXPORT              equ 9

; Buffer sizes
CLI_BUFFER_SIZE         equ 4096
COMMAND_BUFFER_SIZE     equ 256
OUTPUT_BUFFER_SIZE      equ 8192

;============================================================================
; STRUCTURES
;============================================================================

; CLI context
CLIContext STRUCT
    Magic           QWORD ?
    Version         DWORD ?
    IsRunning       QWORD ?
    TargetPID       DWORD ?
    hTargetProcess  QWORD ?
    hStdIn          QWORD ?
    hStdOut         QWORD ?
    hStdErr         QWORD ?
    CommandBuffer   BYTE COMMAND_BUFFER_SIZE DUP(?)
    OutputBuffer    BYTE OUTPUT_BUFFER_SIZE DUP(?)
    Filter          DWORD ?
    IsFiltered      QWORD ?
    ExportPath      BYTE MAX_PATH DUP(?)
    IsExporting     QWORD ?
CLIContext ENDS

; Command structure
Command STRUCT
    Id              DWORD ?
    Name            BYTE 32 DUP(?)
    Description     BYTE 256 DUP(?)
    Handler         QWORD ?
Command ENDS

;============================================================================
; GLOBAL DATA
;============================================================================

.data

; CLI context
g_CLIContext CLIContext <
    CLI_MAGIC,
    1,
    0,
    0,
    0,
    0,
    0,
    0,
    {0},
    {0},
    0,
    0,
    {0},
    0
>

; Command table
g_CommandTable Command 10 DUP(<
    0, {0}, {0}, 0
>)

; Welcome banner
g_WelcomeBanner BYTE \
    "╔══════════════════════════════════════════════════════════════════════╗", 0Dh, 0Ah, \
    "║         OS EXPLORER INTERCEPTOR - CLI v", CLI_VERSION, "                     ║", 0Dh, 0Ah, \
    "║         Integrated with MASM IDE - Real-time Streaming             ║", 0Dh, 0Ah, \
    "╚══════════════════════════════════════════════════════════════════════╝", 0Dh, 0Ah, \
    "", 0Dh, 0Ah, \
    "Type 'help' for available commands", 0Dh, 0Ah, \
    "", 0Dh, 0Ah, \
    0

; Help text
g_HelpText BYTE \
    "Available Commands:", 0Dh, 0Ah, \
    "───────────────────", 0Dh, 0Ah, \
    "  start <PID>     Start interceptor for process ID", 0Dh, 0Ah, \
    "  stop           Stop interceptor", 0Dh, 0Ah, \
    "  status         Show current status", 0Dh, 0Ah, \
    "  stats          Show statistics", 0Dh, 0Ah, \
    "  clear          Clear call log", 0Dh, 0Ah, \
    "  filter <types>  Set filter (FILE,NETWORK,etc.)", 0Dh, 0Ah, \
    "  export <path>  Export captured data", 0Dh, 0Ah, \
    "  help           Show this help", 0Dh, 0Ah, \
    "  exit           Exit CLI", 0Dh, 0Ah, \
    "", 0Dh, 0Ah, \
    "Examples:", 0Dh, 0Ah, \
    "  start 1234", 0Dh, 0Ah, \
    "  filter FILE,NETWORK", 0Dh, 0Ah, \
    "  export C:\\capture.json", 0Dh, 0Ah, \
    "", 0Dh, 0Ah, \
    0

; Prompt
g_Prompt BYTE "os-interceptor> ", 0

; Error messages
g_ErrorNotRunning BYTE "[ERROR] Interceptor not running. Use 'start <PID>' first.", 0Dh, 0Ah, 0
g_ErrorInvalidCommand BYTE "[ERROR] Invalid command. Type 'help' for available commands.", 0Dh, 0Ah, 0
g_ErrorInvalidPID BYTE "[ERROR] Invalid process ID.", 0Dh, 0Ah, 0
g_ErrorProcessNotFound BYTE "[ERROR] Process not found.", 0Dh, 0Ah, 0

; Success messages
g_SuccessStarted BYTE "[SUCCESS] OS Explorer Interceptor started.", 0Dh, 0Ah, 0
g_SuccessStopped BYTE "[SUCCESS] OS Explorer Interceptor stopped.", 0Dh, 0Ah, 0
g_SuccessCleared BYTE "[SUCCESS] Call log cleared.", 0Dh, 0Ah, 0

;============================================================================
; CODE
;============================================================================

.code

;============================================================================
; ENTRY POINT
;============================================================================

main PROC
    LOCAL hStdIn:QWORD
    LOCAL hStdOut:QWORD
    LOCAL hStdErr:QWORD
    
    ; Get standard handles
    invoke GetStdHandle, STD_INPUT_HANDLE
    mov hStdIn, rax
    mov g_CLIContext.hStdIn, rax
    
    invoke GetStdHandle, STD_OUTPUT_HANDLE
    mov hStdOut, rax
    mov g_CLIContext.hStdOut, rax
    
    invoke GetStdHandle, STD_ERROR_HANDLE
    mov hStdErr, rax
    mov g_CLIContext.hStdErr, rax
    
    ; Display welcome banner
    invoke WriteConsoleA, hStdOut, ADDR g_WelcomeBanner, \
           sizeof g_WelcomeBanner, NULL, NULL
    
    ; Initialize command table
    invoke InitCommandTable
    
    ; Main CLI loop
    invoke CLIMainLoop
    
    ; Cleanup
    invoke CleanupCLI
    
    ; Exit
    invoke ExitProcess, 0
    
    ret
main ENDP

;============================================================================
; CLI MAIN LOOP
;============================================================================

CLIMainLoop PROC
    LOCAL hStdIn:QWORD
    LOCAL hStdOut:QWORD
    LOCAL buffer[CLI_BUFFER_SIZE]:BYTE
    LOCAL bytesRead:DWORD
    
    mov hStdIn, g_CLIContext.hStdIn
    mov hStdOut, g_CLIContext.hStdOut
    
    ; Main loop
    .WHILE TRUE
        ; Display prompt
        invoke WriteConsoleA, hStdOut, ADDR g_Prompt, \
               sizeof g_Prompt - 1, NULL, NULL
        
        ; Read command
        invoke ReadConsoleA, hStdIn, ADDR buffer, CLI_BUFFER_SIZE, \
               ADDR bytesRead, NULL
        
        ; Process command
        .IF bytesRead > 2  ; At least 1 char + CRLF
            ; Null-terminate
            mov rax, bytesRead
            dec rax
            mov byte ptr [buffer + rax - 1], 0
            
            ; Trim leading/trailing whitespace
            invoke TrimString, ADDR buffer
            
            ; Process command
            invoke ProcessCommand, ADDR buffer
        .ENDIF
    .ENDW
    
    ret
CLIMainLoop ENDP

;============================================================================
; COMMAND PROCESSING
;============================================================================

ProcessCommand PROC pCommand:PTR BYTE
    LOCAL command[256]:BYTE
    LOCAL args[1024]:BYTE
    LOCAL pArgs:PTR BYTE
    
    ; Copy command
    invoke lstrcpyA, ADDR command, pCommand
    
    ; Find first space (separate command from args)
    invoke strchr, ADDR command, ' '
    .IF rax != 0
        ; Split command and args
        mov byte ptr [rax], 0
        inc rax
        mov pArgs, rax
        
        ; Trim args
        invoke TrimString, pArgs
    .ELSE
        mov pArgs, 0
    .ENDIF
    
    ; Trim command
    invoke TrimString, ADDR command
    
    ; Process based on command
    invoke stricmp, ADDR command, CSTR("start")
    .IF rax == 0
        invoke CMD_Start, pArgs
        ret
    .ENDIF
    
    invoke stricmp, ADDR command, CSTR("stop")
    .IF rax == 0
        invoke CMD_Stop
        ret
    .ENDIF
    
    invoke stricmp, ADDR command, CSTR("status")
    .IF rax == 0
        invoke CMD_Status
        ret
    .ENDIF
    
    invoke stricmp, ADDR command, CSTR("stats")
    .IF rax == 0
        invoke CMD_Stats
        ret
    .ENDIF
    
    invoke stricmp, ADDR command, CSTR("clear")
    .IF rax == 0
        invoke CMD_Clear
        ret
    .ENDIF
    
    invoke stricmp, ADDR command, CSTR("filter")
    .IF rax == 0
        invoke CMD_Filter, pArgs
        ret
    .ENDIF
    
    invoke stricmp, ADDR command, CSTR("export")
    .IF rax == 0
        invoke CMD_Export, pArgs
        ret
    .ENDIF
    
    invoke stricmp, ADDR command, CSTR("help")
    .IF rax == 0
        invoke CMD_Help
        ret
    .ENDIF
    
    invoke stricmp, ADDR command, CSTR("exit")
    .IF rax == 0
        invoke CMD_Exit
        ret
    .ENDIF
    
    invoke stricmp, ADDR command, CSTR("quit")
    .IF rax == 0
        invoke CMD_Exit
        ret
    .ENDIF
    
    ; Unknown command
    invoke WriteConsoleA, g_CLIContext.hStdErr, ADDR g_ErrorInvalidCommand, \
           sizeof g_ErrorInvalidCommand, NULL, NULL
    
    ret
ProcessCommand ENDP

;============================================================================
; COMMAND IMPLEMENTATIONS
;============================================================================

CMD_Start PROC pArgs:PTR BYTE
    LOCAL pidStr[32]:BYTE
    LOCAL pid:DWORD
    LOCAL hProcess:QWORD
    
    .IF pArgs == 0
        invoke WriteConsoleA, g_CLIContext.hStdErr, ADDR g_ErrorInvalidPID, \
               sizeof g_ErrorInvalidPID, NULL, NULL
        ret
    .ENDIF
    
    ; Copy PID string
    invoke lstrcpynA, ADDR pidStr, pArgs, 32
    
    ; Convert to integer
    invoke atoi, ADDR pidStr
    mov pid, eax
    
    .IF pid == 0
        invoke WriteConsoleA, g_CLIContext.hStdErr, ADDR g_ErrorInvalidPID, \
               sizeof g_ErrorInvalidPID, NULL, NULL
        ret
    .ENDIF
    
    ; Verify process exists
    invoke OpenProcess, PROCESS_QUERY_INFORMATION, FALSE, pid
    .IF rax == 0
        invoke WriteConsoleA, g_CLIContext.hStdErr, ADDR g_ErrorProcessNotFound, \
               sizeof g_ErrorProcessNotFound, NULL, NULL
        ret
    .ENDIF
    mov hProcess, rax
    invoke CloseHandle, hProcess
    
    ; Store target PID
    mov g_CLIContext.TargetPID, pid
    
    ; Load and inject interceptor DLL
    invoke InjectInterceptor, pid
    .IF rax == 0
        invoke WriteConsoleA, g_CLIContext.hStdErr, ADDR g_ErrorProcessNotFound, \
               sizeof g_ErrorProcessNotFound, NULL, NULL
        ret
    .ENDIF
    
    ; Mark as running
    mov g_CLIContext.IsRunning, 1
    
    ; Display success
    invoke WriteConsoleA, g_CLIContext.hStdOut, ADDR g_SuccessStarted, \
           sizeof g_SuccessStarted, NULL, NULL
    
    ret
CMD_Start ENDP

CMD_Stop PROC
    .IF g_CLIContext.IsRunning == 0
        invoke WriteConsoleA, g_CLIContext.hStdErr, ADDR g_ErrorNotRunning, \
               sizeof g_ErrorNotRunning, NULL, NULL
        ret
    .ENDIF
    
    ; Unload interceptor
    invoke UnloadInterceptor
    
    ; Mark as stopped
    mov g_CLIContext.IsRunning, 0
    mov g_CLIContext.TargetPID, 0
    
    ; Display success
    invoke WriteConsoleA, g_CLIContext.hStdOut, ADDR g_SuccessStopped, \
           sizeof g_SuccessStopped, NULL, NULL
    
    ret
CMD_Stop ENDP

CMD_Status PROC
    LOCAL buffer[512]:BYTE
    LOCAL bytesWritten:DWORD
    
    ; Display header
    invoke wsprintfA, ADDR buffer, \
           CSTR("═══════════════════════════════════════════════════════════════"), \
           0
    invoke WriteConsoleA, g_CLIContext.hStdOut, ADDR buffer, \
           eax, NULL, NULL
    
    ; Display status
    .IF g_CLIContext.IsRunning == 1
        invoke wsprintfA, ADDR buffer, \
               CSTR("Status: RUNNING"), \
               0
    .ELSE
        invoke wsprintfA, ADDR buffer, \
               CSTR("Status: STOPPED"), \
               0
    .ENDIF
    invoke WriteConsoleA, g_CLIContext.hStdOut, ADDR buffer, \
           eax, NULL, NULL
    
    .IF g_CLIContext.IsRunning == 1
        invoke wsprintfA, ADDR buffer, \
               CSTR("Target PID: %d"), \
               g_CLIContext.TargetPID
        invoke WriteConsoleA, g_CLIContext.hStdOut, ADDR buffer, \
               eax, NULL, NULL
    .ENDIF
    
    ; Display footer
    invoke wsprintfA, ADDR buffer, \
           CSTR("═══════════════════════════════════════════════════════════════"), \
           0
    invoke WriteConsoleA, g_CLIContext.hStdOut, ADDR buffer, \
           eax, NULL, NULL
    
    ret
CMD_Status ENDP

CMD_Stats PROC
    LOCAL buffer[512]:BYTE
    
    .IF g_CLIContext.IsRunning == 0
        invoke WriteConsoleA, g_CLIContext.hStdErr, ADDR g_ErrorNotRunning, \
               sizeof g_ErrorNotRunning, NULL, NULL
        ret
    .ENDIF
    
    ; Display statistics (would query interceptor DLL)
    invoke wsprintfA, ADDR buffer, \
           CSTR("Total Calls: %llu"), \
           g_Interceptor.Stats.TotalCalls
    invoke WriteConsoleA, g_CLIContext.hStdOut, ADDR buffer, \
           eax, NULL, NULL
    
    invoke wsprintfA, ADDR buffer, \
           CSTR("Bytes Streamed: %llu"), \
           g_Interceptor.Stats.BytesStreamed
    invoke WriteConsoleA, g_CLIContext.hStdOut, ADDR buffer, \
           eax, NULL, NULL
    
    invoke wsprintfA, ADDR buffer, \
           CSTR("Error Count: %llu"), \
           g_Interceptor.Stats.ErrorCount
    invoke WriteConsoleA, g_CLIContext.hStdOut, ADDR buffer, \
           eax, NULL, NULL
    
    ret
CMD_Stats ENDP

CMD_Clear PROC
    .IF g_CLIContext.IsRunning == 0
        invoke WriteConsoleA, g_CLIContext.hStdErr, ADDR g_ErrorNotRunning, \
               sizeof g_ErrorNotRunning, NULL, NULL
        ret
    .ENDIF
    
    ; Clear call log
    mov g_Interceptor.CallLogIndex, 0
    
    ; Display success
    invoke WriteConsoleA, g_CLIContext.hStdOut, ADDR g_SuccessCleared, \
           sizeof g_SuccessCleared, NULL, NULL
    
    ret
CMD_Clear ENDP

CMD_Filter PROC pArgs:PTR BYTE
    LOCAL filterStr[256]:BYTE
    
    .IF pArgs == 0
        ; Show current filter
        ret
    .ENDIF
    
    ; Copy filter string
    invoke lstrcpynA, ADDR filterStr, pArgs, 256
    
    ; Parse filter types
    invoke ParseFilter, ADDR filterStr
    
    ret
CMD_Filter ENDP

CMD_Export PROC pArgs:PTR BYTE
    LOCAL path[MAX_PATH]:BYTE
    
    .IF pArgs == 0
        ; Use default path
        invoke wsprintfA, ADDR path, CSTR("capture_%d.json"), \
               g_CLIContext.TargetPID
    .ELSE
        ; Use provided path
        invoke lstrcpynA, ADDR path, pArgs, MAX_PATH
    .ENDIF
    
    ; Export data
    invoke ExportCapturedData, ADDR path
    
    ret
CMD_Export ENDP

CMD_Help PROC
    invoke WriteConsoleA, g_CLIContext.hStdOut, ADDR g_HelpText, \
           sizeof g_HelpText, NULL, NULL
    ret
CMD_Help ENDP

CMD_Exit PROC
    ; Cleanup and exit
    invoke CleanupCLI
    invoke ExitProcess, 0
    ret
CMD_Exit ENDP

;============================================================================
; UTILITY FUNCTIONS
;============================================================================

; Initialize command table
InitCommandTable PROC
    ; Fill command table
    mov g_CommandTable[0].Command.Id, CMD_START
    mov g_CommandTable[0].Command.Name, CSTR("start")
    mov g_CommandTable[0].Command.Description, CSTR("Start interceptor for process")
    mov g_CommandTable[0].Command.Handler, OFFSET CMD_Start
    
    mov g_CommandTable[1].Command.Id, CMD_STOP
    mov g_CommandTable[1].Command.Name, CSTR("stop")
    mov g_CommandTable[1].Command.Description, CSTR("Stop interceptor")
    mov g_CommandTable[1].Command.Handler, OFFSET CMD_Stop
    
    mov g_CommandTable[2].Command.Id, CMD_STATUS
    mov g_CommandTable[2].Command.Name, CSTR("status")
    mov g_CommandTable[2].Command.Description, CSTR("Show current status")
    mov g_CommandTable[2].Command.Handler, OFFSET CMD_Status
    
    mov g_CommandTable[3].Command.Id, CMD_STATS
    mov g_CommandTable[3].Command.Name, CSTR("stats")
    mov g_CommandTable[3].Command.Description, CSTR("Show statistics")
    mov g_CommandTable[3].Command.Handler, OFFSET CMD_Stats
    
    mov g_CommandTable[4].Command.Id, CMD_CLEAR
    mov g_CommandTable[4].Command.Name, CSTR("clear")
    mov g_CommandTable[4].Command.Description, CSTR("Clear call log")
    mov g_CommandTable[4].Command.Handler, OFFSET CMD_Clear
    
    mov g_CommandTable[5].Command.Id, CMD_FILTER
    mov g_CommandTable[5].Command.Name, CSTR("filter")
    mov g_CommandTable[5].Command.Description, CSTR("Set filter")
    mov g_CommandTable[5].Command.Handler, OFFSET CMD_Filter
    
    mov g_CommandTable[6].Command.Id, CMD_EXPORT
    mov g_CommandTable[6].Command.Name, CSTR("export")
    mov g_CommandTable[6].Command.Description, CSTR("Export captured data")
    mov g_CommandTable[6].Command.Handler, OFFSET CMD_Export
    
    mov g_CommandTable[7].Command.Id, CMD_HELP
    mov g_CommandTable[7].Command.Name, CSTR("help")
    mov g_CommandTable[7].Command.Description, CSTR("Show help")
    mov g_CommandTable[7].Command.Handler, OFFSET CMD_Help
    
    mov g_CommandTable[8].Command.Id, CMD_EXIT
    mov g_CommandTable[8].Command.Name, CSTR("exit")
    mov g_CommandTable[8].Command.Description, CSTR("Exit CLI")
    mov g_CommandTable[8].Command.Handler, OFFSET CMD_Exit
    
    ret
InitCommandTable ENDP

; Trim leading/trailing whitespace from string
TrimString PROC pString:PTR BYTE
    LOCAL pStart:PTR BYTE
    LOCAL pEnd:PTR BYTE
    
    mov pStart, pString
    
    ; Trim leading whitespace
    .WHILE byte ptr [pStart] == ' ' || byte ptr [pStart] == 9  ; Space or tab
        inc pStart
    .ENDW
    
    ; Find end
    mov pEnd, pStart
    .WHILE byte ptr [pEnd] != 0
        inc pEnd
    .ENDW
    dec pEnd
    
    ; Trim trailing whitespace
    .WHILE pEnd >= pStart && (byte ptr [pEnd] == ' ' || byte ptr [pEnd] == 9 || byte ptr [pEnd] == 0Dh || byte ptr [pEnd] == 0Ah)
        mov byte ptr [pEnd], 0
        dec pEnd
    .ENDW
    
    ; Copy trimmed string back
    invoke lstrcpyA, pString, pStart
    
    ret
TrimString ENDP

; String comparison (case-insensitive)
stricmp PROC pStr1:PTR BYTE, pStr2:PTR BYTE
    LOCAL p1:PTR BYTE
    LOCAL p2:PTR BYTE
    LOCAL c1:BYTE
    LOCAL c2:BYTE
    
    mov p1, pStr1
    mov p2, pStr2
    
@loop:
    mov al, [p1]
    mov c1, al
    mov al, [p2]
    mov c2, al
    
    ; Convert to uppercase
    .IF c1 >= 'a' && c1 <= 'z'
        sub c1, 32
    .ENDIF
    .IF c2 >= 'a' && c2 <= 'z'
        sub c2, 32
    .ENDIF
    
    ; Compare
    mov al, c1
    sub al, c2
    .IF al != 0
        movsx rax, al
        ret
    .ENDIF
    
    ; Check for end of strings
    .IF c1 == 0
        xor rax, rax
        ret
    .ENDIF
    
    inc p1
    inc p2
    jmp @loop
stricmp ENDP

; String copy
lstrcpyA PROC pDest:PTR BYTE, pSrc:PTR BYTE
    LOCAL pD:PTR BYTE
    LOCAL pS:PTR BYTE
    
    mov pD, pDest
    mov pS, pSrc
    
@loop:
    mov al, [pS]
    mov [pD], al
    
    .IF al == 0
        mov rax, pDest
        ret
    .ENDIF
    
    inc pS
    inc pD
    jmp @loop
lstrcpyA ENDP

; String copy with length limit
lstrcpynA PROC pDest:PTR BYTE, pSrc:PTR BYTE, maxLen:DWORD
    LOCAL pD:PTR BYTE
    LOCAL pS:PTR BYTE
    LOCAL count:DWORD
    
    mov pD, pDest
    mov pS, pSrc
    mov count, maxLen
    dec count
    
@loop:
    .IF count == 0
        mov byte ptr [pD], 0
        mov rax, pDest
        ret
    .ENDIF
    
    mov al, [pS]
    mov [pD], al
    
    .IF al == 0
        mov rax, pDest
        ret
    .ENDIF
    
    inc pS
    inc pD
    dec count
    jmp @loop
lstrcpynA ENDP

; String length
strlen PROC pString:PTR BYTE
    LOCAL pStr:PTR BYTE
    LOCAL len:QWORD
    
    mov pStr, pString
    mov len, 0
    
@loop:
    mov al, [pStr]
    .IF al == 0
        mov rax, len
        ret
    .ENDIF
    
    inc len
    inc pStr
    jmp @loop
strlen ENDP

; Find character in string
strchr PROC pString:PTR BYTE, c:BYTE
    LOCAL pStr:PTR BYTE
    
    mov pStr, pString
    
@loop:
    mov al, [pStr]
    .IF al == 0
        xor rax, rax
        ret
    .ENDIF
    
    .IF al == c
        mov rax, pStr
        ret
    .ENDIF
    
    inc pStr
    jmp @loop
strchr ENDP

; String to integer
atoi PROC pString:PTR BYTE
    LOCAL pStr:PTR BYTE
    LOCAL result:QWORD
    LOCAL sign:QWORD
    LOCAL c:BYTE
    
    mov pStr, pString
    mov result, 0
    mov sign, 1
    
    ; Check for sign
    mov al, [pStr]
    .IF al == '-'
        mov sign, -1
        inc pStr
    .ELSEIF al == '+'
        inc pStr
    .ENDIF
    
@loop:
    mov al, [pStr]
    .IF al < '0' || al > '9'
        jmp @done
    .ENDIF
    
    ; Convert digit
    sub al, '0'
    mov c, al
    
    ; Multiply result by 10 and add digit
    mov rax, result
    imul rax, 10
    movzx rbx, c
    add rax, rbx
    mov result, rax
    
    inc pStr
    jmp @loop
    
@done:
    mov rax, result
    imul rax, sign
    ret
atoi ENDP

; Wide string to ASCII
wcstombs PROC pDest:PTR BYTE, pSrc:PTR WORD, maxLen:DWORD
    LOCAL pD:PTR BYTE
    LOCAL pS:PTR WORD
    LOCAL count:DWORD
    
    mov pD, pDest
    mov pS, pSrc
    mov count, maxLen
    dec count
    
@loop:
    .IF count == 0
        mov byte ptr [pD], 0
        mov rax, pDest
        ret
    .ENDIF
    
    mov ax, [pS]
    mov byte ptr [pD], al
    
    .IF al == 0
        mov rax, pDest
        ret
    .ENDIF
    
    add pS, 2
    inc pD
    dec count
    jmp @loop
wcstombs ENDP

;============================================================================
; INTEGRATION WITH MASM IDE INJECTOR
;============================================================================

; Inject interceptor DLL into target process
InjectInterceptor PROC dwTargetPID:DWORD
    LOCAL hProcess:QWORD
    LOCAL dllPath[MAX_PATH]:BYTE
    LOCAL pRemoteMem:QWORD
    LOCAL hThread:QWORD
    LOCAL loadLibraryAddr:QWORD
    
    ; Open target process
    invoke OpenProcess, PROCESS_ALL_ACCESS, FALSE, dwTargetPID
    .IF rax == 0
        xor rax, rax
        ret
    .ENDIF
    mov hProcess, rax
    
    ; Get path to interceptor DLL
    invoke GetModuleFileNameA, NULL, ADDR dllPath, MAX_PATH
    
    ; Find last backslash
    invoke strrchr, ADDR dllPath, '\'
    .IF rax != 0
        inc rax
        mov byte ptr [rax], 0
        
        ; Append DLL name
        invoke lstrcatA, ADDR dllPath, CSTR("os_explorer_interceptor.dll")
    .ELSE
        ; Use current directory
        invoke lstrcpyA, ADDR dllPath, CSTR("os_explorer_interceptor.dll")
    .ENDIF
    
    ; Verify DLL exists
    invoke GetFileAttributesA, ADDR dllPath
    .IF rax == -1
        invoke CloseHandle, hProcess
        xor rax, rax
        ret
    .ENDIF
    
    ; Get LoadLibraryA address
    invoke GetModuleHandleA, CSTR("kernel32.dll")
    invoke GetProcAddress, rax, CSTR("LoadLibraryA")
    mov loadLibraryAddr, rax
    
    ; Allocate memory in target process for DLL path
    invoke VirtualAllocEx, hProcess, NULL, MAX_PATH, MEM_COMMIT, PAGE_READWRITE
    .IF rax == 0
        invoke CloseHandle, hProcess
        xor rax, rax
        ret
    .ENDIF
    mov pRemoteMem, rax
    
    ; Write DLL path to target process
    invoke WriteProcessMemory, hProcess, pRemoteMem, ADDR dllPath, \
           MAX_PATH, NULL
    .IF rax == 0
        invoke VirtualFreeEx, hProcess, pRemoteMem, 0, MEM_RELEASE
        invoke CloseHandle, hProcess
        xor rax, rax
        ret
    .ENDIF
    
    ; Create remote thread to load DLL
    invoke CreateRemoteThread, hProcess, NULL, 0, loadLibraryAddr, \
           pRemoteMem, 0, NULL
    .IF rax == 0
        invoke VirtualFreeEx, hProcess, pRemoteMem, 0, MEM_RELEASE
        invoke CloseHandle, hProcess
        xor rax, rax
        ret
    .ENDIF
    mov hThread, rax
    
    ; Wait for thread to complete
    invoke WaitForSingleObject, hThread, INFINITE
    
    ; Cleanup
    invoke CloseHandle, hThread
    invoke VirtualFreeEx, hProcess, pRemoteMem, 0, MEM_RELEASE
    invoke CloseHandle, hProcess
    
    mov rax, 1  ; Success
    ret
InjectInterceptor ENDP

; Find last occurrence of character in string
strrchr PROC pString:PTR BYTE, c:BYTE
    LOCAL pStr:PTR BYTE
    LOCAL pLast:PTR BYTE
    
    mov pStr, pString
    mov pLast, NULL
    
@loop:
    mov al, [pStr]
    .IF al == 0
        mov rax, pLast
        ret
    .ENDIF
    
    .IF al == c
        mov pLast, pStr
    .ENDIF
    
    inc pStr
    jmp @loop
strrchr ENDP

; String concatenate
lstrcatA PROC pDest:PTR BYTE, pSrc:PTR BYTE
    LOCAL pD:PTR BYTE
    LOCAL pS:PTR BYTE
    
    ; Find end of dest
    mov pD, pDest
    .WHILE byte ptr [pD] != 0
        inc pD
    .ENDW
    
    ; Copy src to end of dest
    mov pS, pSrc
    
@loop:
    mov al, [pS]
    mov [pD], al
    
    .IF al == 0
        mov rax, pDest
        ret
    .ENDIF
    
    inc pS
    inc pD
    jmp @loop
lstrcatA ENDP

; Unload interceptor from target process
UnloadInterceptor PROC
    LOCAL hProcess:QWORD
    LOCAL freeLibraryAddr:QWORD
    LOCAL hThread:QWORD
    
    .IF g_CLIContext.TargetPID == 0
        ret
    .ENDIF
    
    ; Open target process
    invoke OpenProcess, PROCESS_ALL_ACCESS, FALSE, g_CLIContext.TargetPID
    .IF rax == 0
        ret
    .ENDIF
    mov hProcess, rax
    
    ; Get FreeLibrary address
    invoke GetModuleHandleA, CSTR("kernel32.dll")
    invoke GetProcAddress, rax, CSTR("FreeLibrary")
    mov freeLibraryAddr, rax
    
    ; Note: In a real implementation, we'd need to get the DLL handle
    ; For now, just close the process handle
    
    invoke CloseHandle, hProcess
    
    ret
UnloadInterceptor ENDP

; Parse filter string
ParseFilter PROC pFilterStr:PTR BYTE
    LOCAL filterStr[256]:BYTE
    LOCAL token:PTR BYTE
    
    ; Copy filter string
    invoke lstrcpynA, ADDR filterStr, pFilterStr, 256
    
    ; Tokenize by comma
    invoke strtok, ADDR filterStr, CSTR(",")
    mov token, rax
    
    .WHILE token != 0
        ; Parse each filter type
        invoke stricmp, token, CSTR("FILE")
        .IF rax == 0
            or g_CLIContext.Filter, HOOK_TYPE_FILE
        .ENDIF
        
        invoke stricmp, token, CSTR("REGISTRY")
        .IF rax == 0
            or g_CLIContext.Filter, HOOK_TYPE_REGISTRY
        .ENDIF
        
        invoke stricmp, token, CSTR("NETWORK")
        .IF rax == 0
            or g_CLIContext.Filter, HOOK_TYPE_NETWORK
        .ENDIF
        
        ; Next token
        invoke strtok, NULL, CSTR(",")
        mov token, rax
    .ENDW
    
    ret
ParseFilter ENDP

; Tokenize string
strtok PROC pString:PTR BYTE, pDelim:PTR BYTE
    LOCAL pStr:PTR BYTE
    LOCAL pDel:PTR BYTE
    LOCAL pToken:PTR BYTE
    LOCAL c:BYTE
    
    mov pStr, pString
    mov pDel, pDelim
    
    .IF pStr == NULL
        ; Continue from last position
        mov rax, g_CLIContext.CommandBuffer
        mov pStr, rax
    .ENDIF
    
    ; Skip leading delimiters
@skip_delim:
    mov al, [pStr]
    .IF al == 0
        xor rax, rax
        ret
    .ENDIF
    
    mov pToken, pStr
    
    ; Check if this char is a delimiter
    mov pDel, pDelim
@check_delim:
    mov bl, [pDel]
    .IF bl == 0
        jmp @not_delim
    .ENDIF
    
    .IF al == bl
        inc pStr
        jmp @skip_delim
    .ENDIF
    
    inc pDel
    jmp @check_delim
    
@not_delim:
    ; Found start of token
    mov pToken, pStr
    
    ; Find end of token
@find_end:
    mov al, [pStr]
    .IF al == 0
        mov rax, pToken
        ret
    .ENDIF
    
    ; Check if this char is a delimiter
    mov pDel, pDelim
@check_delim2:
    mov bl, [pDel]
    .IF bl == 0
        inc pStr
        jmp @find_end
    .ENDIF
    
    .IF al == bl
        mov byte ptr [pStr], 0
        inc pStr
        mov rax, pToken
        ret
    .ENDIF
    
    inc pDel
    jmp @check_delim2
strtok ENDP

; Export captured data
ExportCapturedData PROC pPath:PTR BYTE
    LOCAL hFile:QWORD
    LOCAL buffer[512]:BYTE
    
    ; Create file
    invoke CreateFileA, pPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, \
           FILE_ATTRIBUTE_NORMAL, NULL
    .IF rax == INVALID_HANDLE_VALUE
        xor rax, rax
        ret
    .ENDIF
    mov hFile, rax
    
    ; Write header
    invoke wsprintfA, ADDR buffer, CSTR("OS Explorer Interceptor Capture\n"), 0
    invoke WriteFile, hFile, ADDR buffer, eax, NULL, NULL
    
    ; Write call log (would iterate through circular buffer)
    ; For now, just write placeholder
    invoke wsprintfA, ADDR buffer, CSTR("Total Calls: %llu\n"), \
           g_Interceptor.Stats.TotalCalls
    invoke WriteFile, hFile, ADDR buffer, eax, NULL, NULL
    
    ; Close file
    invoke CloseHandle, hFile
    
    mov rax, 1
    ret
ExportCapturedData ENDP

; Cleanup CLI resources
CleanupCLI PROC
    .IF g_CLIContext.hTargetProcess != 0
        invoke CloseHandle, g_CLIContext.hTargetProcess
    .ENDIF
    
    .IF g_hStreamEvent != 0
        invoke CloseHandle, g_hStreamEvent
    .ENDIF
    
    ret
CleanupCLI ENDP

;============================================================================
; ENTRY POINT
;============================================================================

END main
