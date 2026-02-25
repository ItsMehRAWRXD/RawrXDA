;==============================================================================
; win64_api.inc - Extended for Production RawrXD
;==============================================================================

TRUE    EQU 1
FALSE   EQU 0

; Memory
MEM_COMMIT              EQU 1000h
MEM_RESERVE             EQU 2000h
MEM_RELEASE             EQU 8000h
PAGE_READONLY           EQU 02h
PAGE_READWRITE          EQU 04h

; File
GENERIC_READ            EQU 80000000h
FILE_SHARE_READ         EQU 00000001h
OPEN_EXISTING           EQU 3
FILE_MAP_READ           EQU 0004h

; GGUF Magic
GGUF_MAGIC_LE           EQU 46554747h ; 'GGUF'

; Structures (Standard MASM64 compatible)
SRWLOCK STRUCT
    _Ptr QWORD ?
SRWLOCK ENDS

INIT_ONCE STRUCT
    _Ptr QWORD ?
INIT_ONCE ENDS

SYSTEM_INFO STRUCT
    dwOemId                 DWORD ?
    dwPageSize              DWORD ?
    lpMinimumAddr           QWORD ?
    lpMaximumAddr           QWORD ?
    dwActiveProcMask        QWORD ?
    dwNumProcessors         DWORD ?
    dwProcType              DWORD ?
    dwAllocGranularity      DWORD ?
    wProcLevel              WORD ?
    wProcRevision           WORD ?
SYSTEM_INFO ENDS

; DLL Constants
DLL_PROCESS_ATTACH      EQU 1

; AVX-512 constants
ALIGN_16 MACRO
    ALIGN 16
ENDM

;==============================================================================
; NOTE: Structures are not defined here because MASM64 has limitations
; with certain type names. Use offsets manually in code as needed.
;============================================================================== 

;------------------------------------------------------------------------------
; WINDOWS CONSTANTS - Memory Management
;------------------------------------------------------------------------------
MEM_COMMIT              EQU 00001000h
MEM_RESERVE             EQU 00002000h
MEM_RESET               EQU 00080000h
MEM_RELEASE             EQU 00008000h
MEM_LARGE_PAGES         EQU 20000000h

PAGE_NOACCESS           EQU 01h
PAGE_READONLY           EQU 02h
PAGE_READWRITE          EQU 04h
PAGE_WRITECOPY          EQU 08h
PAGE_EXECUTE            EQU 10h
PAGE_EXECUTE_READ       EQU 20h
PAGE_EXECUTE_READWRITE  EQU 40h
PAGE_EXECUTE_WRITECOPY  EQU 80h
PAGE_GUARD              EQU 100h
PAGE_NOCACHE            EQU 200h
PAGE_WRITECOMBINE       EQU 400h

;------------------------------------------------------------------------------
; WINDOWS CONSTANTS - File Operations
;------------------------------------------------------------------------------
GENERIC_READ            EQU 80000000h
GENERIC_WRITE           EQU 40000000h
GENERIC_EXECUTE         EQU 20000000h
GENERIC_ALL             EQU 10000000h

FILE_SHARE_READ         EQU 00000001h
FILE_SHARE_WRITE        EQU 00000002h
FILE_SHARE_DELETE       EQU 00000004h

CREATE_NEW              EQU 1
CREATE_ALWAYS           EQU 2
OPEN_EXISTING           EQU 3
OPEN_ALWAYS             EQU 4
TRUNCATE_EXISTING       EQU 5

FILE_ATTRIBUTE_READONLY         EQU 00000001h
FILE_ATTRIBUTE_HIDDEN           EQU 00000002h
FILE_ATTRIBUTE_SYSTEM           EQU 00000004h
FILE_ATTRIBUTE_DIRECTORY        EQU 00000010h
FILE_ATTRIBUTE_ARCHIVE          EQU 00000020h
FILE_ATTRIBUTE_DEVICE           EQU 00000040h
FILE_ATTRIBUTE_NORMAL           EQU 00000080h
FILE_ATTRIBUTE_TEMPORARY        EQU 00000100h

INVALID_HANDLE_VALUE    EQU -1
INVALID_FILE_SIZE       EQU 0FFFFFFFFh

;------------------------------------------------------------------------------
; WINDOWS CONSTANTS - File Mapping
;------------------------------------------------------------------------------
PAGE_READONLY           EQU 02h
PAGE_READWRITE          EQU 04h
PAGE_WRITECOPY          EQU 08h
SEC_COMMIT              EQU 08000000h
SEC_IMAGE               EQU 01000000h
SEC_NOCACHE             EQU 10000000h
SEC_RESERVE             EQU 04000000h
SEC_LARGE_PAGES         EQU 80000000h

FILE_MAP_WRITE          EQU 00000002h
FILE_MAP_READ           EQU 00000004h
FILE_MAP_ALL_ACCESS     EQU 000F001Fh
FILE_MAP_COPY           EQU 00000001h
FILE_MAP_EXECUTE        EQU 00000020h

;------------------------------------------------------------------------------
; WINDOWS CONSTANTS - DLL Process
;------------------------------------------------------------------------------
DLL_PROCESS_DETACH      EQU 0
DLL_PROCESS_ATTACH      EQU 1
DLL_THREAD_ATTACH       EQU 2
DLL_THREAD_DETACH       EQU 3

;------------------------------------------------------------------------------
; WINDOWS CONSTANTS - Thread Priority
;------------------------------------------------------------------------------
THREAD_PRIORITY_LOWEST          EQU -2
THREAD_PRIORITY_BELOW_NORMAL    EQU -1
THREAD_PRIORITY_NORMAL          EQU 0
THREAD_PRIORITY_ABOVE_NORMAL    EQU 1
THREAD_PRIORITY_HIGHEST         EQU 2
THREAD_PRIORITY_TIME_CRITICAL   EQU 15
THREAD_PRIORITY_IDLE            EQU -15

;------------------------------------------------------------------------------
; WINDOWS CONSTANTS - Wait Functions
;------------------------------------------------------------------------------
INFINITE                EQU 0FFFFFFFFh
WAIT_ABANDONED          EQU 00000080h
WAIT_OBJECT_0           EQU 00000000h
WAIT_TIMEOUT            EQU 00000102h
WAIT_FAILED             EQU 0FFFFFFFFh

;------------------------------------------------------------------------------
; WINDOWS CONSTANTS - Thread Access Rights
;------------------------------------------------------------------------------
THREAD_ALL_ACCESS       EQU 001F03FFh
THREAD_TERMINATE        EQU 00000001h
THREAD_SUSPEND_RESUME   EQU 00000002h
THREAD_GET_CONTEXT      EQU 00000008h
THREAD_SET_CONTEXT      EQU 00000010h
THREAD_SET_INFORMATION  EQU 00000020h
THREAD_QUERY_INFORMATION EQU 00000040h

;------------------------------------------------------------------------------
; WINDOWS CONSTANTS - Error Codes
;------------------------------------------------------------------------------
ERROR_SUCCESS           EQU 0
ERROR_INVALID_FUNCTION  EQU 1
ERROR_FILE_NOT_FOUND    EQU 2
ERROR_PATH_NOT_FOUND    EQU 3
ERROR_ACCESS_DENIED     EQU 5
ERROR_INVALID_HANDLE    EQU 6
ERROR_NOT_ENOUGH_MEMORY EQU 8
ERROR_OUTOFMEMORY       EQU 14
ERROR_INVALID_PARAMETER EQU 87
ERROR_INSUFFICIENT_BUFFER EQU 122
ERROR_ALREADY_EXISTS    EQU 183

;------------------------------------------------------------------------------
; TLS (Thread Local Storage)
;------------------------------------------------------------------------------
TLS_OUT_OF_INDEXES      EQU 0FFFFFFFFh

;------------------------------------------------------------------------------
; EXTERNAL API DECLARATIONS - kernel32.dll
;------------------------------------------------------------------------------
EXTERN CreateFileA:PROC
EXTERN CreateFileW:PROC
EXTERN ReadFile:PROC
EXTERN WriteFile:PROC
EXTERN CloseHandle:PROC
EXTERN GetFileSizeEx:PROC
EXTERN CreateFileMappingA:PROC
EXTERN MapViewOfFile:PROC
EXTERN UnmapViewOfFile:PROC
EXTERN VirtualAlloc:PROC
EXTERN VirtualFree:PROC
EXTERN TlsAlloc:PROC
EXTERN TlsFree:PROC
EXTERN TlsGetValue:PROC
EXTERN TlsSetValue:PROC
EXTERN GetSystemInfo:PROC
EXTERN DisableThreadLibraryCalls:PROC
EXTERN GetTickCount64:PROC
EXTERN QueryPerformanceCounter:PROC
EXTERN QueryPerformanceFrequency:PROC
EXTERN InitializeSRWLock:PROC
EXTERN AcquireSRWLockExclusive:PROC
EXTERN ReleaseSRWLockExclusive:PROC
EXTERN AcquireSRWLockShared:PROC
EXTERN ReleaseSRWLockShared:PROC
EXTERN Sleep:PROC
EXTERN ExitProcess:PROC
EXTERN IsDebuggerPresent:PROC
EXTERN OutputDebugStringA:PROC

;------------------------------------------------------------------------------
; EXTERNAL API DECLARATIONS - msvcrt.dll / ucrtbase.dll
;------------------------------------------------------------------------------
EXTERN malloc:PROC
EXTERN calloc:PROC
EXTERN realloc:PROC
EXTERN free:PROC
EXTERN memset:PROC
EXTERN memcpy:PROC
EXTERN memmove:PROC
EXTERN memcmp:PROC
EXTERN strlen:PROC
; EXTERN :PROC  ; Removed unused and problematic reference
EXTERN printf:PROC
EXTERN floor:PROC
EXTERN ceil:PROC
EXTERN sqrt:PROC
EXTERN sin:PROC
EXTERN cos:PROC
EXTERN exp:PROC
EXTERN pow:PROC
EXTERN log:PROC
;==============================================================================
; MACRO DEFINITIONS FOR COMMON OPERATIONS
;==============================================================================

;------------------------------------------------------------------------------
; ALIGN_UP - Align value to boundary
;   Usage: ALIGN_UP rax, 64  ; Align RAX to 64-byte boundary
;------------------------------------------------------------------------------
ALIGN_UP MACRO reg, boundary
    add reg, boundary - 1
    and reg, NOT (boundary - 1)
ENDM

;------------------------------------------------------------------------------
; ALIGN_DOWN - Align value down to boundary
;------------------------------------------------------------------------------
ALIGN_DOWN MACRO reg, boundary
    and reg, NOT (boundary - 1)
ENDM

;------------------------------------------------------------------------------
; SAVE_REGS - Save non-volatile registers
;------------------------------------------------------------------------------
SAVE_REGS MACRO offset
    mov [rsp+offset], rbx
    mov [rsp+offset+8], rsi
    mov [rsp+offset+16], rdi
    mov [rsp+offset+24], r12
    mov [rsp+offset+32], r13
    mov [rsp+offset+40], r14
    mov [rsp+offset+48], r15
ENDM

;------------------------------------------------------------------------------
; RESTORE_REGS - Restore non-volatile registers
;------------------------------------------------------------------------------
RESTORE_REGS MACRO offset
    mov rbx, [rsp+offset]
    mov rsi, [rsp+offset+8]
    mov rdi, [rsp+offset+16]
    mov r12, [rsp+offset+24]
    mov r13, [rsp+offset+32]
    mov r14, [rsp+offset+40]
    mov r15, [rsp+offset+48]
ENDM

;------------------------------------------------------------------------------
; ALLOC_SHADOW - Allocate shadow space (32 bytes minimum)
;------------------------------------------------------------------------------
ALLOC_SHADOW MACRO
    sub rsp, 20h
ENDM

;------------------------------------------------------------------------------
; FREE_SHADOW - Free shadow space
;------------------------------------------------------------------------------
FREE_SHADOW MACRO
    add rsp, 20h
ENDM

;==============================================================================
; END OF win64_api.inc
;==============================================================================
