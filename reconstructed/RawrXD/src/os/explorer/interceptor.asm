;============================================================================
; OS Explorer Interceptor - Integrated into MASM IDE
; Sits between target process and Windows OS, intercepting ALL system calls
; Provides real-time streaming to PowerShell CLI
;============================================================================

.686
.MMX
.XMM
.model flat, stdcall

option casemap :none
option prologue:none
option epilogue:none

;============================================================================
; INCLUDES
;============================================================================

include \masm32\include\windows.inc
include \masm32\include\kernel32.inc
include \masm32\include\user32.inc
include \masm32\include\advapi32.inc
include \masm32\include\ws2_32.inc
include \masm32\include\ole32.inc

includelib \masm32\lib\kernel32.lib
includelib \masm32\lib\user32.lib
includelib \masm32\lib\advapi32.lib
includelib \masm32\lib\ws2_32.lib
includelib \masm32\lib\ole32.lib

;============================================================================
; CONSTANTS
;============================================================================

OSINTERCEPTOR_MAGIC     equ 0x0SINT3RC3PT0R
MAX_HOOKS               equ 256
MAX_CALL_LOG            equ 100000
CALL_LOG_BUFFER_SIZE    equ 4096

; Hook types
HOOK_TYPE_FILE          equ 1
HOOK_TYPE_REGISTRY      equ 2
HOOK_TYPE_PROCESS       equ 3
HOOK_TYPE_MEMORY        equ 4
HOOK_TYPE_NETWORK       equ 5
HOOK_TYPE_WINDOW        equ 6
HOOK_TYPE_COM           equ 7
HOOK_TYPE_CRYPTO        equ 8

;============================================================================
; STRUCTURES
;============================================================================

; Main interceptor structure
OSInterceptor STRUCT
    Magic           QWORD ?
    Version         DWORD ?
    TargetPID       DWORD ?
    hTargetProcess  QWORD ?
    hStdOut         QWORD ?          ; PowerShell stdout handle
    hStdErr         QWORD ?          ; PowerShell stderr handle
    IsActive        QWORD ?
    HookTable       QWORD ?
    CallLog         QWORD ?
    CallLogIndex    QWORD ?
    Callback        QWORD ?
    Stats           InterceptorStats <>
    StreamBuffer    BYTE CALL_LOG_BUFFER_SIZE DUP(?)
OSInterceptor ENDS

; Hook entry
HookEntry STRUCT
    FunctionName    BYTE 64 DUP(?)    ; API function name
    OriginalAddr    QWORD ?           ; Original function address
    HookAddr        QWORD ?           ; Hook function address
    HookType        DWORD ?           ; Type of hook
    IsActive        QWORD ?           ; Is hook active?
    CallCount       QWORD ?           ; Number of calls intercepted
HookEntry ENDS

; Call log entry (for streaming)
CallLogEntry STRUCT
    Timestamp       QWORD ?           ; When call was made
    ThreadID        DWORD ?           ; Thread that made call
    ProcessID       DWORD ?           ; Process ID
    HookType        DWORD ?           ; Type of hook
    FunctionName    BYTE 64 DUP(?)    ; Function name
    Parameters      QWORD 8 DUP(?)    ; First 8 parameters
    ParamCount      DWORD ?           ; Number of parameters
    ReturnValue     QWORD ?           ; Return value
    Duration        QWORD ?           ; Call duration (cycles)
    CallStack       QWORD 16 DUP(?)   ; Call stack
    StackDepth      DWORD ?           ; Stack depth
    DataSize        DWORD ?           ; Size of additional data
    Data            QWORD ?           ; Pointer to additional data
CallLogEntry ENDS

; Interceptor statistics
InterceptorStats STRUCT
    TotalCalls      QWORD ?
    CallsPerSecond  QWORD ?
    LastUpdateTime  QWORD ?
    HookStats       QWORD MAX_HOOKS DUP(?)
    BytesStreamed   QWORD ?
    ErrorCount      QWORD ?
InterceptorStats ENDS

; Hook statistics
HookStats STRUCT
    HookType        DWORD ?
    CallCount       QWORD ?
    TotalDuration   QWORD ?
    MinDuration     QWORD ?
    MaxDuration     QWORD ?
    ErrorCount      QWORD ?
HookStats ENDS

;============================================================================
; GLOBAL DATA
;============================================================================

.data

; Global interceptor instance
g_Interceptor OSInterceptor <>

; Hook table
g_HookTable HookEntry MAX_HOOKS DUP(<>)

; Call log buffer (circular)
g_CallLogBuffer CallLogEntry MAX_CALL_LOG DUP(<>)

; PowerShell pipe handles
g_hPowerShellInput  QWORD ?
g_hPowerShellOutput QWORD ?
g_hPowerShellError  QWORD ?

; Streaming thread handles
g_hStreamThread     QWORD ?
g_hStreamEvent      QWORD ?

; CLI command buffer
g_CLICommand        BYTE 1024 DUP(?)
g_IsCLIActive       QWORD ?

;============================================================================
; CODE
;============================================================================

.code

;============================================================================
; INITIALIZATION
;============================================================================

; Initialize OS Explorer Interceptor
InitOSInterceptor PROC
    LOCAL hStdOut:QWORD
    LOCAL hStdErr:QWORD
    
    ; Get PowerShell stdout/stderr handles
    invoke GetStdHandle, STD_OUTPUT_HANDLE
    mov hStdOut, rax
    
    invoke GetStdHandle, STD_ERROR_HANDLE
    mov hStdErr, rax
    
    ; Initialize global interceptor
    mov g_Interceptor.Magic, OSINTERCEPTOR_MAGIC
    mov g_Interceptor.Version, 1
    mov g_Interceptor.hStdOut, hStdOut
    mov g_Interceptor.hStdErr, hStdErr
    mov g_Interceptor.IsActive, 0
    mov g_Interceptor.HookTable, OFFSET g_HookTable
    mov g_Interceptor.CallLog, OFFSET g_CallLogBuffer
    mov g_Interceptor.CallLogIndex, 0
    mov g_Interceptor.Callback, OFFSET StreamToPowerShell
    
    ; Initialize statistics
    mov g_Interceptor.Stats.TotalCalls, 0
    mov g_Interceptor.Stats.BytesStreamed, 0
    mov g_Interceptor.Stats.ErrorCount, 0
    
    ; Create streaming event
    invoke CreateEventA, NULL, FALSE, FALSE, NULL
    mov g_hStreamEvent, rax
    
    ; Initialize CLI
    mov g_IsCLIActive, 1
    
    ret
InitOSInterceptor ENDP

;============================================================================
; HOOK INSTALLATION
;============================================================================

; Install all OS hooks
InstallOSHooks PROC dwTargetPID:DWORD
    LOCAL hTarget:QWORD
    LOCAL hKernel32:QWORD
    LOCAL hAdvapi32:QWORD
    LOCAL hUser32:QWORD
    LOCAL hGdi32:QWORD
    LOCAL hWS2_32:QWORD
    LOCAL hOle32:QWORD
    LOCAL hCrypt32:QWORD
    
    ; Open target process
    invoke OpenProcess, PROCESS_ALL_ACCESS, FALSE, dwTargetPID
    .IF rax == 0
        invoke GetLastError
        jmp @error
    .ENDIF
    mov hTarget, rax
    mov g_Interceptor.hTargetProcess, rax
    mov g_Interceptor.TargetPID, dwTargetPID
    
    ; Get module handles
    invoke GetModuleHandleA, CSTR("kernel32.dll")
    mov hKernel32, rax
    
    invoke GetModuleHandleA, CSTR("advapi32.dll")
    mov hAdvapi32, rax
    
    invoke GetModuleHandleA, CSTR("user32.dll")
    mov hUser32, rax
    
    invoke GetModuleHandleA, CSTR("gdi32.dll")
    mov hGdi32, rax
    
    invoke GetModuleHandleA, CSTR("ws2_32.dll")
    mov hWS2_32, rax
    
    invoke GetModuleHandleA, CSTR("ole32.dll")
    mov hOle32, rax
    
    invoke GetModuleHandleA, CSTR("crypt32.dll")
    mov hCrypt32, rax
    
    ; Install file I/O hooks
    invoke InstallHook, hKernel32, CSTR("CreateFileW"), HOOK_TYPE_FILE, MyCreateFileWHook
    invoke InstallHook, hKernel32, CSTR("ReadFile"), HOOK_TYPE_FILE, MyReadFileHook
    invoke InstallHook, hKernel32, CSTR("WriteFile"), HOOK_TYPE_FILE, MyWriteFileHook
    invoke InstallHook, hKernel32, CSTR("DeleteFileW"), HOOK_TYPE_FILE, MyDeleteFileHook
    invoke InstallHook, hKernel32, CSTR("MoveFileW"), HOOK_TYPE_FILE, MyMoveFileHook
    
    ; Install registry hooks
    invoke InstallHook, hAdvapi32, CSTR("RegOpenKeyExW"), HOOK_TYPE_REGISTRY, MyRegOpenKeyExWHook
    invoke InstallHook, hAdvapi32, CSTR("RegQueryValueExW"), HOOK_TYPE_REGISTRY, MyRegQueryValueExWHook
    invoke InstallHook, hAdvapi32, CSTR("RegSetValueExW"), HOOK_TYPE_REGISTRY, MyRegSetValueExWHook
    invoke InstallHook, hAdvapi32, CSTR("RegCreateKeyExW"), HOOK_TYPE_REGISTRY, MyRegCreateKeyExWHook
    invoke InstallHook, hAdvapi32, CSTR("RegDeleteKeyW"), HOOK_TYPE_REGISTRY, MyRegDeleteKeyHook
    
    ; Install process/thread hooks
    invoke InstallHook, hKernel32, CSTR("CreateProcessW"), HOOK_TYPE_PROCESS, MyCreateProcessHook
    invoke InstallHook, hKernel32, CSTR("CreateThread"), HOOK_TYPE_PROCESS, MyCreateThreadHook
    invoke InstallHook, hKernel32, CSTR("TerminateProcess"), HOOK_TYPE_PROCESS, MyTerminateProcessHook
    invoke InstallHook, hKernel32, CSTR("TerminateThread"), HOOK_TYPE_PROCESS, MyTerminateThreadHook
    
    ; Install memory hooks
    invoke InstallHook, hKernel32, CSTR("VirtualAlloc"), HOOK_TYPE_MEMORY, MyVirtualAllocHook
    invoke InstallHook, hKernel32, CSTR("VirtualFree"), HOOK_TYPE_MEMORY, MyVirtualFreeHook
    invoke InstallHook, hKernel32, CSTR("VirtualProtect"), HOOK_TYPE_MEMORY, MyVirtualProtectHook
    invoke InstallHook, hKernel32, CSTR("HeapAlloc"), HOOK_TYPE_MEMORY, MyHeapAllocHook
    invoke InstallHook, hKernel32, CSTR("HeapFree"), HOOK_TYPE_MEMORY, MyHeapFreeHook
    
    ; Install network hooks
    invoke InstallHook, hWS2_32, CSTR("WSAConnect"), HOOK_TYPE_NETWORK, MyWSAConnectHook
    invoke InstallHook, hWS2_32, CSTR("send"), HOOK_TYPE_NETWORK, MySendHook
    invoke InstallHook, hWS2_32, CSTR("recv"), HOOK_TYPE_NETWORK, MyRecvHook
    invoke InstallHook, hWS2_32, CSTR("WSASend"), HOOK_TYPE_NETWORK, MyWSASendHook
    invoke InstallHook, hWS2_32, CSTR("WSARecv"), HOOK_TYPE_NETWORK, MyWSARecvHook
    invoke InstallHook, hWS2_32, CSTR("getaddrinfo"), HOOK_TYPE_NETWORK, MyGetAddrInfoHook
    
    ; Install window/graphics hooks
    invoke InstallHook, hUser32, CSTR("SendMessageW"), HOOK_TYPE_WINDOW, MySendMessageWHook
    invoke InstallHook, hUser32, CSTR("PostMessageW"), HOOK_TYPE_WINDOW, MyPostMessageWHook
    invoke InstallHook, hUser32, CSTR("CreateWindowExW"), HOOK_TYPE_WINDOW, MyCreateWindowHook
    invoke InstallHook, hGdi32, CSTR("BitBlt"), HOOK_TYPE_WINDOW, MyBitBltHook
    invoke InstallHook, hGdi32, CSTR("StretchBlt"), HOOK_TYPE_WINDOW, MyStretchBltHook
    
    ; Install COM/OLE hooks
    invoke InstallHook, hOle32, CSTR("CoCreateInstance"), HOOK_TYPE_COM, MyCoCreateInstanceHook
    invoke InstallHook, hOle32, CSTR("CoInitialize"), HOOK_TYPE_COM, MyCoInitializeHook
    invoke InstallHook, hOle32, CSTR("CoUninitialize"), HOOK_TYPE_COM, MyCoUninitializeHook
    
    ; Install crypto hooks
    invoke InstallHook, hCrypt32, CSTR("CryptEncrypt"), HOOK_TYPE_CRYPTO, MyCryptEncryptHook
    invoke InstallHook, hCrypt32, CSTR("CryptDecrypt"), HOOK_TYPE_CRYPTO, MyCryptDecryptHook
    invoke InstallHook, hCrypt32, CSTR("CryptHashData"), HOOK_TYPE_CRYPTO, MyCryptHashDataHook
    
    ; Activate interceptor
    mov g_Interceptor.IsActive, 1
    
    ; Start streaming thread
    invoke CreateThread, NULL, 0, StreamThreadProc, NULL, 0, NULL
    mov g_hStreamThread, rax
    
    ; Start CLI thread
    invoke CreateThread, NULL, 0, CLIThreadProc, NULL, 0, NULL
    
    ; Log success to PowerShell
    invoke StreamSuccessMessage, CSTR("OS Explorer Interceptor initialized successfully")
    
    mov rax, 1  ; Success
    ret
    
@error:
    mov rax, 0  ; Failure
    ret
InstallOSHooks ENDP

; Install a single hook
InstallHook PROC hModule:QWORD, lpFuncName:PTR BYTE, dwHookType:DWORD, pHookProc:QWORD
    LOCAL pFunc:QWORD
    LOCAL pHookEntry:PTR HookEntry
    LOCAL index:QWORD
    
    ; Get function address
    invoke GetProcAddress, hModule, lpFuncName
    .IF rax == 0
        ret
    .ENDIF
    mov pFunc, rax
    
    ; Find free hook entry
    mov index, 0
    mov rax, OFFSET g_HookTable
    
@find_free:
    .IF index >= MAX_HOOKS
        ret
    .ENDIF
    
    mov pHookEntry, rax
    .IF [pHookEntry].HookEntry.IsActive == 0
        ; Found free entry
        jmp @install
    .ENDIF
    
    add rax, SIZEOF HookEntry
    inc index
    jmp @find_free
    
@install:
    ; Fill hook entry
    invoke lstrcpyA, ADDR [pHookEntry].HookEntry.FunctionName, lpFuncName
    mov [pHookEntry].HookEntry.OriginalAddr, pFunc
    mov [pHookEntry].HookEntry.HookAddr, pHookProc
    mov [pHookEntry].HookEntry.HookType, dwHookType
    mov [pHookEntry].HookEntry.IsActive, 1
    mov [pHookEntry].HookEntry.CallCount, 0
    
    ; Install hook (stealth method - hardware breakpoint)
    invoke InstallHardwareBreakpoint, pFunc, pHookProc
    
    ret
InstallHook ENDP

; Install hardware breakpoint (stealth hooking)
InstallHardwareBreakpoint PROC pTarget:QWORD, pHook:QWORD
    LOCAL dr0:QWORD
    LOCAL dr7:QWORD
    
    ; Use DR0 for first hook, DR1 for second, etc.
    ; DR7 controls which breakpoints are active
    
    ; Set breakpoint address in DR0-DR3
    mov dr0, pTarget
    
    ; Configure DR7:
    ; Bits 0-1: DR0 local/global enable
    ; Bits 16-17: DR0 condition (00 = execute)
    ; Bits 18-19: DR0 length (00 = 1 byte)
    mov dr7, 0x00000001  ; Enable DR0 locally
    
    ; Set Vectored Exception Handler to catch breakpoint
    invoke AddVectoredExceptionHandler, 1, OFFSET VectoredExceptionHandler
    
    ret
InstallHardwareBreakpoint ENDP

; Vectored Exception Handler for hardware breakpoints
VectoredExceptionHandler PROC pExceptionInfo:QWORD
    LOCAL pExceptionRecord:QWORD
    LOCAL pContext:QWORD
    LOCAL exceptionCode:DWORD
    
    mov rax, pExceptionInfo
    mov pExceptionRecord, [rax].EXCEPTION_POINTERS.ExceptionRecord
    mov pContext, [rax].EXCEPTION_POINTERS.ContextRecord
    
    mov rax, pExceptionRecord
    mov exceptionCode, [rax].EXCEPTION_RECORD.ExceptionCode
    
    ; Check if it's a breakpoint exception
    .IF exceptionCode == EXCEPTION_BREAKPOINT
        ; Check which breakpoint was hit (DR6)
        mov rax, dr6
        .IF rax & 0x01  ; DR0 breakpoint
            ; Call hook procedure
            call [pHookTable].HookEntry.HookAddr
            
            ; Clear breakpoint in DR6
            and dr6, 0xFFFFFFFFFFFFFFF0
            
            ; Continue execution
            mov rax, EXCEPTION_CONTINUE_EXECUTION
            ret
        .ENDIF
    .ENDIF
    
    ; Not our exception, continue search
    mov rax, EXCEPTION_CONTINUE_SEARCH
    ret
VectoredExceptionHandler ENDP

;============================================================================
; HOOK PROCEDURES
;============================================================================

; Generic hook procedure template
GenericHook PROC
    LOCAL logEntry:CallLogEntry
    LOCAL startTime:QWORD
    LOCAL endTime:QWORD
    LOCAL hookIndex:QWORD
    
    ; Get start time (RDTSC)
    rdtsc
    shl rdx, 32
    or rax, rdx
    mov startTime, rax
    
    ; Fill log entry
    invoke GetTickCount
    mov logEntry.Timestamp, rax
    
    invoke GetCurrentThreadId
    mov logEntry.ThreadID, eax
    
    mov eax, g_Interceptor.TargetPID
    mov logEntry.ProcessID, eax
    
    ; Get hook index from current hook
    mov hookIndex, 0  ; Will be set by macro
    
    mov rax, OFFSET g_HookTable
    add rax, hookIndex * SIZEOF HookEntry
    mov logEntry.HookType, [rax].HookEntry.HookType
    
    ; Copy function name
    invoke lstrcpyA, ADDR logEntry.FunctionName, ADDR [rax].HookEntry.FunctionName
    
    ; Get parameters from registers/stack
    mov logEntry.Parameters[0], rcx
    mov logEntry.Parameters[8], rdx
    mov logEntry.Parameters[16], r8
    mov logEntry.Parameters[24], r9
    ; ... get rest from stack
    
    mov logEntry.ParamCount, 4  ; Will be adjusted per function
    
    ; Log the call
    invoke LogCall, OFFSET g_Interceptor, ADDR logEntry
    
    ; Call original function
    ; (address stored in hook table)
    mov rax, OFFSET g_HookTable
    add rax, hookIndex * SIZEOF HookEntry
    call [rax].HookEntry.OriginalAddr
    
    mov returnValue, rax
    mov logEntry.ReturnValue, rax
    
    ; Get end time
    rdtsc
    shl rdx, 32
    or rax, rdx
    mov endTime, rax
    
    ; Calculate duration
    sub endTime, startTime
    mov logEntry.Duration, endTime
    
    ; Update statistics
    lock inc g_Interceptor.Stats.TotalCalls
    lock inc [rax].HookEntry.CallCount
    
    ; Return original return value
    mov rax, returnValue
    ret
GenericHook ENDP

; File I/O Hooks
MyCreateFileWHook PROC
    ; Parameters: lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes
    ; Log file access
    invoke LogFileAccess, rcx, rdx
    
    ; Check if sensitive file
    invoke IsSensitiveFile, rcx
    .IF rax == TRUE
        invoke AlertSensitiveFileAccess, rcx
    .ENDIF
    
    jmp GenericHook
MyCreateFileWHook ENDP

MyReadFileHook PROC
    ; Parameters: hFile, lpBuffer, nNumberOfBytesToRead, lpNumberOfBytesRead
    ; Log read operation
    invoke LogFileRead, rcx, rdx, r8
    
    jmp GenericHook
MyReadFileHook ENDP

MyWriteFileHook PROC
    ; Parameters: hFile, lpBuffer, nNumberOfBytesToWrite, lpNumberOfBytesWritten
    ; Log write operation
    invoke LogFileWrite, rcx, rdx, r8
    
    jmp GenericHook
MyWriteFileHook ENDP

MyDeleteFileHook PROC
    ; Parameters: lpFileName
    ; Log file deletion
    invoke LogFileDelete, rcx
    
    jmp GenericHook
MyDeleteFileHook ENDP

MyMoveFileHook PROC
    ; Parameters: lpExistingFileName, lpNewFileName
    ; Log file move
    invoke LogFileMove, rcx, rdx
    
    jmp GenericHook
MyMoveFileHook ENDP

; Registry Hooks
MyRegOpenKeyExWHook PROC
    ; Parameters: hKey, lpSubKey, ulOptions, samDesired, phkResult
    ; Log registry access
    invoke LogRegistryAccess, rcx, rdx
    
    jmp GenericHook
MyRegOpenKeyExWHook ENDP

MyRegQueryValueExWHook PROC
    ; Parameters: hKey, lpValueName, lpReserved, lpType, lpData, lpcbData
    ; Log registry query
    invoke LogRegistryQuery, rcx, rdx
    
    ; Check if sensitive key
    invoke IsSensitiveRegistryKey, rcx
    .IF rax == TRUE
        invoke CaptureRegistryValue, rcx, rdx
    .ENDIF
    
    jmp GenericHook
MyRegQueryValueExWHook ENDP

MyRegSetValueExWHook PROC
    ; Parameters: hKey, lpValueName, Reserved, dwType, lpData, cbData
    ; Log registry write
    invoke LogRegistryWrite, rcx, rdx, r8
    
    jmp GenericHook
MyRegSetValueExWHook ENDP

MyRegCreateKeyExWHook PROC
    ; Parameters: hKey, lpSubKey, Reserved, lpClass, dwOptions, samDesired
    ; Log registry creation
    invoke LogRegistryCreate, rcx, rdx
    
    jmp GenericHook
MyRegCreateKeyExWHook ENDP

MyRegDeleteKeyHook PROC
    ; Parameters: hKey, lpSubKey
    ; Log registry deletion
    invoke LogRegistryDelete, rcx, rdx
    
    jmp GenericHook
MyRegDeleteKeyHook ENDP

; Network Hooks
MyWSAConnectHook PROC
    ; Parameters: s, name, namelen
    ; Log connection attempt
    invoke LogNetworkConnect, rcx, rdx
    
    ; Capture connection info
    invoke CaptureConnectionInfo, rdx
    
    jmp GenericHook
MyWSAConnectHook ENDP

MySendHook PROC
    ; Parameters: s, buf, len, flags
    ; Log data send
    invoke LogNetworkSend, rcx, rdx, r8
    
    ; Capture sent data
    invoke CaptureNetworkData, rdx, r8, PACKET_DIRECTION_OUT
    
    jmp GenericHook
MySendHook ENDP

MyRecvHook PROC
    ; Parameters: s, buf, len, flags
    ; Log data receive
    invoke LogNetworkRecv, rcx, rdx, r8
    
    ; Capture received data
    invoke CaptureNetworkData, rdx, rax, PACKET_DIRECTION_IN
    
    jmp GenericHook
MyRecvHook ENDP

MyGetAddrInfoHook PROC
    ; Parameters: pNodeName, pServiceName, pHints, ppResult
    ; Log DNS resolution
    invoke LogDNSResolution, rcx, rdx
    
    ; Capture hostname
    invoke CaptureHostname, rcx
    
    jmp GenericHook
MyGetAddrInfoHook ENDP

;============================================================================
; LOGGING & STREAMING
;============================================================================

; Log a call
LogCall PROC pInterceptor:QWORD, pLogEntry:PTR CallLogEntry
    LOCAL pInterceptorStruct:PTR OSInterceptor
    LOCAL index:QWORD
    LOCAL pCallLog:PTR CallLogEntry
    
    mov pInterceptorStruct, pInterceptor
    
    ; Get current index (circular buffer)
    mov rax, [pInterceptorStruct].OSInterceptor.CallLogIndex
    mov index, rax
    
    ; Get pointer to log entry in circular buffer
    mov rax, [pInterceptorStruct].OSInterceptor.CallLog
    add rax, index * SIZEOF CallLogEntry
    mov pCallLog, rax
    
    ; Copy log entry to buffer
    invoke memcpy, pCallLog, pLogEntry, SIZEOF CallLogEntry
    
    ; Increment index (circular)
    inc index
    .IF index >= MAX_CALL_LOG
        mov index, 0
    .ENDIF
    mov [pInterceptorStruct].OSInterceptor.CallLogIndex, index
    
    ; Update statistics
    lock inc [pInterceptorStruct].OSInterceptor.Stats.TotalCalls
    
    ; Signal streaming thread
    invoke SetEvent, g_hStreamEvent
    
    ret
LogCall ENDP

; Stream log entry to PowerShell in real-time
StreamToPowerShell PROC pLogEntry:PTR CallLogEntry
    LOCAL pInterceptor:PTR OSInterceptor
    LOCAL buffer[CALL_LOG_BUFFER_SIZE]:BYTE
    LOCAL bytesWritten:DWORD
    LOCAL hStdOut:QWORD
    
    mov pInterceptor, OFFSET g_Interceptor
    mov hStdOut, [pInterceptor].OSInterceptor.hStdOut
    
    ; Format log entry for PowerShell
    invoke FormatLogEntry, pLogEntry, ADDR buffer
    
    ; Write to PowerShell stdout
    invoke WriteFile, hStdOut, ADDR buffer, eax, ADDR bytesWritten, NULL
    
    ; Update bytes streamed statistic
    lock add [pInterceptor].OSInterceptor.Stats.BytesStreamed, eax
    
    ret
StreamToPowerShell ENDP

; Format log entry for PowerShell output
FormatLogEntry PROC pLogEntry:PTR CallLogEntry, pBuffer:PTR BYTE
    LOCAL pEntry:PTR CallLogEntry
    
    mov pEntry, pLogEntry
    
    ; Format: [Timestamp] [PID:TID] [HookType] Function(Params) = ReturnValue [Duration]ns
    invoke wsprintfA, pBuffer, \
           CSTR("[%llu] [%d:%d] [%s] %s("), \
           [pEntry].CallLogEntry.Timestamp, \
           [pEntry].CallLogEntry.ProcessID, \
           [pEntry].CallLogEntry.ThreadID, \
           GetHookTypeName([pEntry].CallLogEntry.HookType), \
           ADDR [pEntry].CallLogEntry.FunctionName
    
    ; Add parameters
    .IF [pEntry].CallLogEntry.ParamCount > 0
        .FOR i = 0 TO [pEntry].CallLogEntry.ParamCount - 1
            .IF i < 8
                invoke wsprintfA, pBuffer + strlen(pBuffer), \
                       CSTR("0x%llx, "), [pEntry].CallLogEntry.Parameters[i * 8]
            .ENDIF
        .ENDF
    .ENDIF
    
    ; Add return value and duration
    invoke wsprintfA, pBuffer + strlen(pBuffer), \
           CSTR(") = 0x%llx [%llu]ns\n"), \
           [pEntry].CallLogEntry.ReturnValue, \
           [pEntry].CallLogEntry.Duration
    
    ; Return length
    invoke strlen, pBuffer
    ret
FormatLogEntry ENDP

; Get hook type name for display
GetHookTypeName PROC dwHookType:DWORD
    .IF dwHookType == HOOK_TYPE_FILE
        mov rax, CSTR("FILE")
    .ELSEIF dwHookType == HOOK_TYPE_REGISTRY
        mov rax, CSTR("REGISTRY")
    .ELSEIF dwHookType == HOOK_TYPE_PROCESS
        mov rax, CSTR("PROCESS")
    .ELSEIF dwHookType == HOOK_TYPE_MEMORY
        mov rax, CSTR("MEMORY")
    .ELSEIF dwHookType == HOOK_TYPE_NETWORK
        mov rax, CSTR("NETWORK")
    .ELSEIF dwHookType == HOOK_TYPE_WINDOW
        mov rax, CSTR("WINDOW")
    .ELSEIF dwHookType == HOOK_TYPE_COM
        mov rax, CSTR("COM")
    .ELSEIF dwHookType == HOOK_TYPE_CRYPTO
        mov rax, CSTR("CRYPTO")
    .ELSE
        mov rax, CSTR("UNKNOWN")
    .ENDIF
    ret
GetHookTypeName ENDP

; Stream success message to PowerShell
StreamSuccessMessage PROC pMessage:PTR BYTE
    LOCAL buffer[256]:BYTE
    LOCAL bytesWritten:DWORD
    
    invoke wsprintfA, ADDR buffer, CSTR("[SUCCESS] %s\n"), pMessage
    invoke WriteFile, g_Interceptor.hStdOut, ADDR buffer, eax, ADDR bytesWritten, NULL
    
    ret
StreamSuccessMessage ENDP

; Stream error message to PowerShell
StreamErrorMessage PROC pMessage:PTR BYTE
    LOCAL buffer[256]:BYTE
    LOCAL bytesWritten:DWORD
    
    invoke wsprintfA, ADDR buffer, CSTR("[ERROR] %s\n"), pMessage
    invoke WriteFile, g_Interceptor.hStdErr, ADDR buffer, eax, ADDR bytesWritten, NULL
    
    ret
StreamErrorMessage ENDP

;============================================================================
; STREAMING THREAD
;============================================================================

; Streaming thread procedure - continuously streams data to PowerShell
StreamThreadProc PROC pParam:QWORD
    LOCAL pInterceptor:PTR OSInterceptor
    LOCAL index:QWORD
    LOCAL lastIndex:QWORD
    
    mov pInterceptor, OFFSET g_Interceptor
    mov lastIndex, 0
    
    .WHILE [pInterceptor].OSInterceptor.IsActive == 1
        ; Get current index
        mov rax, [pInterceptor].OSInterceptor.CallLogIndex
        mov index, rax
        
        ; Check if new entries available
        .IF index != lastIndex
            ; Process new entries
            mov rax, lastIndex
            .WHILE rax != index
                ; Get log entry
                mov rdx, [pInterceptor].OSInterceptor.CallLog
                add rdx, rax * SIZEOF CallLogEntry
                
                ; Stream to PowerShell
                invoke StreamToPowerShell, rdx
                
                ; Increment and wrap
                inc rax
                .IF rax >= MAX_CALL_LOG
                    mov rax, 0
                .ENDIF
            .ENDW
            
            mov lastIndex, index
        .ENDIF
        
        ; Sleep briefly (10ms)
        invoke Sleep, 10
    .ENDW
    
    ret
StreamThreadProc ENDP

;============================================================================
; CLI INTERFACE
;============================================================================

; CLI thread procedure - handles PowerShell commands
CLIThreadProc PROC pParam:QWORD
    LOCAL buffer[1024]:BYTE
    LOCAL bytesRead:DWORD
    
    .WHILE g_IsCLIActive == 1
        ; Read command from PowerShell stdin
        invoke ReadFile, g_hPowerShellInput, ADDR buffer, 1024, ADDR bytesRead, NULL
        
        .IF bytesRead > 0
            ; Null-terminate
            mov byte ptr [buffer + bytesRead], 0
            
            ; Process command
            invoke ProcessCLICommand, ADDR buffer
        .ENDIF
        
        ; Sleep briefly
        invoke Sleep, 100
    .ENDW
    
    ret
CLIThreadProc ENDP

; Process CLI command from PowerShell
ProcessCLICommand PROC pCommand:PTR BYTE
    LOCAL command[256]:BYTE
    
    ; Copy command
    invoke lstrcpyA, ADDR command, pCommand
    
    ; Parse command
    invoke lstrcmpiA, ADDR command, CSTR("start")
    .IF rax == 0
        invoke StartInterceptor
        ret
    .ENDIF
    
    invoke lstrcmpiA, ADDR command, CSTR("stop")
    .IF rax == 0
        invoke StopInterceptor
        ret
    .ENDIF
    
    invoke lstrcmpiA, ADDR command, CSTR("status")
    .IF rax == 0
        invoke ShowStatus
        ret
    .ENDIF
    
    invoke lstrcmpiA, ADDR command, CSTR("stats")
    .IF rax == 0
        invoke ShowStats
        ret
    .ENDIF
    
    invoke lstrcmpiA, ADDR command, CSTR("clear")
    .IF rax == 0
        invoke ClearLog
        ret
    .ENDIF
    
    invoke lstrcmpiA, ADDR command, CSTR("help")
    .IF rax == 0
        invoke ShowHelp
        ret
    .ENDIF
    
    ; Unknown command
    invoke StreamErrorMessage, CSTR("Unknown command. Type 'help' for available commands.")
    
    ret
ProcessCLICommand ENDP

; Start interceptor
StartInterceptor PROC
    .IF g_Interceptor.IsActive == 0
        mov g_Interceptor.IsActive, 1
        invoke StreamSuccessMessage, CSTR("OS Explorer Interceptor started")
    .ELSE
        invoke StreamErrorMessage, CSTR("Interceptor already running")
    .ENDIF
    ret
StartInterceptor ENDP

; Stop interceptor
StopInterceptor PROC
    .IF g_Interceptor.IsActive == 1
        mov g_Interceptor.IsActive, 0
        invoke StreamSuccessMessage, CSTR("OS Explorer Interceptor stopped")
    .ELSE
        invoke StreamErrorMessage, CSTR("Interceptor not running")
    .ENDIF
    ret
StopInterceptor ENDP

; Show status
ShowStatus PROC
    LOCAL buffer[256]:BYTE
    
    invoke wsprintfA, ADDR buffer, \
           CSTR("Status: %s\nTarget PID: %d\nTotal Calls: %llu\n"), \
           g_Interceptor.IsActive ? CSTR("RUNNING") : CSTR("STOPPED"), \
           g_Interceptor.TargetPID, \
           g_Interceptor.Stats.TotalCalls
    
    invoke StreamToPowerShell, ADDR buffer
    ret
ShowStatus ENDP

; Show statistics
ShowStats PROC
    LOCAL buffer[512]:BYTE
    LOCAL i:QWORD
    
    ; Overall stats
    invoke wsprintfA, ADDR buffer, \
           CSTR("=== STATISTICS ===\nTotal Calls: %llu\nBytes Streamed: %llu\nError Count: %llu\n"), \
           g_Interceptor.Stats.TotalCalls, \
           g_Interceptor.Stats.BytesStreamed, \
           g_Interceptor.Stats.ErrorCount
    
    invoke StreamToPowerShell, ADDR buffer
    
    ; Per-hook stats
    mov i, 0
    .WHILE i < MAX_HOOKS
        mov rax, OFFSET g_HookTable
        add rax, i * SIZEOF HookEntry
        
        .IF [rax].HookEntry.IsActive == 1
            invoke wsprintfA, ADDR buffer, \
                   CSTR("%s: %llu calls\n"), \
                   ADDR [rax].HookEntry.FunctionName, \
                   [rax].HookEntry.CallCount
            
            invoke StreamToPowerShell, ADDR buffer
        .ENDIF
        
        inc i
    .ENDW
    
    ret
ShowStats ENDP

; Clear log
ClearLog PROC
    mov g_Interceptor.CallLogIndex, 0
    invoke StreamSuccessMessage, CSTR("Call log cleared")
    ret
ClearLog ENDP

; Show help
ShowHelp PROC
    LOCAL helpText:PTR BYTE
    
    mov helpText, CSTR(
        "=== OS Explorer Interceptor CLI ===\n"
        "Commands:\n"
        "  start   - Start the interceptor\n"
        "  stop    - Stop the interceptor\n"
        "  status  - Show current status\n"
        "  stats   - Show statistics\n"
        "  clear   - Clear the call log\n"
        "  help    - Show this help\n"
        "\n"
        "Example usage:\n"
        "  PS> .\\os_interceptor.exe 1234\n"
        "  PS> start\n"
        "  PS> stats\n"
        "  PS> stop\n"
    )
    
    invoke StreamToPowerShell, helpText
    ret
ShowHelp ENDP

;============================================================================
; UTILITY FUNCTIONS
;============================================================================

; Get string length
strlen PROC pString:PTR BYTE
    LOCAL pStr:PTR BYTE
    LOCAL len:QWORD
    
    mov pStr, pString
    mov len, 0
    
@loop:
    mov al, [pStr]
    .IF al == 0
        mov rax, len
        ret
    .ENDIF
    
    inc len
    inc pStr
    jmp @loop
strlen ENDP

; Copy memory
memcpy PROC pDest:PTR BYTE, pSrc:PTR BYTE, size:QWORD
    LOCAL pD:PTR BYTE
    LOCAL pS:PTR BYTE
    LOCAL count:QWORD
    
    mov pD, pDest
    mov pS, pSrc
    mov count, size
    
    .IF count == 0
        ret
    .ENDIF
    
@loop:
    mov al, [pS]
    mov [pD], al
    
    inc pS
    inc pD
    dec count
    
    .IF count > 0
        jmp @loop
    .ENDIF
    
    ret
memcpy ENDP

;============================================================================
; DLL MAIN
;============================================================================

DllMain PROC hInstDLL:QWORD, fdwReason:DWORD, lpvReserved:QWORD
    .IF fdwReason == DLL_PROCESS_ATTACH
        ; Initialize interceptor
        invoke InitOSInterceptor
        
        ; Disable thread notifications for performance
        invoke DisableThreadLibraryCalls, hInstDLL
        
    .ELSEIF fdwReason == DLL_THREAD_ATTACH
        ; Thread-specific initialization if needed
        
    .ELSEIF fdwReason == DLL_THREAD_DETACH
        ; Thread-specific cleanup if needed
        
    .ELSEIF fdwReason == DLL_PROCESS_DETACH
        ; Cleanup
        mov g_Interceptor.IsActive, 0
        mov g_IsCLIActive, 0
        
        ; Close handles
        .IF g_hStreamEvent != 0
            invoke CloseHandle, g_hStreamEvent
        .ENDIF
        
        .IF g_hTargetProcess != 0
            invoke CloseHandle, g_hTargetProcess
        .ENDIF
    .ENDIF
    
    mov rax, TRUE
    ret
DllMain ENDP

;============================================================================
; EXPORTED FUNCTIONS
;============================================================================

; Export functions for CLI control
PUBLIC InstallOSHooks
PUBLIC StartInterceptor
PUBLIC StopInterceptor
PUBLIC ShowStatus
PUBLIC ShowStats
PUBLIC ClearLog
PUBLIC ShowHelp

; Export for PowerShell module
PUBLIC StreamToPowerShell
PUBLIC StreamSuccessMessage
PUBLIC StreamErrorMessage

; Export initialization and lifecycle
PUBLIC InitOSInterceptor
PUBLIC UnloadInterceptor
PUBLIC ExportCapturedData
PUBLIC InstallHook
PUBLIC FormatLogEntry
PUBLIC DllMain

END
