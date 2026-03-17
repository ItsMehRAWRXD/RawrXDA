; ═══════════════════════════════════════════════════════════════════════════════
; RawrXD_Defs.inc - Common Definitions and Prototypes
; ═══════════════════════════════════════════════════════════════════════════════

; Basic Types
; DWORD, QWORD, WORD, BYTE are built-in
HANDLE typedef QWORD
BOOL typedef DWORD
LPVOID typedef QWORD

; Constants
HEAP_ZERO_MEMORY EQU 00000008h
TRUE EQU 1
FALSE EQU 0
LMEM_FIXED EQU 0

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
EXTERN GetTickCount64 : PROC
EXTERN LocalAlloc : PROC
EXTERN LocalFree : PROC
EXTERN RtlZeroMemory : PROC

; Winsock Definitions
WSADATA STRUCT
    wVersion        WORD ?
    wHighVersion    WORD ?
    iMaxSockets     WORD ?
    iMaxUdpDg       WORD ?
    lpVendorInfo    QWORD ?
    szDescription   BYTE 257 DUP (?)
    szSystemStatus  BYTE 129 DUP (?)
WSADATA ENDS

EXTERN WSAStartup : PROC
EXTERN socket : PROC
EXTERN CreateIoCompletionPort : PROC
EXTERN CreateEventA : PROC
EXTERN SetEvent : PROC
EXTERN WaitForSingleObject : PROC
EXTERN CloseHandle : PROC
EXTERN CreateThread : PROC
EXTERN WriteFile : PROC
EXTERN ReadFile : PROC
EXTERN GetStdHandle : PROC

; Common Constants
STD_OUTPUT_HANDLE EQU -11
STD_INPUT_HANDLE EQU -10
STD_ERROR_HANDLE EQU -12
INVALID_HANDLE_VALUE EQU -1
INFINITE EQU 0FFFFFFFFh
