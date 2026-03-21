; RawrXD Unlinked Symbol Resolver - Full Production
; 150 symbols in 10 batches of 15 - No stubs, direct resolution
; Pure x64 MASM - Zero dependencies

EXTERN PEWriter_AddImport:PROC

.data

; ============================================================================
; BATCH 1: Core Process & Memory (15 symbols)
; ============================================================================
BATCH1_SYMBOLS dq offset SYM_ExitProcess, offset SYM_GetProcessHeap, offset SYM_HeapAlloc
               dq offset SYM_HeapFree, offset SYM_VirtualAlloc, offset SYM_VirtualFree
               dq offset SYM_VirtualProtect, offset SYM_GetCurrentProcess, offset SYM_GetCurrentProcessId
               dq offset SYM_TerminateProcess, offset SYM_OpenProcess, offset SYM_GetExitCodeProcess
               dq offset SYM_FlushInstructionCache, offset SYM_GetProcessTimes, offset SYM_SetProcessAffinityMask

SYM_ExitProcess db 'ExitProcess',0
SYM_GetProcessHeap db 'GetProcessHeap',0
SYM_HeapAlloc db 'HeapAlloc',0
SYM_HeapFree db 'HeapFree',0
SYM_VirtualAlloc db 'VirtualAlloc',0
SYM_VirtualFree db 'VirtualFree',0
SYM_VirtualProtect db 'VirtualProtect',0
SYM_GetCurrentProcess db 'GetCurrentProcess',0
SYM_GetCurrentProcessId db 'GetCurrentProcessId',0
SYM_TerminateProcess db 'TerminateProcess',0
SYM_OpenProcess db 'OpenProcess',0
SYM_GetExitCodeProcess db 'GetExitCodeProcess',0
SYM_FlushInstructionCache db 'FlushInstructionCache',0
SYM_GetProcessTimes db 'GetProcessTimes',0
SYM_SetProcessAffinityMask db 'SetProcessAffinityMask',0

; ============================================================================
; BATCH 2: Thread Management (15 symbols)
; ============================================================================
BATCH2_SYMBOLS dq offset SYM_CreateThread, offset SYM_ExitThread, offset SYM_TerminateThread
               dq offset SYM_SuspendThread, offset SYM_ResumeThread, offset SYM_GetCurrentThread
               dq offset SYM_GetCurrentThreadId, offset SYM_GetThreadContext, offset SYM_SetThreadContext
               dq offset SYM_GetThreadPriority, offset SYM_SetThreadPriority, offset SYM_Sleep
               dq offset SYM_SleepEx, offset SYM_SwitchToThread, offset SYM_GetThreadTimes

SYM_CreateThread db 'CreateThread',0
SYM_ExitThread db 'ExitThread',0
SYM_TerminateThread db 'TerminateThread',0
SYM_SuspendThread db 'SuspendThread',0
SYM_ResumeThread db 'ResumeThread',0
SYM_GetCurrentThread db 'GetCurrentThread',0
SYM_GetCurrentThreadId db 'GetCurrentThreadId',0
SYM_GetThreadContext db 'GetThreadContext',0
SYM_SetThreadContext db 'SetThreadContext',0
SYM_GetThreadPriority db 'GetThreadPriority',0
SYM_SetThreadPriority db 'SetThreadPriority',0
SYM_Sleep db 'Sleep',0
SYM_SleepEx db 'SleepEx',0
SYM_SwitchToThread db 'SwitchToThread',0
SYM_GetThreadTimes db 'GetThreadTimes',0

; ============================================================================
; BATCH 3: Synchronization Primitives (15 symbols)
; ============================================================================
BATCH3_SYMBOLS dq offset SYM_WaitForSingleObject, offset SYM_WaitForMultipleObjects, offset SYM_CreateEventA
               dq offset SYM_SetEvent, offset SYM_ResetEvent, offset SYM_CreateMutexA
               dq offset SYM_ReleaseMutex, offset SYM_CreateSemaphoreA, offset SYM_ReleaseSemaphore
               dq offset SYM_InitializeCriticalSection, offset SYM_EnterCriticalSection, offset SYM_LeaveCriticalSection
               dq offset SYM_DeleteCriticalSection, offset SYM_TryEnterCriticalSection, offset SYM_InitializeSRWLock

SYM_WaitForSingleObject db 'WaitForSingleObject',0
SYM_WaitForMultipleObjects db 'WaitForMultipleObjects',0
SYM_CreateEventA db 'CreateEventA',0
SYM_SetEvent db 'SetEvent',0
SYM_ResetEvent db 'ResetEvent',0
SYM_CreateMutexA db 'CreateMutexA',0
SYM_ReleaseMutex db 'ReleaseMutex',0
SYM_CreateSemaphoreA db 'CreateSemaphoreA',0
SYM_ReleaseSemaphore db 'ReleaseSemaphore',0
SYM_InitializeCriticalSection db 'InitializeCriticalSection',0
SYM_EnterCriticalSection db 'EnterCriticalSection',0
SYM_LeaveCriticalSection db 'LeaveCriticalSection',0
SYM_DeleteCriticalSection db 'DeleteCriticalSection',0
SYM_TryEnterCriticalSection db 'TryEnterCriticalSection',0
SYM_InitializeSRWLock db 'InitializeSRWLock',0

; ============================================================================
; BATCH 4: File I/O Core (15 symbols)
; ============================================================================
BATCH4_SYMBOLS dq offset SYM_CreateFileA, offset SYM_ReadFile, offset SYM_WriteFile
               dq offset SYM_CloseHandle, offset SYM_GetFileSize, offset SYM_GetFileSizeEx
               dq offset SYM_SetFilePointer, offset SYM_SetFilePointerEx, offset SYM_SetEndOfFile
               dq offset SYM_FlushFileBuffers, offset SYM_GetFileType, offset SYM_GetFileTime
               dq offset SYM_SetFileTime, offset SYM_LockFile, offset SYM_UnlockFile

SYM_CreateFileA db 'CreateFileA',0
SYM_ReadFile db 'ReadFile',0
SYM_WriteFile db 'WriteFile',0
SYM_CloseHandle db 'CloseHandle',0
SYM_GetFileSize db 'GetFileSize',0
SYM_GetFileSizeEx db 'GetFileSizeEx',0
SYM_SetFilePointer db 'SetFilePointer',0
SYM_SetFilePointerEx db 'SetFilePointerEx',0
SYM_SetEndOfFile db 'SetEndOfFile',0
SYM_FlushFileBuffers db 'FlushFileBuffers',0
SYM_GetFileType db 'GetFileType',0
SYM_GetFileTime db 'GetFileTime',0
SYM_SetFileTime db 'SetFileTime',0
SYM_LockFile db 'LockFile',0
SYM_UnlockFile db 'UnlockFile',0

; ============================================================================
; BATCH 5: File System Operations (15 symbols)
; ============================================================================
BATCH5_SYMBOLS dq offset SYM_DeleteFileA, offset SYM_CopyFileA, offset SYM_MoveFileA
               dq offset SYM_GetFileAttributesA, offset SYM_SetFileAttributesA, offset SYM_FindFirstFileA
               dq offset SYM_FindNextFileA, offset SYM_FindClose, offset SYM_CreateDirectoryA
               dq offset SYM_RemoveDirectoryA, offset SYM_GetCurrentDirectoryA, offset SYM_SetCurrentDirectoryA
               dq offset SYM_GetFullPathNameA, offset SYM_GetTempPathA, offset SYM_GetTempFileNameA

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

; ============================================================================
; BATCH 6: Console I/O (15 symbols)
; ============================================================================
BATCH6_SYMBOLS dq offset SYM_GetStdHandle, offset SYM_SetStdHandle, offset SYM_WriteConsoleA
               dq offset SYM_ReadConsoleA, offset SYM_AllocConsole, offset SYM_FreeConsole
               dq offset SYM_GetConsoleMode, offset SYM_SetConsoleMode, offset SYM_GetConsoleTitleA
               dq offset SYM_SetConsoleTitleA, offset SYM_GetConsoleScreenBufferInfo, offset SYM_SetConsoleCursorPosition
               dq offset SYM_FillConsoleOutputCharacterA, offset SYM_FillConsoleOutputAttribute, offset SYM_GetNumberOfConsoleInputEvents

SYM_GetStdHandle db 'GetStdHandle',0
SYM_SetStdHandle db 'SetStdHandle',0
SYM_WriteConsoleA db 'WriteConsoleA',0
SYM_ReadConsoleA db 'ReadConsoleA',0
SYM_AllocConsole db 'AllocConsole',0
SYM_FreeConsole db 'FreeConsole',0
SYM_GetConsoleMode db 'GetConsoleMode',0
SYM_SetConsoleMode db 'SetConsoleMode',0
SYM_GetConsoleTitleA db 'GetConsoleTitleA',0
SYM_SetConsoleTitleA db 'SetConsoleTitleA',0
SYM_GetConsoleScreenBufferInfo db 'GetConsoleScreenBufferInfo',0
SYM_SetConsoleCursorPosition db 'SetConsoleCursorPosition',0
SYM_FillConsoleOutputCharacterA db 'FillConsoleOutputCharacterA',0
SYM_FillConsoleOutputAttribute db 'FillConsoleOutputAttribute',0
SYM_GetNumberOfConsoleInputEvents db 'GetNumberOfConsoleInputEvents',0

; ============================================================================
; BATCH 7: Module & Library Loading (15 symbols)
; ============================================================================
BATCH7_SYMBOLS dq offset SYM_GetModuleHandleA, offset SYM_GetModuleHandleExA, offset SYM_GetModuleFileNameA
               dq offset SYM_LoadLibraryA, offset SYM_LoadLibraryExA, offset SYM_FreeLibrary
               dq offset SYM_GetProcAddress, offset SYM_EnumProcessModules, offset SYM_GetModuleInformation
               dq offset SYM_GetModuleBaseNameA, offset SYM_GetModuleFileNameExA, offset SYM_LoadResource
               dq offset SYM_FindResourceA, offset SYM_SizeofResource, offset SYM_LockResource

SYM_GetModuleHandleA db 'GetModuleHandleA',0
SYM_GetModuleHandleExA db 'GetModuleHandleExA',0
SYM_GetModuleFileNameA db 'GetModuleFileNameA',0
SYM_LoadLibraryA db 'LoadLibraryA',0
SYM_LoadLibraryExA db 'LoadLibraryExA',0
SYM_FreeLibrary db 'FreeLibrary',0
SYM_GetProcAddress db 'GetProcAddress',0
SYM_EnumProcessModules db 'EnumProcessModules',0
SYM_GetModuleInformation db 'GetModuleInformation',0
SYM_GetModuleBaseNameA db 'GetModuleBaseNameA',0
SYM_GetModuleFileNameExA db 'GetModuleFileNameExA',0
SYM_LoadResource db 'LoadResource',0
SYM_FindResourceA db 'FindResourceA',0
SYM_SizeofResource db 'SizeofResource',0
SYM_LockResource db 'LockResource',0

; ============================================================================
; BATCH 8: System Information & Time (15 symbols)
; ============================================================================
BATCH8_SYMBOLS dq offset SYM_GetSystemInfo, offset SYM_GetSystemTime, offset SYM_GetLocalTime
               dq offset SYM_SetSystemTime, offset SYM_SetLocalTime, offset SYM_SystemTimeToFileTime
               dq offset SYM_FileTimeToSystemTime, offset SYM_GetSystemTimeAsFileTime, offset SYM_GetTickCount
               dq offset SYM_GetTickCount64, offset SYM_QueryPerformanceCounter, offset SYM_QueryPerformanceFrequency
               dq offset SYM_GetSystemTimeAdjustment, offset SYM_GetTimeZoneInformation, offset SYM_SetTimeZoneInformation

SYM_GetSystemInfo db 'GetSystemInfo',0
SYM_GetSystemTime db 'GetSystemTime',0
SYM_GetLocalTime db 'GetLocalTime',0
SYM_SetSystemTime db 'SetSystemTime',0
SYM_SetLocalTime db 'SetLocalTime',0
SYM_SystemTimeToFileTime db 'SystemTimeToFileTime',0
SYM_FileTimeToSystemTime db 'FileTimeToSystemTime',0
SYM_GetSystemTimeAsFileTime db 'GetSystemTimeAsFileTime',0
SYM_GetTickCount db 'GetTickCount',0
SYM_GetTickCount64 db 'GetTickCount64',0
SYM_QueryPerformanceCounter db 'QueryPerformanceCounter',0
SYM_QueryPerformanceFrequency db 'QueryPerformanceFrequency',0
SYM_GetSystemTimeAdjustment db 'GetSystemTimeAdjustment',0
SYM_GetTimeZoneInformation db 'GetTimeZoneInformation',0
SYM_SetTimeZoneInformation db 'SetTimeZoneInformation',0

; ============================================================================
; BATCH 9: Error Handling & Environment (15 symbols)
; ============================================================================
BATCH9_SYMBOLS dq offset SYM_GetLastError, offset SYM_SetLastError, offset SYM_FormatMessageA
               dq offset SYM_GetCommandLineA, offset SYM_GetEnvironmentStrings, offset SYM_FreeEnvironmentStringsA
               dq offset SYM_GetEnvironmentVariableA, offset SYM_SetEnvironmentVariableA, offset SYM_ExpandEnvironmentStringsA
               dq offset SYM_GetVersion, offset SYM_GetVersionExA, offset SYM_GetComputerNameA
               dq offset SYM_SetComputerNameA, offset SYM_GetUserNameA, offset SYM_GetSystemDirectoryA

SYM_GetLastError db 'GetLastError',0
SYM_SetLastError db 'SetLastError',0
SYM_FormatMessageA db 'FormatMessageA',0
SYM_GetCommandLineA db 'GetCommandLineA',0
SYM_GetEnvironmentStrings db 'GetEnvironmentStrings',0
SYM_FreeEnvironmentStringsA db 'FreeEnvironmentStringsA',0
SYM_GetEnvironmentVariableA db 'GetEnvironmentVariableA',0
SYM_SetEnvironmentVariableA db 'SetEnvironmentVariableA',0
SYM_ExpandEnvironmentStringsA db 'ExpandEnvironmentStringsA',0
SYM_GetVersion db 'GetVersion',0
SYM_GetVersionExA db 'GetVersionExA',0
SYM_GetComputerNameA db 'GetComputerNameA',0
SYM_SetComputerNameA db 'SetComputerNameA',0
SYM_GetUserNameA db 'GetUserNameA',0
SYM_GetSystemDirectoryA db 'GetSystemDirectoryA',0

; ============================================================================
; BATCH 10: Advanced Memory & Debugging (15 symbols)
; ============================================================================
BATCH10_SYMBOLS dq offset SYM_VirtualQuery, offset SYM_VirtualQueryEx, offset SYM_VirtualAllocEx
                dq offset SYM_VirtualFreeEx, offset SYM_VirtualProtectEx, offset SYM_ReadProcessMemory
                dq offset SYM_WriteProcessMemory, offset SYM_CreateRemoteThread, offset SYM_IsDebuggerPresent
                dq offset SYM_CheckRemoteDebuggerPresent, offset SYM_OutputDebugStringA, offset SYM_DebugBreak
                dq offset SYM_GlobalMemoryStatusEx, offset SYM_GetProcessWorkingSetSize, offset SYM_SetProcessWorkingSetSize

SYM_VirtualQuery db 'VirtualQuery',0
SYM_VirtualQueryEx db 'VirtualQueryEx',0
SYM_VirtualAllocEx db 'VirtualAllocEx',0
SYM_VirtualFreeEx db 'VirtualFreeEx',0
SYM_VirtualProtectEx db 'VirtualProtectEx',0
SYM_ReadProcessMemory db 'ReadProcessMemory',0
SYM_WriteProcessMemory db 'WriteProcessMemory',0
SYM_CreateRemoteThread db 'CreateRemoteThread',0
SYM_IsDebuggerPresent db 'IsDebuggerPresent',0
SYM_CheckRemoteDebuggerPresent db 'CheckRemoteDebuggerPresent',0
SYM_OutputDebugStringA db 'OutputDebugStringA',0
SYM_DebugBreak db 'DebugBreak',0
SYM_GlobalMemoryStatusEx db 'GlobalMemoryStatusEx',0
SYM_GetProcessWorkingSetSize db 'GetProcessWorkingSetSize',0
SYM_SetProcessWorkingSetSize db 'SetProcessWorkingSetSize',0

DLL_KERNEL32 db 'kernel32.dll',0

.code

; ============================================================================
; Batch Resolver - Add single batch (1-10)
; RCX = PE context, RDX = batch number
; Returns RAX = 1 success, 0 failure
; ============================================================================
SymbolBatchResolver_AddBatch proc
    push rbx
    push rsi
    push rdi
    push r12
    sub rsp, 28h
    
    mov rbx, rcx
    mov r12, rdx
    
    ; Dispatch to batch
    lea rax, @batch_table
    cmp r12, 10
    ja @invalid
    lea rsi, [rax + r12*8 - 8]
    mov rsi, [rsi]
    jmp @process
    
@batch_table:
    dq offset BATCH1_SYMBOLS
    dq offset BATCH2_SYMBOLS
    dq offset BATCH3_SYMBOLS
    dq offset BATCH4_SYMBOLS
    dq offset BATCH5_SYMBOLS
    dq offset BATCH6_SYMBOLS
    dq offset BATCH7_SYMBOLS
    dq offset BATCH8_SYMBOLS
    dq offset BATCH9_SYMBOLS
    dq offset BATCH10_SYMBOLS
    
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
    
@invalid:
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

; ============================================================================
; Add all 10 batches (150 symbols total)
; RCX = PE context
; Returns RAX = 1 success, 0 failure
; ============================================================================
SymbolBatchResolver_AddAll proc
    push rbx
    push r12
    sub rsp, 28h
    
    mov rbx, rcx
    mov r12, 1
@loop:
    mov rcx, rbx
    mov rdx, r12
    call SymbolBatchResolver_AddBatch
    test rax, rax
    jz @fail
    inc r12
    cmp r12, 11
    jb @loop
    
    mov eax, 1
    jmp @done
@fail:
    xor eax, eax
@done:
    add rsp, 28h
    pop r12
    pop rbx
    ret
SymbolBatchResolver_AddAll endp

; ============================================================================
; Add range of batches
; RCX = PE context, RDX = start batch, R8 = end batch (inclusive)
; Returns RAX = 1 success, 0 failure
; ============================================================================
SymbolBatchResolver_AddRange proc
    push rbx
    push r12
    push r13
    sub rsp, 20h
    
    mov rbx, rcx
    mov r12, rdx
    mov r13, r8
    
    cmp r12, r13
    ja @fail
    cmp r13, 10
    ja @fail
    
@loop:
    mov rcx, rbx
    mov rdx, r12
    call SymbolBatchResolver_AddBatch
    test rax, rax
    jz @fail
    inc r12
    cmp r12, r13
    jbe @loop
    
    mov eax, 1
    jmp @done
@fail:
    xor eax, eax
@done:
    add rsp, 20h
    pop r13
    pop r12
    pop rbx
    ret
SymbolBatchResolver_AddRange endp

end
