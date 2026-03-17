; ============================================================================
; WINDOWS_SDK.INC - Windows SDK Replacement for MASM32 includes
; Provides all necessary Windows API declarations for RawrXD IDE
; Compatible with ml64.exe (x64 MASM) and Windows SDK 10.0.22621.0+
; ============================================================================

; ============================================================================
; WINDOWS CONSTANTS
; ============================================================================
NULL                    EQU 0
TRUE                    EQU 1
FALSE                   EQU 0

; Memory allocation flags
MEM_COMMIT              EQU 00001000h
MEM_RESERVE             EQU 00002000h
MEM_RELEASE             EQU 00008000h
PAGE_READWRITE          EQU 04h
PAGE_EXECUTE_READWRITE  EQU 40h

; File I/O constants
GENERIC_READ            EQU 80000000h
GENERIC_WRITE           EQU 40000000h
FILE_SHARE_READ         EQU 00000001h
FILE_SHARE_WRITE        EQU 00000002h
OPEN_EXISTING           EQU 3
CREATE_ALWAYS           EQU 2
FILE_ATTRIBUTE_NORMAL   EQU 80h
INVALID_HANDLE_VALUE    EQU -1

; Standard handles
STD_INPUT_HANDLE        EQU -10
STD_OUTPUT_HANDLE       EQU -11
STD_ERROR_HANDLE        EQU -12

; Window messages
WM_CREATE               EQU 0001h
WM_DESTROY              EQU 0002h
WM_CLOSE                EQU 0010h
WM_QUIT                 EQU 0012h
WM_PAINT                EQU 000Fh
WM_COMMAND              EQU 0111h

; ============================================================================
; KERNEL32.DLL FUNCTION PROTOTYPES
; ============================================================================
EXTERN GetModuleHandleA:PROC
EXTERN GetProcAddress:PROC
EXTERN LoadLibraryA:PROC
EXTERN FreeLibrary:PROC
EXTERN ExitProcess:PROC
EXTERN GetLastError:PROC
EXTERN SetLastError:PROC

; Memory management
EXTERN VirtualAlloc:PROC
EXTERN VirtualFree:PROC
EXTERN VirtualProtect:PROC
EXTERN HeapAlloc:PROC
EXTERN HeapFree:PROC
EXTERN GetProcessHeap:PROC

; File I/O
EXTERN CreateFileA:PROC
EXTERN CreateFileW:PROC
EXTERN ReadFile:PROC
EXTERN WriteFile:PROC
EXTERN CloseHandle:PROC
EXTERN GetFileSize:PROC
EXTERN SetFilePointer:PROC
EXTERN FlushFileBuffers:PROC

; String/Memory operations
EXTERN lstrlenA:PROC
EXTERN lstrcpyA:PROC
EXTERN lstrcatA:PROC
EXTERN lstrcmpA:PROC
EXTERN RtlMoveMemory:PROC
EXTERN RtlZeroMemory:PROC
EXTERN RtlFillMemory:PROC

; Thread management
EXTERN CreateThread:PROC
EXTERN TerminateThread:PROC
EXTERN WaitForSingleObject:PROC
EXTERN Sleep:PROC

; Console I/O
EXTERN GetStdHandle:PROC
EXTERN WriteConsoleA:PROC
EXTERN AllocConsole:PROC
EXTERN FreeConsole:PROC

; Critical sections
EXTERN InitializeCriticalSection:PROC
EXTERN EnterCriticalSection:PROC
EXTERN LeaveCriticalSection:PROC
EXTERN DeleteCriticalSection:PROC

; ============================================================================
; USER32.DLL FUNCTION PROTOTYPES
; ============================================================================
EXTERN MessageBoxA:PROC
EXTERN MessageBoxW:PROC
EXTERN CreateWindowExA:PROC
EXTERN DestroyWindow:PROC
EXTERN ShowWindow:PROC
EXTERN UpdateWindow:PROC
EXTERN GetMessageA:PROC
EXTERN TranslateMessage:PROC
EXTERN DispatchMessageA:PROC
EXTERN PostQuitMessage:PROC
EXTERN DefWindowProcA:PROC
EXTERN RegisterClassExA:PROC
EXTERN LoadIconA:PROC
EXTERN LoadCursorA:PROC
EXTERN GetClientRect:PROC
EXTERN InvalidateRect:PROC

; ============================================================================
; STRUCTURES
; ============================================================================

; CRITICAL_SECTION structure (x64)
CRITICAL_SECTION STRUCT
    DebugInfo           QWORD ?
    LockCount           DWORD ?
    RecursionCount      DWORD ?
    OwningThread        QWORD ?
    LockSemaphore       QWORD ?
    SpinCount           QWORD ?
CRITICAL_SECTION ENDS

; SECURITY_ATTRIBUTES structure
SECURITY_ATTRIBUTES STRUCT
    nLength             DWORD ?
    lpSecurityDescriptor QWORD ?
    bInheritHandle      DWORD ?
SECURITY_ATTRIBUTES ENDS

; ============================================================================
; MACROS
; ============================================================================

; Invoke macro for procedure calls (simplified for Windows SDK)
INVOKE MACRO procname:REQ, args:VARARG
    LOCAL argcount
    argcount = 0
    FOR arg, <args>
        argcount = argcount + 1
    ENDM
    
    IFE argcount
        call procname
    ELSE
        FOR arg, <args>
            IFIDNI <arg>, <NULL>
                xor rcx, rcx
            ELSE
                mov rcx, arg
            ENDIF
        ENDM
        call procname
    ENDIF
ENDM

; String definition macro
SADD MACRO quoted_text:REQ
    LOCAL local_string
    .data
    local_string db quoted_text, 0
    .code
    EXITM <offset local_string>
ENDM

; ============================================================================
; DATA SECTION MACROS
; ============================================================================
.data?
    ; Reserved for uninitialized data

.data
    ; Reserved for initialized data

.const
    ; Reserved for constants

.code
    ; Reserved for code
