TRUE equ 1
FALSE equ 0
WAIT_OBJECT_0 equ 0L

SYSTEM_INFO STRUCT
    union
        dwOemId DWORD ?
        struct
            wProcessorArchitecture WORD ?
            wReserved WORD ?
        ends
    ends
    dwPageSize DWORD ?
    lpMinimumApplicationAddress QWORD ?
    lpMaximumApplicationAddress QWORD ?
    dwActiveProcessorMask QWORD ?
    dwNumberOfProcessors DWORD ?
    dwProcessorType DWORD ?
    dwAllocationGranularity DWORD ?
    wProcessorLevel WORD ?
    wProcessorRevision WORD ?
SYSTEM_INFO ENDS
