; ═══════════════════════════════════════════════════════════════════════════════
; RawrXD_Defs.inc - Common Definitions and Prototypes
; ═══════════════════════════════════════════════════════════════════════════════

; Basic Types
DWORD typedef DWORD
QWORD typedef QWORD
HANDLE typedef QWORD
LPVOID typedef QWORD
BOOL typedef DWORD
WORD typedef WORD
BYTE typedef BYTE

; Constants
HEAP_ZERO_MEMORY EQU 00000008h

; Structures
SYSTEM_INFO STRUCT
    wProcessorArchitecture      WORD ?
    wReserved                   WORD ?
    dwPageSize                  DWORD ?
    lpMinimumApplicationAddress LPVOID ?
    lpMaximumApplicationAddress LPVOID ?
    dwActiveProcessorMask       QWORD ?
    dwNumberOfProcessors        DWORD ?
    dwProcessorType             DWORD ?
    dwAllocationGranularity     DWORD ?
    wProcessorLevel             WORD ?
    wProcessorRevision          WORD ?
SYSTEM_INFO ENDS

OVERLAPPED STRUCT
    Internal        QWORD ?
    InternalHigh    QWORD ?
    Offset_         DWORD ?
    OffsetHigh      DWORD ?
    hEvent          QWORD ?
OVERLAPPED ENDS

; Kernel32 Prototypes
EXTERN GetSystemInfo : PROC
EXTERN SwitchToThread : PROC
EXTERN GetProcessHeap : PROC
EXTERN HeapAlloc : PROC
EXTERN HeapFree : PROC
EXTERN SetThreadAffinityMask : PROC
EXTERN VirtualAlloc : PROC
EXTERN VirtualFree : PROC
EXTERN LoadLibraryA : PROC
EXTERN GetProcAddress : PROC
EXTERN Sleep : PROC
EXTERN QueryPerformanceCounter : PROC
EXTERN QueryPerformanceFrequency : PROC
EXTERN ExitProcess : PROC
EXTERN OutputDebugStringA : PROC

; Winsock2 / User32 (Add as needed)
