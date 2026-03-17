; =============================================================================
; masm64_compat.inc — Compatibility shim for masm64 SDK (Hutch) code
; =============================================================================
; Provides the constants, EXTERN declarations, and macros that masm64 SDK
; files expect (OPTION WIN64, addr, cstr, window constants) so they can
; compile with stock ml64.exe without the full SDK installed.
; =============================================================================

IFNDEF MASM64_COMPAT_INC
MASM64_COMPAT_INC EQU 1

; ---------------------------------------------------------------------------
; Named Pipe Constants
; ---------------------------------------------------------------------------
PIPE_ACCESS_DUPLEX          EQU 3
PIPE_TYPE_MESSAGE           EQU 4
PIPE_READMODE_MESSAGE       EQU 2
PIPE_WAIT                   EQU 0
PIPE_UNLIMITED_INSTANCES    EQU 255

; Mailslot
MAILSLOT_WAIT_FOREVER       EQU 0FFFFFFFFh

; Process Creation
CREATE_NO_WINDOW            EQU 08000000h
STARTF_USESTDHANDLES        EQU 00000100h
STARTF_USESHOWWINDOW        EQU 00000001h

; File Attributes
INVALID_FILE_ATTRIBUTES     EQU 0FFFFFFFFh

; Heap
HEAP_ZERO_MEMORY            EQU 00000008h

; Boolean
ifndef TRUE
TRUE                        EQU 1
endif
ifndef FALSE
FALSE                       EQU 0
endif
ifndef NULL
NULL                        EQU 0
endif
ifndef INVALID_HANDLE_VALUE
INVALID_HANDLE_VALUE        EQU -1
endif

; Console
ifndef STD_OUTPUT_HANDLE
STD_OUTPUT_HANDLE           EQU -11
endif
ifndef STD_ERROR_HANDLE
STD_ERROR_HANDLE            EQU -12
endif

; Memory
ifndef MEM_COMMIT
MEM_COMMIT                  EQU 1000h
endif
ifndef MEM_RESERVE
MEM_RESERVE                 EQU 2000h
endif
ifndef PAGE_READWRITE
PAGE_READWRITE              EQU 04h
endif

; STARTUPINFO size (x64)
SIZEOF_STARTUPINFO          EQU 104

; CRITICAL_SECTION (x64 - 40 bytes)
ifndef CRITICAL_SECTION
CRITICAL_SECTION STRUCT
    DebugInfo       DQ ?
    LockCount       DD ?
    RecursionCount  DD ?
    OwningThread    DQ ?
    LockSemaphore   DQ ?
    SpinCount       DQ ?
CRITICAL_SECTION ENDS
endif

; ---------------------------------------------------------------------------
; API Extern Declarations (kernel32, user32, shell32, advapi32)
; ---------------------------------------------------------------------------
EXTERNDEF CreateNamedPipeA:PROC
EXTERNDEF ConnectNamedPipe:PROC
EXTERNDEF DisconnectNamedPipe:PROC
EXTERNDEF CreateMailslotA:PROC
EXTERNDEF CreatePipe:PROC
EXTERNDEF SetHandleInformation:PROC
EXTERNDEF FlushFileBuffers:PROC
EXTERNDEF CreateProcessA:PROC
EXTERNDEF GetFileAttributesA:PROC
EXTERNDEF OutputDebugStringA:PROC
EXTERNDEF GetCommandLineA:PROC
EXTERNDEF WriteConsoleA:PROC
EXTERNDEF GetProcessHeap:PROC
EXTERNDEF HeapAlloc:PROC
EXTERNDEF HeapFree:PROC
EXTERNDEF CreateFileA:PROC
EXTERNDEF ReadFile:PROC
EXTERNDEF WriteFile:PROC
EXTERNDEF CloseHandle:PROC
EXTERNDEF ExitProcess:PROC
EXTERNDEF GetStdHandle:PROC
EXTERNDEF VirtualAlloc:PROC
EXTERNDEF VirtualFree:PROC
EXTERNDEF LoadLibraryA:PROC
EXTERNDEF GetProcAddress:PROC
EXTERNDEF FreeLibrary:PROC
EXTERNDEF CreateThread:PROC
EXTERNDEF Sleep:PROC
EXTERNDEF WaitForSingleObject:PROC
EXTERNDEF lstrlenA:PROC
EXTERNDEF GetTickCount:PROC
EXTERNDEF GetLastError:PROC
EXTERNDEF SetFilePointerEx:PROC

; shell32
EXTERNDEF ShellExecuteA:PROC
EXTERNDEF SHGetFolderPathA:PROC

; user32
EXTERNDEF MessageBoxA:PROC

; ntdll
EXTERNDEF NtQuerySystemInformation:PROC

; msvcrt
EXTERNDEF sprintf_s:PROC
EXTERNDEF strcpy_s:PROC
EXTERNDEF memset:PROC

; ---------------------------------------------------------------------------
; cstr Macro — Creates an inline string constant (MASM64 SDK compatible)
; Usage: lea rcx, cstr("Hello World")
;   or:  mov rcx, cstr("Hello World")    ; via OFFSET
; ---------------------------------------------------------------------------
_cstr_idx = 0

cstr MACRO txt:VARARG
    LOCAL _sz
    _cstr_idx = _cstr_idx + 1
    .data
    _sz db txt, 0
    .code
    EXITM <OFFSET _sz>
ENDM

; ---------------------------------------------------------------------------
; addr Macro — For use inside invoke, resolves to OFFSET for globals
; In masm64 SDK, addr handles both stack locals and globals. This simplified
; version only supports globals (OFFSET). For locals, use LEA manually.
; ---------------------------------------------------------------------------
; Note: When used standalone (not in invoke), write: OFFSET varname
; The invoke macro in rawrxd_win64.inc handles this.

; ---------------------------------------------------------------------------
; STARTUPINFO struct (simplified for x64)
; ---------------------------------------------------------------------------
ifndef STARTUPINFOA
STARTUPINFOA STRUCT
    cb              DD ?
    lpReserved      DQ ?
    lpDesktop       DQ ?
    lpTitle         DQ ?
    dwX             DD ?
    dwY             DD ?
    dwXSize         DD ?
    dwYSize         DD ?
    dwXCountChars   DD ?
    dwYCountChars   DD ?
    dwFillAttribute DD ?
    dwFlags         DD ?
    wShowWindow     DW ?
    cbReserved2     DW ?
    lpReserved2     DQ ?
    hStdInput       DQ ?
    hStdOutput      DQ ?
    hStdError       DQ ?
STARTUPINFOA ENDS
endif

ifndef PROCESS_INFORMATION
PROCESS_INFORMATION STRUCT
    hProcess    DQ ?
    hThread     DQ ?
    dwProcessId DD ?
    dwThreadId  DD ?
PROCESS_INFORMATION ENDS
endif

; ---------------------------------------------------------------------------
; MEMORYSTATUSEX (for GlobalMemoryStatusEx)
; ---------------------------------------------------------------------------
ifndef MEMORYSTATUSEX
MEMORYSTATUSEX STRUCT
    dwLength                DD ?
    dwMemoryLoad            DD ?
    ullTotalPhys            DQ ?
    ullAvailPhys            DQ ?
    ullTotalPageFile        DQ ?
    ullAvailPageFile        DQ ?
    ullTotalVirtual         DQ ?
    ullAvailVirtual         DQ ?
    ullAvailExtendedVirtual DQ ?
MEMORYSTATUSEX ENDS
endif

EXTERNDEF GlobalMemoryStatusEx:PROC

ENDIF ; MASM64_COMPAT_INC
