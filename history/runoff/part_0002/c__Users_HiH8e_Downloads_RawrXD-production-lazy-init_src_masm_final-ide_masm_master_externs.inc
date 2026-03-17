; MASM Master Externs and Structures
; Centralized declarations for all MASM files
; Include this file at the top of every MASM source

; ============================================================
; Windows API Externs
; ============================================================
extern CreateMutexA:proc
extern ReleaseMutex:proc
extern WaitForSingleObject:proc
extern CreateThread:proc
extern CloseHandle:proc
extern CreatePipeEx:proc
extern CreateProcessA:proc
extern WriteFile:proc
extern ReadFile:proc
extern PeekNamedPipe:proc
extern GetStdHandle:proc
extern SetStdHandle:proc
extern Sleep:proc
extern ExitProcess:proc
extern BeginPaint:proc
extern EndPaint:proc
extern SetTextColor:proc
extern SetBkColor:proc
extern TextOutA:proc
extern GetDC:proc
extern ReleaseDC:proc

; ============================================================
; Windows API Constants
; ============================================================
INFINITE equ 0FFFFFFFFh
WAIT_OBJECT_0 equ 0
WAIT_TIMEOUT equ 258
STARTUPINFOA_SIZE equ 68
PROCESS_INFORMATION_SIZE equ 16
SECURITY_ATTRIBUTES_SIZE equ 12

; ============================================================
; Structure Definitions
; ============================================================

; SECURITY_ATTRIBUTES structure
SECURITY_ATTRIBUTES struct
    nLength DWORD ?
    lpSecurityDescriptor QWORD ?
    bInheritHandle DWORD ?
SECURITY_ATTRIBUTES ends

sa SECURITY_ATTRIBUTES <>

; STARTUPINFOA structure
STARTUPINFOA struct
    cb DWORD ?
    lpReserved QWORD ?
    lpDesktop QWORD ?
    lpTitle QWORD ?
    dwX DWORD ?
    dwY DWORD ?
    dwXSize DWORD ?
    dwYSize DWORD ?
    dwXCountChars DWORD ?
    dwYCountChars DWORD ?
    dwFillAttribute DWORD ?
    dwFlags DWORD ?
    wShowWindow WORD ?
    cbReserved2 WORD ?
    lpReserved2 QWORD ?
    hStdInput QWORD ?
    hStdOutput QWORD ?
    hStdError QWORD ?
STARTUPINFOA ends

; PROCESS_INFORMATION structure
PROCESS_INFORMATION struct
    hProcess QWORD ?
    hThread QWORD ?
    dwProcessId DWORD ?
    dwThreadId DWORD ?
PROCESS_INFORMATION ends

; PAINTSTRUCT structure
PAINTSTRUCT struct
    hdc QWORD ?
    fErase DWORD ?
    rcPaint_left DWORD ?
    rcPaint_top DWORD ?
    rcPaint_right DWORD ?
    rcPaint_bottom DWORD ?
    fRestore DWORD ?
    fIncUpdate DWORD ?
    rgbReserved BYTE 32 dup(?)
PAINTSTRUCT ends

ps PAINTSTRUCT <>

; ============================================================
; Handle and Resource Variables (BSS Section)
; ============================================================
.data?
    hStdInput QWORD ?
    hStdInRead QWORD ?
    hStdInWrite QWORD ?
    hStdOutput QWORD ?
    hStdOutRead QWORD ?
    hStdOutWrite QWORD ?
    hStdError QWORD ?
    hStdErrRead QWORD ?
    hStdErrWrite QWORD ?
    bytesRead QWORD ?
    bytesWritten QWORD ?
    bytesAvailable QWORD ?
    pi PROCESS_INFORMATION <>
    startup_info STARTUPINFOA <>
    wptr QWORD ?
    dwFlags DWORD ?
    cb DWORD ?
