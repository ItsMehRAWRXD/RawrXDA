; ============================================================================
; mini_winconst.inc - Minimal Win32 constants for MASM-only modules
; Use this to avoid pulling in windows.inc for non-UI code.
; ============================================================================

; Booleans / handles
NULL                    equ 0
TRUE                    equ 1
FALSE                   equ 0
INVALID_HANDLE_VALUE    equ -1

; Common access flags
GENERIC_READ            equ 80000000h
GENERIC_WRITE           equ 40000000h

FILE_SHARE_READ         equ 1h
FILE_SHARE_WRITE        equ 2h
FILE_SHARE_DELETE       equ 4h

OPEN_EXISTING           equ 3h
CREATE_NEW              equ 1h
CREATE_ALWAYS           equ 2h
OPEN_ALWAYS             equ 4h
TRUNCATE_EXISTING       equ 5h

FILE_ATTRIBUTE_NORMAL   equ 80h
FILE_FLAG_SEQUENTIAL_SCAN equ 08000000h

; Heap
HEAP_ZERO_MEMORY        equ 8h

; File mapping
PAGE_READONLY           equ 2h
PAGE_READWRITE          equ 4h
FILE_MAP_READ           equ 4h
FILE_MAP_WRITE          equ 2h

; DLL reasons
DLL_PROCESS_DETACH      equ 0
DLL_PROCESS_ATTACH      equ 1
DLL_THREAD_ATTACH       equ 2
DLL_THREAD_DETACH       equ 3

; Structures
SYSTEM_INFO STRUCT
    UNION
        dwOemId             DWORD ?
        STRUCT
            wProcessorArchitecture  WORD ?
            wReserved               WORD ?
        ENDS
    ENDS
    dwPageSize              DWORD ?
    lpMinimumApplicationAddress DWORD ?
    lpMaximumApplicationAddress DWORD ?
    dwActiveProcessorMask   DWORD ?
    dwNumberOfProcessors    DWORD ?
    dwProcessorType         DWORD ?
    dwAllocationGranularity DWORD ?
    wProcessorLevel         WORD ?
    wProcessorRevision      WORD ?
SYSTEM_INFO ENDS

MEMORYSTATUSEX STRUCT
    dwLength                DWORD ?
    dwMemoryLoad            DWORD ?
    ullTotalPhys            QWORD ?
    ullAvailPhys            QWORD ?
    ullTotalPageFile        QWORD ?
    ullAvailPageFile        QWORD ?
    ullTotalVirtual         QWORD ?
    ullAvailVirtual         QWORD ?
    ullAvailExtendedVirtual QWORD ?
MEMORYSTATUSEX ENDS
