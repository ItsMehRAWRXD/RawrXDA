; ============================================================================
; RawrXD Agentic IDE - Terminal Implementation (Pure MASM)
; Embedded PowerShell/Command Prompt
; ============================================================================

.686
.model flat, stdcall
option casemap:none

include ..\include\winapi_min.inc

 .data
include constants.inc
include structures.inc
include macros.inc

; ============================================================================
; DATA SECTION
; ============================================================================

.data
    szTerminalClass   db "EDIT", 0
    szTerminalPrompt  db "C:\> ", 0
    szCmdExe          db "cmd.exe", 0
    szPwshExe         db "pwsh.exe", 0
    szNewLine         db 13, 10, 0
    
    ; Terminal state
    g_hTerminal       dd 0
    g_hTerminalPipe   dd 0
    g_hTerminalThread dd 0
    g_bTerminalRunning dd 0

.data?
    g_szTerminalBuffer db 4096 dup(?)

; ============================================================================
; PROCEDURES
; ============================================================================

; ============================================================================
; CreateTerminal - Create terminal control
; Returns: Terminal handle in eax
; ============================================================================
CreateTerminal proc
    LOCAL dwStyle:DWORD
    LOCAL hTerminal:DWORD
    
    ; Create edit control for terminal
    mov dwStyle, WS_CHILD or WS_VISIBLE or WS_BORDER or ES_MULTILINE or ES_AUTOVSCROLL or ES_READONLY
    
    CreateWnd szTerminalClass, NULL, dwStyle, 0, 0, 400, 200, hMainWindow, IDC_TERMINAL
    mov hTerminal, eax
    mov g_hTerminal, eax
    
    .if eax == 0
        xor eax, eax
        ret
    .endif
    
    ; Set font
    invoke SendMessage, hTerminal, WM_SETFONT, hMonoFont, TRUE
    
    ; Set background color
    invoke SendMessage, hTerminal, EM_SETBKGNDCOLOR, 0, 00000000h  ; Black
    
    ; Set text color
    invoke SendMessage, hTerminal, EM_SETTEXTCOLOR, 0, 00FFFFFFh  ; White
    
    ; Initialize terminal
    call Terminal_Initialize
    
    mov eax, hTerminal
    ret
CreateTerminal endp

; ============================================================================
; Terminal_Initialize - Initialize terminal with prompt
; ============================================================================
Terminal_Initialize proc
    invoke SendMessage, g_hTerminal, WM_SETTEXT, 0, addr szTerminalPrompt
    ret
Terminal_Initialize endp

; ============================================================================
; Terminal_ExecuteCommand - Execute command in terminal
; Input: pszCommand
; ============================================================================
Terminal_ExecuteCommand proc pszCommand:DWORD
    LOCAL si:STARTUPINFO
    LOCAL pi:PROCESS_INFORMATION
    LOCAL hReadPipe:DWORD
    LOCAL hWritePipe:DWORD
    LOCAL sa:SECURITY_ATTRIBUTES
    LOCAL dwBytesRead:DWORD
    LOCAL chBuffer db 1024 dup(0)
    LOCAL szFullCommand db 512 dup(0)
    
    ; Create pipes for I/O
    mov sa.nLength, sizeof SECURITY_ATTRIBUTES
    mov sa.lpSecurityDescriptor, NULL
    mov sa.bInheritHandle, TRUE
    
    invoke CreatePipe, addr hReadPipe, addr hWritePipe, addr sa, 0
    .if eax == 0
        ret
    .endif
    
    ; Set up startup info
    mov si.cb, sizeof STARTUPINFO
    mov si.lpReserved, NULL
    mov si.lpDesktop, NULL
    mov si.lpTitle, NULL
    mov si.dwX, 0
    mov si.dwY, 0
    mov si.dwXSize, 0
    mov si.dwYSize, 0
    mov si.dwXCountChars, 0
    mov si.dwYCountChars, 0
    mov si.dwFillAttribute, 0
    mov si.dwFlags, STARTF_USESTDHANDLES
    mov si.wShowWindow, SW_HIDE
    mov si.cbReserved2, 0
    mov si.lpReserved2, NULL
    mov si.hStdInput, NULL
    mov si.hStdOutput, hWritePipe
    mov si.hStdError, hWritePipe
    
    ; Build full command
    szCopy addr szFullCommand, pszCommand
    szCat addr szFullCommand, " 2>&1"  ; Redirect stderr to stdout
    
    ; Create process
    invoke CreateProcess, NULL, addr szFullCommand, NULL, NULL, TRUE, 
        CREATE_NO_WINDOW, NULL, NULL, addr si, addr pi
    
    .if eax == 0
        invoke CloseHandle, hReadPipe
        invoke CloseHandle, hWritePipe
        ret
    .endif
    
    ; Close write pipe (child process has it)
    invoke CloseHandle, hWritePipe
    
    ; Read output
    @@ReadLoop:
        invoke ReadFile, hReadPipe, addr chBuffer, sizeof chBuffer, addr dwBytesRead, NULL
        .if eax == 0 || dwBytesRead == 0
            jmp @ReadDone
        .endif
        
        ; Append to terminal
        invoke Terminal_AppendText, addr chBuffer
        jmp @ReadLoop
    
    @ReadDone:
    
    ; Wait for process to complete
    invoke WaitForSingleObject, pi.hProcess, INFINITE
    
    ; Cleanup
    invoke CloseHandle, pi.hProcess
    invoke CloseHandle, pi.hThread
    invoke CloseHandle, hReadPipe
    
    ; Add new prompt
    invoke Terminal_AppendText, addr szNewLine
    invoke Terminal_AppendText, addr szTerminalPrompt
    
    ret
Terminal_ExecuteCommand endp

; ============================================================================
; Terminal_AppendText - Append text to terminal
; Input: pszText
; ============================================================================
Terminal_AppendText proc pszText:DWORD
    LOCAL dwTextLength:DWORD
    LOCAL dwCurrentLength:DWORD
    
    ; Get current text length
    invoke SendMessage, g_hTerminal, WM_GETTEXTLENGTH, 0, 0
    mov dwCurrentLength, eax
    
    ; Get text to append length
    szLen pszText
    mov dwTextLength, eax
    
    ; Append text
    invoke SendMessage, g_hTerminal, EM_SETSEL, dwCurrentLength, dwCurrentLength
    invoke SendMessage, g_hTerminal, EM_REPLACESEL, FALSE, pszText
    
    ; Scroll to bottom
    invoke SendMessage, g_hTerminal, EM_SCROLL, SB_BOTTOM, 0
    
    ret
Terminal_AppendText endp

; ============================================================================
; Terminal_Clear - Clear terminal
; ============================================================================
Terminal_Clear proc
    invoke SendMessage, g_hTerminal, WM_SETTEXT, 0, addr szTerminalPrompt
    ret
Terminal_Clear endp

; ============================================================================
; Terminal_RunBuild - Run build command
; ============================================================================
Terminal_RunBuild proc
    invoke Terminal_ExecuteCommand, addr szBuildCommand
    ret
Terminal_RunBuild endp

; ============================================================================
; Terminal_RunTests - Run test command
; ============================================================================
Terminal_RunTests proc
    invoke Terminal_ExecuteCommand, addr szTestCommand
    ret
Terminal_RunTests endp

; ============================================================================
; Terminal_GitStatus - Run git status
; ============================================================================
Terminal_GitStatus proc
    invoke Terminal_ExecuteCommand, addr szGitStatus
    ret
Terminal_GitStatus endp

; ============================================================================
; Terminal_GitCommit - Run git commit
; ============================================================================
Terminal_GitCommit proc
    invoke Terminal_ExecuteCommand, addr szGitCommit
    ret
Terminal_GitCommit endp

; ============================================================================
; Terminal_ExecuteCustom - Execute custom command
; Input: pszCommand
; ============================================================================
Terminal_ExecuteCustom proc pszCommand:DWORD
    invoke Terminal_ExecuteCommand, pszCommand
    ret
Terminal_ExecuteCustom endp

; ============================================================================
; Terminal_GetOutput - Get terminal output
; Returns: Pointer to output in eax (caller must free)
; ============================================================================
Terminal_GetOutput proc
    LOCAL dwLength:DWORD
    LOCAL pBuffer:DWORD
    
    invoke SendMessage, g_hTerminal, WM_GETTEXTLENGTH, 0, 0
    mov dwLength, eax
    
    MemAlloc dwLength
    mov pBuffer, eax
    
    invoke SendMessage, g_hTerminal, WM_GETTEXT, dwLength, pBuffer
    
    mov eax, pBuffer
    ret
Terminal_GetOutput endp

; ============================================================================
; Terminal_Cleanup - Cleanup terminal resources
; ============================================================================
Terminal_Cleanup proc
    .if g_hTerminalPipe != 0
        invoke CloseHandle, g_hTerminalPipe
        mov g_hTerminalPipe, 0
    .endif
    
    .if g_hTerminalThread != 0
        invoke CloseHandle, g_hTerminalThread
        mov g_hTerminalThread, 0
    .endif
    
    ret
Terminal_Cleanup endp

; ============================================================================
; Data for terminal commands
; ============================================================================

.data
    szBuildCommand    db "cmake --build . --config Release", 0
    szTestCommand     db "cmake --build . --target test", 0
    szGitStatus       db "git status", 0
    szGitCommit       db "git commit -m \"Agentic commit\"", 0

end
