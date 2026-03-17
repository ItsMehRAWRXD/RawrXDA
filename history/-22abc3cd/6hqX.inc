; =============================================================================
; rawrxd_win64.inc — Minimal Win64 MASM64 include (replaces windows.inc)
; =============================================================================
; Provides:
;   1. invoke macro for x64 calling convention (up to 6 args)
;   2. EXTERN declarations for kernel32/ntdll APIs used by modules
;   3. Common constants (NULL, TRUE, FALSE, INVALID_HANDLE_VALUE)
;   4. No 32-bit legacy, no MASM32 SDK dependency
; =============================================================================

; ---------------------------------------------------------------------------
; Constants
; ---------------------------------------------------------------------------
NULL                    EQU 0
TRUE                    EQU 1
FALSE                   EQU 0
INVALID_HANDLE_VALUE    EQU -1

; CreateFile access/share/creation constants
GENERIC_READ            EQU 80000000h
GENERIC_WRITE           EQU 40000000h
GENERIC_READ_WRITE      EQU 0C0000000h
FILE_SHARE_READ         EQU 1
FILE_SHARE_WRITE        EQU 2
CREATE_ALWAYS           EQU 2
OPEN_EXISTING           EQU 3
FILE_ATTRIBUTE_NORMAL   EQU 80h
FILE_BEGIN              EQU 0
FILE_CURRENT            EQU 1

; VirtualAlloc constants
MEM_COMMIT              EQU 1000h
MEM_RESERVE             EQU 2000h
PAGE_READWRITE          EQU 04h
PAGE_EXECUTE_READWRITE  EQU 40h

; Thread constants
INFINITE_WAIT           EQU 0FFFFFFFFh

; GetStdHandle constants
STD_OUTPUT_HANDLE       EQU -11
STD_ERROR_HANDLE        EQU -12

; ---------------------------------------------------------------------------
; Win32 API EXTERN declarations (kernel32, ntdll, msvcrt)
; ---------------------------------------------------------------------------
EXTERN CreateFileA:PROC
EXTERN ReadFile:PROC
EXTERN WriteFile:PROC
EXTERN CloseHandle:PROC
EXTERN SetFilePointerEx:PROC
EXTERN ExitProcess:PROC
EXTERN GetStdHandle:PROC
EXTERN WriteConsoleA:PROC
EXTERN lstrlenA:PROC
EXTERN VirtualAlloc:PROC
EXTERN VirtualFree:PROC
EXTERN LoadLibraryA:PROC
EXTERN GetProcAddress:PROC
EXTERN FreeLibrary:PROC
EXTERN CreateThread:PROC
EXTERN WaitForSingleObject:PROC
EXTERN Sleep:PROC
EXTERN GetTickCount:PROC
EXTERN GetLastError:PROC
EXTERN GetProcessHeap:PROC
EXTERN HeapAlloc:PROC
EXTERN HeapFree:PROC

; msvcrt string helpers
EXTERN sprintf_s:PROC
EXTERN strcpy_s:PROC

; ---------------------------------------------------------------------------
; invoke macro — x64 calling convention (RCX, RDX, R8, R9, stack)
; ---------------------------------------------------------------------------
; Usage:   invoke FuncName, arg1, arg2, arg3, arg4, arg5, arg6
; Supports 0-6 arguments. Stack space is always allocated (shadow + args).
; All register arguments are MOV'd, not PUSH'd.
; ---------------------------------------------------------------------------

invoke MACRO fname:REQ, a1, a2, a3, a4, a5, a6
    LOCAL _stack_args

    ;; Count stack arguments beyond 4
    _stack_args = 0
    IFNB <a6>
        _stack_args = 2
    ELSEIFNB <a5>
        _stack_args = 1
    ENDIF

    ;; Allocate shadow space (32 bytes min) + stack args (8 bytes each), aligned to 16
    IF _stack_args GT 0
        sub rsp, 8 * (4 + _stack_args)
    ELSE
        sub rsp, 32
    ENDIF

    ;; Load stack arguments (right to left) beyond first 4
    IFNB <a6>
        mov rax, a6
        mov [rsp+40], rax
    ENDIF
    IFNB <a5>
        mov rax, a5
        mov [rsp+32], rax
    ENDIF

    ;; Load register arguments
    IFNB <a4>
        mov r9, a4
    ENDIF
    IFNB <a3>
        mov r8, a3
    ENDIF
    IFNB <a2>
        mov rdx, a2
    ENDIF
    IFNB <a1>
        mov rcx, a1
    ENDIF

    call fname

    ;; Restore stack
    IF _stack_args GT 0
        add rsp, 8 * (4 + _stack_args)
    ELSE
        add rsp, 32
    ENDIF
ENDM
