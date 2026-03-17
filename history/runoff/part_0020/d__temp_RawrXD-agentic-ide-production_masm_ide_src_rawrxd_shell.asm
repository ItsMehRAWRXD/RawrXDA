;======================================================================
; RawrXD IDE - Embedded Command Shell
; PowerShell (pwsh.exe) + Command Prompt (cmd.exe) integration
;======================================================================
INCLUDE rawrxd_includes.inc

.DATA
; Shell state
g_hShellPanel           DQ ?
g_hShellOutput          DQ ?
g_hShellInput           DQ ?
g_hShellProcess         DQ ?
g_hShellThread          DQ ?
g_hStdoutRead           DQ ?
g_hStdoutWrite          DQ ?
g_hStdinRead            DQ ?
g_hStdinWrite           DQ ?
g_currentShellMode      DQ 0  ; 0=PowerShell, 1=CMD
g_shellPrompt[256]      DB 256 DUP(0)
g_commandHistory[50*256] DB 50*256 DUP(0)
g_historyCount          DQ 0
g_historyIndex          DQ 0
g_isShellRunning        DQ 0

; Process info
SHELL_PROCESS STRUCT
    hProcess            DQ ?
    hThread             DQ ?
    dwProcessId         DQ ?
    dwThreadId          DQ ?
SHELL_PROCESS ENDS

g_shellProc             SHELL_PROCESS {}

.CODE

;----------------------------------------------------------------------
; RawrXD_Shell_Create - Create embedded shell panel
;----------------------------------------------------------------------
RawrXD_Shell_Create PROC hParent:QWORD, x:QWORD, y:QWORD, cx:QWORD, cy:QWORD
    LOCAL si:STARTUPINFOA
    LOCAL pi:PROCESS_INFORMATION
    LOCAL hFont:QWORD
    LOCAL logFont:LOGFONTA
    
    ; Create shell panel container
    INVOKE CreateWindowEx,
        WS_EX_CLIENTEDGE,
        "STATIC",
        NULL,
        WS_CHILD OR WS_VISIBLE,
        x, y, cx, cy,
        hParent, NULL, GetModuleHandle(NULL), NULL
    
    mov g_hShellPanel, rax
    
    ; Create toolbar for shell controls (PowerShell/CMD toggle, Clear, etc)
    INVOKE CreateWindowEx,
        0,
        "ToolbarWindow32",
        NULL,
        WS_CHILD OR WS_VISIBLE,
        x, y, cx, 25,
        hParent, NULL, GetModuleHandle(NULL), NULL
    
    ; Create output rich edit control
    INVOKE CreateWindowEx,
        WS_EX_CLIENTEDGE,
        "RICHEDIT50W",
        NULL,
        WS_CHILD OR WS_VISIBLE OR ES_MULTILINE OR ES_READONLY OR WS_VSCROLL,
        x, y + 25, cx, cy - 60,
        hParent, NULL, GetModuleHandle(NULL), NULL
    
    mov g_hShellOutput, rax
    
    ; Create input edit control
    INVOKE CreateWindowEx,
        WS_EX_CLIENTEDGE,
        "EDIT",
        NULL,
        WS_CHILD OR WS_VISIBLE OR ES_AUTOHSCROLL,
        x, y + cy - 30, cx, 25,
        hParent, NULL, GetModuleHandle(NULL), NULL
    
    mov g_hShellInput, rax
    
    ; Set font
    INVOKE RtlZeroMemory, ADDR logFont, SIZEOF LOGFONTA
    mov logFont.lfHeight, -12
    mov logFont.lfWeight, FW_NORMAL
    INVOKE lstrcpyA, ADDR logFont.lfFaceName, OFFSET szConsoleFont
    
    INVOKE CreateFontIndirectA, ADDR logFont
    mov hFont, rax
    
    INVOKE SendMessage, g_hShellOutput, WM_SETFONT, hFont, TRUE
    INVOKE SendMessage, g_hShellInput, WM_SETFONT, hFont, TRUE
    
    ; Start PowerShell by default
    INVOKE RawrXD_Shell_StartPowerShell
    
    xor eax, eax
    ret
    
RawrXD_Shell_Create ENDP

;----------------------------------------------------------------------
; RawrXD_Shell_StartPowerShell - Launch PowerShell
;----------------------------------------------------------------------
RawrXD_Shell_StartPowerShell PROC
    LOCAL si:STARTUPINFOA
    LOCAL pi:PROCESS_INFORMATION
    
    mov g_currentShellMode, 0
    
    ; Initialize startup info
    INVOKE RtlZeroMemory, ADDR si, SIZEOF STARTUPINFOA
    mov si.cb, SIZEOF STARTUPINFOA
    mov si.dwFlags, STARTF_USESTDHANDLES or STARTF_USESHOWWINDOW
    mov si.wShowWindow, SW_HIDE
    
    ; Create pipes for stdout/stderr/stdin
    INVOKE CreatePipe, ADDR g_hStdoutRead, ADDR g_hStdoutWrite, NULL, 0
    INVOKE CreatePipe, ADDR g_hStdinRead, ADDR g_hStdinWrite, NULL, 0
    
    mov si.hStdInput, g_hStdinRead
    mov si.hStdOutput, g_hStdoutWrite
    mov si.hStdError, g_hStdoutWrite
    
    ; Create PowerShell process
    INVOKE CreateProcessA,
        OFFSET szPowerShell,
        OFFSET szPowerShellArgs,
        NULL, NULL,
        TRUE,  ; bInheritHandles
        0,
        NULL,
        ADDR g_currentPath,
        ADDR si,
        ADDR pi
    
    test eax, eax
    jz @@error
    
    mov g_hShellProcess, pi.hProcess
    mov g_hShellThread, pi.hThread
    mov g_shellProc.hProcess, pi.hProcess
    mov g_shellProc.hThread, pi.hThread
    mov g_shellProc.dwProcessId, pi.dwProcessId
    mov g_shellProc.dwThreadId, pi.dwThreadId
    
    mov g_isShellRunning, 1
    
    ; Close unused pipe handles
    INVOKE CloseHandle, g_hStdoutWrite
    INVOKE CloseHandle, g_hStdinRead
    
    ; Start output monitoring thread
    INVOKE CreateThread, NULL, 0, OFFSET RawrXD_Shell_OutputMonitor, 0, 0, NULL
    
    ; Display PowerShell banner
    INVOKE RawrXD_Shell_OutputMessage, OFFSET szPowerShellBanner, 1
    
    xor eax, eax
    ret
    
@@error:
    INVOKE RawrXD_Output_Error, OFFSET szFailedStartPS
    mov eax, -1
    ret
    
RawrXD_Shell_StartPowerShell ENDP

;----------------------------------------------------------------------
; RawrXD_Shell_StartCMD - Launch Command Prompt
;----------------------------------------------------------------------
RawrXD_Shell_StartCMD PROC
    LOCAL si:STARTUPINFOA
    LOCAL pi:PROCESS_INFORMATION
    
    ; Terminate previous shell if running
    cmp g_isShellRunning, 1
    jne @@proceed
    
    INVOKE RawrXD_Shell_Terminate
    
@@proceed:
    mov g_currentShellMode, 1
    
    ; Initialize startup info
    INVOKE RtlZeroMemory, ADDR si, SIZEOF STARTUPINFOA
    mov si.cb, SIZEOF STARTUPINFOA
    mov si.dwFlags, STARTF_USESTDHANDLES or STARTF_USESHOWWINDOW
    mov si.wShowWindow, SW_HIDE
    
    ; Create pipes
    INVOKE CreatePipe, ADDR g_hStdoutRead, ADDR g_hStdoutWrite, NULL, 0
    INVOKE CreatePipe, ADDR g_hStdinRead, ADDR g_hStdinWrite, NULL, 0
    
    mov si.hStdInput, g_hStdinRead
    mov si.hStdOutput, g_hStdoutWrite
    mov si.hStdError, g_hStdoutWrite
    
    ; Create CMD process
    INVOKE CreateProcessA,
        OFFSET szCmd,
        OFFSET szCmdArgs,
        NULL, NULL,
        TRUE,
        0,
        NULL,
        ADDR g_currentPath,
        ADDR si,
        ADDR pi
    
    test eax, eax
    jz @@error
    
    mov g_hShellProcess, pi.hProcess
    mov g_hShellThread, pi.hThread
    mov g_shellProc.hProcess, pi.hProcess
    mov g_shellProc.hThread, pi.hThread
    mov g_shellProc.dwProcessId, pi.dwProcessId
    mov g_shellProc.dwThreadId, pi.dwThreadId
    
    mov g_isShellRunning, 1
    
    ; Close unused handles
    INVOKE CloseHandle, g_hStdoutWrite
    INVOKE CloseHandle, g_hStdinRead
    
    ; Start output monitoring
    INVOKE CreateThread, NULL, 0, OFFSET RawrXD_Shell_OutputMonitor, 0, 0, NULL
    
    ; Display CMD banner
    INVOKE RawrXD_Shell_OutputMessage, OFFSET szCMDBanner, 1
    
    xor eax, eax
    ret
    
@@error:
    INVOKE RawrXD_Output_Error, OFFSET szFailedStartCMD
    mov eax, -1
    ret
    
RawrXD_Shell_StartCMD ENDP

;----------------------------------------------------------------------
; RawrXD_Shell_OutputMonitor - Thread to monitor shell output
;----------------------------------------------------------------------
RawrXD_Shell_OutputMonitor PROC lpParam:QWORD
    LOCAL szBuffer[8192]:BYTE
    LOCAL dwRead:QWORD
    LOCAL hHeap:QWORD
    
@@read_loop:
    ; Check if process still running
    INVOKE WaitForSingleObject, g_hShellProcess, 0
    cmp eax, WAIT_OBJECT_0
    je @@done
    
    ; Read from pipe
    INVOKE ReadFile, g_hStdoutRead, ADDR szBuffer, 8192, ADDR dwRead, NULL
    test eax, eax
    jz @@read_loop
    
    ; Null-terminate output
    mov eax, dwRead
    mov byte [ADDR szBuffer + rax], 0
    
    ; Display output (append to rich edit)
    INVOKE RawrXD_Shell_AppendOutput, ADDR szBuffer, dwRead
    
    jmp @@read_loop
    
@@done:
    mov g_isShellRunning, 0
    INVOKE ExitThread, 0
    ret
    
RawrXD_Shell_OutputMonitor ENDP

;----------------------------------------------------------------------
; RawrXD_Shell_AppendOutput - Append text to output control
;----------------------------------------------------------------------
RawrXD_Shell_AppendOutput PROC pszText:QWORD, dwLen:QWORD
    LOCAL charRange:CHARRANGE
    LOCAL cf:CHARFORMATA
    LOCAL cr:COLORREF
    
    ; Move to end
    INVOKE SendMessage, g_hShellOutput, EM_EXGETSEL, 0, ADDR charRange
    mov charRange.cpMin, -1
    mov charRange.cpMax, -1
    INVOKE SendMessage, g_hShellOutput, EM_EXSETSEL, 0, ADDR charRange
    
    ; Set color based on output type
    ; Check for error keywords
    INVOKE RawrXD_Util_StrStrA, pszText, OFFSET szErrorKeyword
    test eax, eax
    jz @@not_error
    mov cr, 0x0000FF  ; Red
    jmp @@set_format
    
@@not_error:
    mov cr, 0x00FF00  ; Green
    
@@set_format:
    INVOKE RtlZeroMemory, ADDR cf, SIZEOF CHARFORMATA
    mov cf.cbSize, SIZEOF CHARFORMATA
    mov cf.dwMask, CFM_COLOR
    mov cf.crTextColor, cr
    
    INVOKE SendMessage, g_hShellOutput, EM_SETCHARFORMAT, SCF_SELECTION, ADDR cf
    
    ; Insert text
    INVOKE SendMessage, g_hShellOutput, EM_REPLACESEL, FALSE, pszText
    
    ; Auto-scroll to end
    INVOKE SendMessage, g_hShellOutput, EM_SCROLL, SB_BOTTOM, 0
    
    ret
    
RawrXD_Shell_AppendOutput ENDP

;----------------------------------------------------------------------
; RawrXD_Shell_ExecuteCommand - Execute shell command
;----------------------------------------------------------------------
RawrXD_Shell_ExecuteCommand PROC pszCommand:QWORD
    LOCAL szFull[1024]:BYTE
    LOCAL dwWritten:QWORD
    
    cmp g_isShellRunning, 0
    je @@not_running
    
    ; Add to history
    INVOKE RawrXD_Shell_AddHistory, pszCommand
    
    ; Build command with newline
    INVOKE lstrcpyA, ADDR szFull, pszCommand
    INVOKE lstrcatA, ADDR szFull, OFFSET szNewline
    
    ; Write to stdin
    INVOKE WriteFile, g_hStdinWrite, ADDR szFull, SIZEOF szFull, ADDR dwWritten, NULL
    
    ; Clear input
    INVOKE SetWindowTextA, g_hShellInput, OFFSET szEmpty
    
    xor eax, eax
    ret
    
@@not_running:
    INVOKE RawrXD_Output_Error, OFFSET szShellNotRunning
    mov eax, -1
    ret
    
RawrXD_Shell_ExecuteCommand ENDP

;----------------------------------------------------------------------
; RawrXD_Shell_AddHistory - Add command to history
;----------------------------------------------------------------------
RawrXD_Shell_AddHistory PROC pszCommand:QWORD
    LOCAL idx:QWORD
    
    cmp g_historyCount, 50
    jge @@shift_history
    
    ; Add to history
    mov idx, g_historyCount
    imul idx, 256
    INVOKE lstrcpyA, OFFSET g_commandHistory + idx, pszCommand
    inc g_historyCount
    mov g_historyIndex, g_historyCount
    
    xor eax, eax
    ret
    
@@shift_history:
    ; Shift history up
    mov rcx, 0
@@shift_loop:
    cmp rcx, 49
    jge @@shift_done
    
    ; Copy history[rcx+1] to history[rcx]
    mov rdx, rcx
    imul rdx, 256
    mov rax, rcx
    add rax, 1
    imul rax, 256
    
    INVOKE lstrcpyA,
        OFFSET g_commandHistory + rdx,
        OFFSET g_commandHistory + rax
    
    inc rcx
    jmp @@shift_loop
    
@@shift_done:
    ; Add new command at end
    mov idx, 49
    imul idx, 256
    INVOKE lstrcpyA, OFFSET g_commandHistory + idx, pszCommand
    mov g_historyIndex, 50
    
    xor eax, eax
    ret
    
RawrXD_Shell_AddHistory ENDP

;----------------------------------------------------------------------
; RawrXD_Shell_HistoryUp - Cycle up in history
;----------------------------------------------------------------------
RawrXD_Shell_HistoryUp PROC
    cmp g_historyIndex, 0
    je @@done
    
    dec g_historyIndex
    mov rax, g_historyIndex
    imul rax, 256
    
    INVOKE SetWindowTextA, g_hShellInput, OFFSET g_commandHistory + rax
    
    ; Move cursor to end
    mov rcx, -1
    INVOKE SendMessage, g_hShellInput, EM_SETSEL, rcx, rcx
    
@@done:
    ret
    
RawrXD_Shell_HistoryUp ENDP

;----------------------------------------------------------------------
; RawrXD_Shell_HistoryDown - Cycle down in history
;----------------------------------------------------------------------
RawrXD_Shell_HistoryDown PROC
    cmp g_historyIndex, g_historyCount
    jge @@done
    
    inc g_historyIndex
    mov rax, g_historyIndex
    imul rax, 256
    
    INVOKE SetWindowTextA, g_hShellInput, OFFSET g_commandHistory + rax
    
@@done:
    ret
    
RawrXD_Shell_HistoryDown ENDP

;----------------------------------------------------------------------
; RawrXD_Shell_ClearOutput - Clear output display
;----------------------------------------------------------------------
RawrXD_Shell_ClearOutput PROC
    INVOKE SetWindowTextA, g_hShellOutput, OFFSET szEmpty
    ret
RawrXD_Shell_ClearOutput ENDP

;----------------------------------------------------------------------
; RawrXD_Shell_Terminate - Terminate active shell
;----------------------------------------------------------------------
RawrXD_Shell_Terminate PROC
    cmp g_isShellRunning, 0
    je @@done
    
    ; Close pipes
    INVOKE CloseHandle, g_hStdoutRead
    INVOKE CloseHandle, g_hStdinWrite
    
    ; Terminate process
    INVOKE TerminateProcess, g_hShellProcess, 0
    INVOKE WaitForSingleObject, g_hShellProcess, 5000
    
    ; Close process/thread handles
    INVOKE CloseHandle, g_hShellProcess
    INVOKE CloseHandle, g_hShellThread
    
    mov g_isShellRunning, 0
    
@@done:
    xor eax, eax
    ret
    
RawrXD_Shell_Terminate ENDP

;----------------------------------------------------------------------
; RawrXD_Shell_OutputMessage - Output message to shell display
;----------------------------------------------------------------------
RawrXD_Shell_OutputMessage PROC pszMessage:QWORD, colorType:QWORD
    LOCAL cf:CHARFORMATA
    LOCAL cr:COLORREF
    
    ; Determine color
    mov eax, colorType
    cmp eax, 0
    je @@normal
    cmp eax, 1
    je @@info
    cmp eax, 2
    je @@warning
    cmp eax, 3
    je @@error_color
    
@@normal:
    mov cr, 0xC0C0C0  ; Gray
    jmp @@append
@@info:
    mov cr, 0xFFFF00  ; Cyan
    jmp @@append
@@warning:
    mov cr, 0x00FF00  ; Yellow
    jmp @@append
@@error_color:
    mov cr, 0x0000FF  ; Red
    
@@append:
    INVOKE RtlZeroMemory, ADDR cf, SIZEOF CHARFORMATA
    mov cf.cbSize, SIZEOF CHARFORMATA
    mov cf.dwMask, CFM_COLOR
    mov cf.crTextColor, cr
    
    INVOKE SendMessage, g_hShellOutput, EM_SETCHARFORMAT, SCF_SELECTION, ADDR cf
    INVOKE SendMessage, g_hShellOutput, EM_REPLACESEL, FALSE, pszMessage
    INVOKE SendMessage, g_hShellOutput, EM_REPLACESEL, FALSE, OFFSET szNewline
    
    ret
    
RawrXD_Shell_OutputMessage ENDP

;----------------------------------------------------------------------
; RawrXD_Shell_GetWorkingDirectory - Get shell working directory
;----------------------------------------------------------------------
RawrXD_Shell_GetWorkingDirectory PROC pszBuffer:QWORD
    INVOKE lstrcpyA, pszBuffer, ADDR g_currentPath
    ret
RawrXD_Shell_GetWorkingDirectory ENDP

;----------------------------------------------------------------------
; RawrXD_Shell_SetWorkingDirectory - Set shell working directory
;----------------------------------------------------------------------
RawrXD_Shell_SetWorkingDirectory PROC pszPath:QWORD
    LOCAL szCmd[512]:BYTE
    
    INVOKE lstrcpyA, ADDR g_currentPath, pszPath
    
    ; Change directory in shell
    cmp g_currentShellMode, 0
    je @@powershell
    
    ; CMD: cd /d C:\path
    INVOKE lstrcpyA, ADDR szCmd, OFFSET szCDCmd
    INVOKE lstrcatA, ADDR szCmd, pszPath
    jmp @@execute
    
@@powershell:
    ; PowerShell: cd C:\path or Set-Location
    INVOKE lstrcpyA, ADDR szCmd, OFFSET szSetLocation
    INVOKE lstrcatA, ADDR szCmd, pszPath
    
@@execute:
    INVOKE RawrXD_Shell_ExecuteCommand, ADDR szCmd
    ret
    
RawrXD_Shell_SetWorkingDirectory ENDP

; String literals
szPowerShell            DB "pwsh.exe", 0
szPowerShellArgs        DB "-NoExit -NoProfile", 0
szCmd                   DB "cmd.exe", 0
szCmdArgs               DB "/k", 0
szConsoleFont           DB "Consolas", 0
szPowerShellBanner      DB "Windows PowerShell", 13, 10, "Copyright (C) Microsoft Corporation", 13, 10, 0
szCMDBanner             DB "Microsoft Windows", 13, 10, "Copyright (C) 2024 Microsoft Corporation", 13, 10, 0
szFailedStartPS         DB "Failed to start PowerShell", 0
szFailedStartCMD        DB "Failed to start Command Prompt", 0
szShellNotRunning       DB "Shell is not running", 0
szErrorKeyword          DB "error", 0
szNewline               DB 13, 10, 0
szEmpty                 DB "", 0
szCDCmd                 DB "cd /d ", 0
szSetLocation           DB "Set-Location ", 0

END
