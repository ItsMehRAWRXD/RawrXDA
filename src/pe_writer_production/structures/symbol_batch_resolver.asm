; RawrXD Symbol Batch Resolver - Production Grade
; Resolves unlinked symbols in batches of 15 without stubs
; Pure x64 MASM - Zero CRT dependencies

.code

; Batch 1: Core Kernel32 Functions (15 symbols)
BATCH1_SYMBOLS:
    dq offset SYM_ExitProcess
    dq offset SYM_GetStdHandle
    dq offset SYM_WriteConsoleA
    dq offset SYM_ReadConsoleA
    dq offset SYM_GetProcessHeap
    dq offset SYM_HeapAlloc
    dq offset SYM_HeapFree
    dq offset SYM_CreateFileA
    dq offset SYM_ReadFile
    dq offset SYM_WriteFile
    dq offset SYM_CloseHandle
    dq offset SYM_GetFileSize
    dq offset SYM_SetFilePointer
    dq offset SYM_VirtualAlloc
    dq offset SYM_VirtualFree

SYM_ExitProcess db 'ExitProcess',0
SYM_GetStdHandle db 'GetStdHandle',0
SYM_WriteConsoleA db 'WriteConsoleA',0
SYM_ReadConsoleA db 'ReadConsoleA',0
SYM_GetProcessHeap db 'GetProcessHeap',0
SYM_HeapAlloc db 'HeapAlloc',0
SYM_HeapFree db 'HeapFree',0
SYM_CreateFileA db 'CreateFileA',0
SYM_ReadFile db 'ReadFile',0
SYM_WriteFile db 'WriteFile',0
SYM_CloseHandle db 'CloseHandle',0
SYM_GetFileSize db 'GetFileSize',0
SYM_SetFilePointer db 'SetFilePointer',0
SYM_VirtualAlloc db 'VirtualAlloc',0
SYM_VirtualFree db 'VirtualFree',0

; Batch 2: Extended Kernel32 (15 symbols)
BATCH2_SYMBOLS:
    dq offset SYM_GetModuleHandleA
    dq offset SYM_GetProcAddress
    dq offset SYM_LoadLibraryA
    dq offset SYM_FreeLibrary
    dq offset SYM_GetLastError
    dq offset SYM_SetLastError
    dq offset SYM_GetCommandLineA
    dq offset SYM_GetEnvironmentStrings
    dq offset SYM_FreeEnvironmentStringsA
    dq offset SYM_GetCurrentProcess
    dq offset SYM_GetCurrentThread
    dq offset SYM_GetCurrentProcessId
    dq offset SYM_GetCurrentThreadId
    dq offset SYM_Sleep
    dq offset SYM_GetTickCount64

SYM_GetModuleHandleA db 'GetModuleHandleA',0
SYM_GetProcAddress db 'GetProcAddress',0
SYM_LoadLibraryA db 'LoadLibraryA',0
SYM_FreeLibrary db 'FreeLibrary',0
SYM_GetLastError db 'GetLastError',0
SYM_SetLastError db 'SetLastError',0
SYM_GetCommandLineA db 'GetCommandLineA',0
SYM_GetEnvironmentStrings db 'GetEnvironmentStrings',0
SYM_FreeEnvironmentStringsA db 'FreeEnvironmentStringsA',0
SYM_GetCurrentProcess db 'GetCurrentProcess',0
SYM_GetCurrentThread db 'GetCurrentThread',0
SYM_GetCurrentProcessId db 'GetCurrentProcessId',0
SYM_GetCurrentThreadId db 'GetCurrentThreadId',0
SYM_Sleep db 'Sleep',0
SYM_GetTickCount64 db 'GetTickCount64',0

; Batch 3: Memory & Process (15 symbols)
BATCH3_SYMBOLS:
    dq offset SYM_VirtualProtect
    dq offset SYM_VirtualQuery
    dq offset SYM_CreateThread
    dq offset SYM_ExitThread
    dq offset SYM_TerminateThread
    dq offset SYM_WaitForSingleObject
    dq offset SYM_CreateEventA
    dq offset SYM_SetEvent
    dq offset SYM_ResetEvent
    dq offset SYM_CreateMutexA
    dq offset SYM_ReleaseMutex
    dq offset SYM_CreateSemaphoreA
    dq offset SYM_ReleaseSemaphore
    dq offset SYM_InitializeCriticalSection
    dq offset SYM_DeleteCriticalSection

SYM_VirtualProtect db 'VirtualProtect',0
SYM_VirtualQuery db 'VirtualQuery',0
SYM_CreateThread db 'CreateThread',0
SYM_ExitThread db 'ExitThread',0
SYM_TerminateThread db 'TerminateThread',0
SYM_WaitForSingleObject db 'WaitForSingleObject',0
SYM_CreateEventA db 'CreateEventA',0
SYM_SetEvent db 'SetEvent',0
SYM_ResetEvent db 'ResetEvent',0
SYM_CreateMutexA db 'CreateMutexA',0
SYM_ReleaseMutex db 'ReleaseMutex',0
SYM_CreateSemaphoreA db 'CreateSemaphoreA',0
SYM_ReleaseSemaphore db 'ReleaseSemaphore',0
SYM_InitializeCriticalSection db 'InitializeCriticalSection',0
SYM_DeleteCriticalSection db 'DeleteCriticalSection',0

; Batch 4: File System (15 symbols)
BATCH4_SYMBOLS:
    dq offset SYM_DeleteFileA
    dq offset SYM_CopyFileA
    dq offset SYM_MoveFileA
    dq offset SYM_GetFileAttributesA
    dq offset SYM_SetFileAttributesA
    dq offset SYM_FindFirstFileA
    dq offset SYM_FindNextFileA
    dq offset SYM_FindClose
    dq offset SYM_CreateDirectoryA
    dq offset SYM_RemoveDirectoryA
    dq offset SYM_GetCurrentDirectoryA
    dq offset SYM_SetCurrentDirectoryA
    dq offset SYM_GetFullPathNameA
    dq offset SYM_GetTempPathA
    dq offset SYM_GetTempFileNameA

SYM_DeleteFileA db 'DeleteFileA',0
SYM_CopyFileA db 'CopyFileA',0
SYM_MoveFileA db 'MoveFileA',0
SYM_GetFileAttributesA db 'GetFileAttributesA',0
SYM_SetFileAttributesA db 'SetFileAttributesA',0
SYM_FindFirstFileA db 'FindFirstFileA',0
SYM_FindNextFileA db 'FindNextFileA',0
SYM_FindClose db 'FindClose',0
SYM_CreateDirectoryA db 'CreateDirectoryA',0
SYM_RemoveDirectoryA db 'RemoveDirectoryA',0
SYM_GetCurrentDirectoryA db 'GetCurrentDirectoryA',0
SYM_SetCurrentDirectoryA db 'SetCurrentDirectoryA',0
SYM_GetFullPathNameA db 'GetFullPathNameA',0
SYM_GetTempPathA db 'GetTempPathA',0
SYM_GetTempFileNameA db 'GetTempFileNameA',0

; Batch 5: Console & Time (15 symbols)
BATCH5_SYMBOLS:
    dq offset SYM_AllocConsole
    dq offset SYM_FreeConsole
    dq offset SYM_GetConsoleMode
    dq offset SYM_SetConsoleMode
    dq offset SYM_GetConsoleTitleA
    dq offset SYM_SetConsoleTitleA
    dq offset SYM_GetSystemTime
    dq offset SYM_GetLocalTime
    dq offset SYM_SystemTimeToFileTime
    dq offset SYM_FileTimeToSystemTime
    dq offset SYM_GetSystemTimeAsFileTime
    dq offset SYM_QueryPerformanceCounter
    dq offset SYM_QueryPerformanceFrequency
    dq offset SYM_GetSystemInfo
    dq offset SYM_GlobalMemoryStatusEx

SYM_AllocConsole db 'AllocConsole',0
SYM_FreeConsole db 'FreeConsole',0
SYM_GetConsoleMode db 'GetConsoleMode',0
SYM_SetConsoleMode db 'SetConsoleMode',0
SYM_GetConsoleTitleA db 'GetConsoleTitleA',0
SYM_SetConsoleTitleA db 'SetConsoleTitleA',0
SYM_GetSystemTime db 'GetSystemTime',0
SYM_GetLocalTime db 'GetLocalTime',0
SYM_SystemTimeToFileTime db 'SystemTimeToFileTime',0
SYM_FileTimeToSystemTime db 'FileTimeToSystemTime',0
SYM_GetSystemTimeAsFileTime db 'GetSystemTimeAsFileTime',0
SYM_QueryPerformanceCounter db 'QueryPerformanceCounter',0
SYM_QueryPerformanceFrequency db 'QueryPerformanceFrequency',0
SYM_GetSystemInfo db 'GetSystemInfo',0
SYM_GlobalMemoryStatusEx db 'GlobalMemoryStatusEx',0

DLL_KERNEL32 db 'kernel32.dll',0

; Resolver: Batch import builder
; RCX = PE context, RDX = batch number (1-5)
; Returns RAX = 1 success, 0 failure
SymbolBatchResolver_AddBatch proc
    push rbx
    push rsi
    push rdi
    push r12
    sub rsp, 28h
    
    mov rbx, rcx
    mov r12, rdx
    
    ; Select batch
    cmp r12, 1
    je @batch1
    cmp r12, 2
    je @batch2
    cmp r12, 3
    je @batch3
    cmp r12, 4
    je @batch4
    cmp r12, 5
    je @batch5
    xor eax, eax
    jmp @done
    
@batch1:
    lea rsi, BATCH1_SYMBOLS
    jmp @process
@batch2:
    lea rsi, BATCH2_SYMBOLS
    jmp @process
@batch3:
    lea rsi, BATCH3_SYMBOLS
    jmp @process
@batch4:
    lea rsi, BATCH4_SYMBOLS
    jmp @process
@batch5:
    lea rsi, BATCH5_SYMBOLS
    
@process:
    mov rdi, 15
@loop:
    mov rcx, rbx
    lea rdx, DLL_KERNEL32
    mov r8, [rsi]
    call PEWriter_AddImport
    test rax, rax
    jz @fail
    add rsi, 8
    dec rdi
    jnz @loop
    
    mov eax, 1
    jmp @done
    
@fail:
    xor eax, eax
@done:
    add rsp, 28h
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
SymbolBatchResolver_AddBatch endp

; Add all batches
SymbolBatchResolver_AddAll proc
    push rbx
    sub rsp, 20h
    
    mov rbx, rcx
    mov rdx, 1
@loop:
    mov rcx, rbx
    call SymbolBatchResolver_AddBatch
    test rax, rax
    jz @fail
    inc rdx
    cmp rdx, 6
    jb @loop
    
    mov eax, 1
    jmp @done
@fail:
    xor eax, eax
@done:
    add rsp, 20h
    pop rbx
    ret
SymbolBatchResolver_AddAll endp

end
