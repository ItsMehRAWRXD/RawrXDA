;==============================================================================
; git_integration.asm - Pure MASM64 Git Version Control Integration
; ==========================================================================
; Wraps git.exe commands and parses output for the IDE.
; Zero C++ runtime dependencies.
;==============================================================================

.686p
.xmm
.model flat, c
option casemap:none

include windows.inc
include process_manager.inc

.data
    szGitStatus         BYTE "git status --short",0
    szGitLog            BYTE "git log --oneline -n 20",0
    szGitAdd            BYTE "git add .",0
    szGitCommit         BYTE "git commit -m ",0
    szGitPush           BYTE "git push",0
    szGitPull           BYTE "git pull",0
    
    szGitError          BYTE "Git command failed or git.exe not found.",0

.code

;==============================================================================
; GitExecuteCommand - Runs a git command and returns the output
;==============================================================================
GitExecuteCommand PROC uses rbx rsi rdi lpCommand:QWORD, pOutputBuffer:QWORD, dwMaxLen:DWORD
    LOCAL pi:PROCESS_INFO_EX
    LOCAL dwTotalRead:DWORD
    LOCAL dwRead:DWORD
    
    invoke CreateRedirectedProcess, lpCommand, addr pi
    .if rax == 0
        ret
    .endif
    
    mov dwTotalRead, 0
    
_read_loop:
    mov rsi, pOutputBuffer
    add rsi, qword ptr dwTotalRead
    
    mov eax, dwMaxLen
    sub eax, dwTotalRead
    .if eax <= 0
        jmp _done
    .endif
    
    invoke ReadProcessOutput, pi.hStdOutRead, rsi, eax
    .if eax == -1 || eax == 0
        jmp _done
    .endif
    
    add dwTotalRead, eax
    jmp _read_loop
    
_done:
    ; Null terminate
    mov rsi, pOutputBuffer
    add rsi, qword ptr dwTotalRead
    mov byte ptr [rsi], 0
    
    ; Cleanup
    invoke CloseHandle, pi.hProcess
    invoke CloseHandle, pi.hThread
    invoke CloseHandle, pi.hStdInWrite
    invoke CloseHandle, pi.hStdOutRead
    
    mov eax, dwTotalRead
    ret
GitExecuteCommand ENDP

;==============================================================================
; GitGetStatus - Returns the short status of the current repository
;==============================================================================
GitGetStatus PROC pBuffer:QWORD, dwMaxLen:DWORD
    invoke GitExecuteCommand, addr szGitStatus, pBuffer, dwMaxLen
    ret
GitGetStatus ENDP

;==============================================================================
; GitGetLog - Returns the recent commit log
;==============================================================================
GitGetLog PROC pBuffer:QWORD, dwMaxLen:DWORD
    invoke GitExecuteCommand, addr szGitLog, pBuffer, dwMaxLen
    ret
GitGetLog ENDP

END
