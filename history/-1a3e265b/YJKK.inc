;==============================================================================
; win64_api.inc
; Windows x64 API Type Definitions and Constants
; Required for RawrXD_NativeModelBridge_v2.asm
;==============================================================================

;------------------------------------------------------------------------------
; BASIC TYPES AND CONSTANTS
;------------------------------------------------------------------------------

TRUE    EQU 1
FALSE   EQU 0

SRWLOCK STRUCT
    _Ptr QWORD ?
SRWLOCK ENDS

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
EXTERN SetFilePointer:PROC

EXTERN CreateFileMappingA:PROC
EXTERN CreateFileMappingW:PROC
EXTERN MapViewOfFile:PROC
EXTERN UnmapViewOfFile:PROC

EXTERN VirtualAlloc:PROC
EXTERN VirtualFree:PROC
EXTERN VirtualProtect:PROC
EXTERN VirtualQuery:PROC

EXTERN HeapCreate:PROC
EXTERN HeapDestroy:PROC
EXTERN HeapAlloc:PROC
EXTERN HeapFree:PROC
EXTERN GetProcessHeap:PROC

EXTERN GetSystemInfo:PROC
EXTERN GetNativeSystemInfo:PROC

EXTERN TlsAlloc:PROC
EXTERN TlsFree:PROC
EXTERN TlsGetValue:PROC
EXTERN TlsSetValue:PROC

EXTERN CreateThread:PROC
EXTERN ExitThread:PROC
EXTERN GetCurrentThread:PROC
EXTERN GetCurrentThreadId:PROC
EXTERN SuspendThread:PROC
EXTERN ResumeThread:PROC
EXTERN TerminateThread:PROC
EXTERN GetExitCodeThread:PROC
EXTERN WaitForSingleObject:PROC
EXTERN WaitForMultipleObjects:PROC

EXTERN CreateEventA:PROC
EXTERN CreateEventW:PROC
EXTERN SetEvent:PROC
EXTERN ResetEvent:PROC
EXTERN PulseEvent:PROC

EXTERN CreateSemaphoreA:PROC
EXTERN CreateSemaphoreW:PROC
EXTERN ReleaseSemaphore:PROC

EXTERN CreateMutexA:PROC
EXTERN CreateMutexW:PROC
EXTERN ReleaseMutex:PROC

EXTERN InitializeSRWLock:PROC
EXTERN AcquireSRWLockExclusive:PROC
EXTERN ReleaseSRWLockExclusive:PROC
EXTERN AcquireSRWLockShared:PROC
EXTERN ReleaseSRWLockShared:PROC

EXTERN InitializeCriticalSection:PROC
EXTERN EnterCriticalSection:PROC
EXTERN LeaveCriticalSection:PROC
EXTERN DeleteCriticalSection:PROC
EXTERN TryEnterCriticalSection:PROC

EXTERN Sleep:PROC
EXTERN SleepEx:PROC

EXTERN GetLastError:PROC
EXTERN SetLastError:PROC
EXTERN FormatMessageA:PROC
EXTERN FormatMessageW:PROC

EXTERN OutputDebugStringA:PROC
EXTERN OutputDebugStringW:PROC

EXTERN DisableThreadLibraryCalls:PROC
EXTERN LoadLibraryA:PROC
EXTERN LoadLibraryW:PROC
EXTERN FreeLibrary:PROC
EXTERN GetProcAddress:PROC

EXTERN GetModuleHandleA:PROC
EXTERN GetModuleHandleW:PROC
EXTERN GetModuleFileNameA:PROC
EXTERN GetModuleFileNameW:PROC

EXTERN QueryPerformanceCounter:PROC
EXTERN QueryPerformanceFrequency:PROC
EXTERN GetTickCount:PROC
EXTERN GetTickCount64:PROC

;------------------------------------------------------------------------------
; EXTERNAL API DECLARATIONS - ntdll.dll (Advanced)
;------------------------------------------------------------------------------
EXTERN RtlCaptureContext:PROC
EXTERN RtlLookupFunctionEntry:PROC
EXTERN RtlVirtualUnwind:PROC

;------------------------------------------------------------------------------
; EXTERNAL API DECLARATIONS - user32.dll (Optional)
;------------------------------------------------------------------------------
EXTERN MessageBoxA:PROC
EXTERN MessageBoxW:PROC

;------------------------------------------------------------------------------
; C RUNTIME LIBRARY (msvcrt.dll or static)
;------------------------------------------------------------------------------
EXTERN malloc:PROC
EXTERN free:PROC
EXTERN realloc:PROC
EXTERN calloc:PROC
EXTERN memset:PROC
EXTERN memcpy:PROC
EXTERN memmove:PROC
EXTERN memcmp:PROC
EXTERN strlen:PROC
EXTERN strcmp:PROC
EXTERN strncmp:PROC
EXTERN strcpy:PROC
EXTERN strncpy:PROC
EXTERN strcat:PROC
EXTERN strncat:PROC
EXTERN strchr:PROC
EXTERN strrchr:PROC
EXTERN strstr:PROC
EXTERN sprintf:PROC
EXTERN snprintf:PROC
EXTERN sscanf:PROC
EXTERN rand:PROC
EXTERN srand:PROC
EXTERN abs:PROC
EXTERN labs:PROC
EXTERN sqrt:PROC
EXTERN sqrtf:PROC
EXTERN sin:PROC
EXTERN cos:PROC
EXTERN exp:PROC
EXTERN log:PROC
EXTERN pow:PROC
EXTERN floor:PROC
EXTERN ceil:PROC

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
