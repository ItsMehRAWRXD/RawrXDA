;============================================================================
; OS Explorer Memory Reader - Universal Version v4.0
; Process enumeration and memory analysis tool
;============================================================================

.386
.model flat, stdcall
option casemap :none

;============================================================================
; INCLUDES
;============================================================================

include windows.inc
include kernel32.inc
include user32.inc
include advapi32.inc
include psapi.inc
include tlhelp32.inc

includelib kernel32.lib
includelib user32.lib
includelib advapi32.lib
includelib psapi.lib

;============================================================================
; CONSTANTS
;============================================================================

CLI_VERSION             equ "4.0.0"
MAX_PATH                equ 260
MAX_PROCESSES           equ 1024
MEM_COMMIT              equ 00001000h
PAGE_READWRITE          equ 00000004h
PAGE_EXECUTE_READWRITE equ 00000040h
PAGE_EXECUTE_READ       equ 00000020h
PAGE_READONLY           equ 00000002h
INFINITE                equ 0FFFFFFFFh
PROCESS_ALL_ACCESS      equ 001F0FFFh
PROCESS_VM_READ         equ 00000010h
PROCESS_QUERY_INFORMATION equ 00000400h
TH32CS_SNAPPROCESS      equ 00000002h
CREATE_SUSPENDED        equ 00000004h
MEM_DUMP_SIZE           equ 4096

;============================================================================
; DATA
;============================================================================

.data

szWelcome               db "OS Explorer Memory Reader v", CLI_VERSION, 0Dh, 0Ah
                        db "========================================", 0Dh, 0Ah, 0Dh, 0Ah, 0
szMenu                  db "[1] List all processes", 0Dh, 0Ah
                        db "[2] Scan process memory", 0Dh, 0Ah
                        db "[3] Dump memory region", 0Dh, 0Ah
                        db "[4] Find pattern in memory", 0Dh, 0Ah
                        db "[5] Exit", 0Dh, 0Ah
                        db "Select option: ", 0
szPromptPid             db "Enter PID: ", 0
szPromptAddress         db "Enter address (hex): ", 0
szPromptSize            db "Enter size: ", 0
szPromptPattern         db "Enter pattern (hex): ", 0
szProcessesHeader       db 0Dh, 0Ah, "PID\t\tProcess Name", 0Dh, 0Ah
                        db "========================================", 0Dh, 0Ah, 0
szProcessEntry          db "%d\t\t%s", 0Dh, 0Ah, 0
szMemoryHeader          db 0Dh, 0Ah, "Address\t\tSize\t\tState\t\tProtect", 0Dh, 0Ah
                        db "========================================", 0Dh, 0Ah, 0
szMemoryEntry           db "%08X\t%X\t%d\t%X", 0Dh, 0Ah, 0
szDumpHeader            db 0Dh, 0Ah, "Memory dump:", 0Dh, 0Ah
                        db "========================================", 0Dh, 0Ah, 0
szPatternFound          db "[+] Pattern found at: %08X", 0Dh, 0Ah, 0
szError                 db "[-] ERROR: ", 0
szErrorOpenProcess      db "Failed to open process.", 0Dh, 0Ah, 0
szErrorReadMemory       db "Failed to read memory.", 0Dh, 0Ah, 0
szErrorInvalidPid       db "Invalid PID.", 0Dh, 0Ah, 0
szErrorNoAccess         db "No access to process.", 0Dh, 0Ah, 0
szSuccess               db "[+] Operation completed successfully.", 0Dh, 0Ah, 0
szPressAnyKey           db 0Dh, 0Ah, "Press any key to continue...", 0
szFormatHex             db "%08X", 0
szFormatDec             db "%d", 0
szFormatString          db "%s", 0
szBuffer                db 256 dup(0)
szHexBuffer             db 16 dup(0)
processIds              dd MAX_PROCESSES dup(0)
memoryBuffer            db MEM_DUMP_SIZE dup(0)
hStdIn                  dd 0
hStdOut                 dd 0

;============================================================================
; CODE
;============================================================================

.code

;----------------------------------------------------------------------------
; Get current directory and build full DLL path
;----------------------------------------------------------------------------
GetDllPath PROC
    LOCAL hModule :DWORD
    
    ; Get module handle (this executable)
    invoke GetModuleHandle, NULL
    mov hModule, eax
    
    ; Get module file name to extract directory
    invoke GetModuleFileName, hModule, addr szDllPath, MAX_PATH
    
    ; Find last backslash and replace with null to get directory
    mov ecx, eax
    dec ecx
    lea edx, szDllPath
    add edx, ecx

@@find_backslash:
    cmp byte ptr [edx], 5Ch  ; Backslash character (\)
    je @@found
    dec edx
    dec ecx
    jns @@find_backslash
    jmp @@done
    
@@found:
    mov byte ptr [edx+1], 0
    
@@done:
    ; Append DLL name
    invoke lstrcat, addr szDllPath, addr szInterceptorDll
    
    ret
GetDllPath ENDP

;----------------------------------------------------------------------------
; Check if process is x64 (returns TRUE if x64, FALSE if x86)
;----------------------------------------------------------------------------
IsX64Process PROC hProcess:DWORD
    LOCAL hKernel32 :DWORD
    LOCAL pIsWow64Process :DWORD
    LOCAL bIsWow64 :DWORD
    
    ; Get kernel32 handle
    invoke GetModuleHandle, addr szKernel32
    mov hKernel32, eax
    
    ; Get IsWow64Process address
    invoke GetProcAddress, hKernel32, addr szIsWow64Process
    mov pIsWow64Process, eax
    cmp eax, NULL
    je @@error  ; Function not available (older Windows)
    
    ; Check if process is running under WOW64
    lea eax, bIsWow64
    push eax
    push hProcess
    call pIsWow64Process
    cmp eax, 0
    je @@error
    
    ; If bIsWow64 is TRUE, it's an x86 process running on x64
    ; If FALSE, it's either x64 or x86 on x86
    mov eax, bIsWow64
    cmp eax, 0
    jne @@is_x86
    
    ; Check if we're on x64 Windows
    invoke GetModuleHandle, addr szKernel32
    mov hKernel32, eax
    invoke GetProcAddress, hKernel32, addr szIsWow64Process
    cmp eax, NULL
    je @@is_x86  ; Function not available, must be x86 Windows
    
    ; We're on x64 Windows and process is not WOW64, so it's x64
    mov eax, TRUE
    ret
    
@@is_x86:
    mov eax, FALSE
    ret
    
@@error:
    mov eax, FALSE  ; Default to x86 on error
    ret
IsX64Process ENDP

;----------------------------------------------------------------------------
; Find existing Cursor process (simplified version without toolhelp)
; In production, you would use EnumProcesses or toolhelp32
;----------------------------------------------------------------------------
FindCursorProcess PROC
    LOCAL dwPid :DWORD
    
    ; For now, return 0 (no existing process)
    ; In a full implementation, you would:
    ; 1. Use EnumProcesses to get all process IDs
    ; 2. Open each process and get its name
    ; 3. Compare with "Cursor.exe"
    mov dwPid, 0
    mov eax, dwPid
    ret
FindCursorProcess ENDP

;----------------------------------------------------------------------------
; Inject DLL using CreateRemoteThread
;----------------------------------------------------------------------------
InjectDll_CreateRemoteThread PROC hProcess:DWORD, lpDllPath:DWORD, pLoadLibrary:DWORD
    LOCAL pRemoteMem :DWORD
    LOCAL hThread :DWORD
    LOCAL dwWritten :DWORD
    LOCAL dwDllLen :DWORD
    LOCAL dwResult :DWORD
    
    ; Get DLL path length
    invoke lstrlen, lpDllPath
    inc eax
    mov dwDllLen, eax
    
    ; Allocate memory in target process
    invoke VirtualAllocEx, hProcess, NULL, dwDllLen, MEM_COMMIT, PAGE_READWRITE
    cmp eax, NULL
    je @@error
    mov pRemoteMem, eax
    
    ; Write DLL path to target process
    invoke WriteProcessMemory, hProcess, pRemoteMem, lpDllPath, dwDllLen, addr dwWritten
    cmp eax, 0
    je @@error_free_mem
    
    ; Create remote thread to load DLL
    invoke CreateRemoteThread, hProcess, NULL, 0, pLoadLibrary, pRemoteMem, 0, NULL
    cmp eax, NULL
    je @@error_free_mem
    mov hThread, eax
    
    ; Wait for thread to complete
    invoke WaitForSingleObject, hThread, INFINITE
    
    ; Cleanup
    invoke CloseHandle, hThread
    invoke VirtualFreeEx, hProcess, pRemoteMem, 0, MEM_RELEASE
    
    mov eax, TRUE
    ret
    
@@error_free_mem:
    invoke VirtualFreeEx, hProcess, pRemoteMem, 0, MEM_RELEASE
@@error:
    mov eax, FALSE
    ret
InjectDll_CreateRemoteThread ENDP

;----------------------------------------------------------------------------
; Inject DLL using NtCreateThreadEx (undocumented but more reliable)
;----------------------------------------------------------------------------
InjectDll_NtCreateThreadEx PROC hProcess:DWORD, lpDllPath:DWORD, pLoadLibrary:DWORD
    LOCAL pRemoteMem :DWORD
    LOCAL hThread :DWORD
    LOCAL dwWritten :DWORD
    LOCAL dwDllLen :DWORD
    LOCAL hNtdll :DWORD
    LOCAL pNtCreateThreadEx :DWORD
    LOCAL ntStatus :DWORD
    LOCAL dwResult :DWORD
    
    ; Get ntdll handle and NtCreateThreadEx address
    invoke GetModuleHandle, addr szNtdll
    mov hNtdll, eax
    invoke GetProcAddress, hNtdll, addr szNtCreateThreadEx
    mov pNtCreateThreadEx, eax
    cmp eax, NULL
    je @@error
    
    ; Get DLL path length
    invoke lstrlen, lpDllPath
    inc eax
    mov dwDllLen, eax
    
    ; Allocate memory in target process
    invoke VirtualAllocEx, hProcess, NULL, dwDllLen, MEM_COMMIT, PAGE_READWRITE
    cmp eax, NULL
    je @@error
    mov pRemoteMem, eax
    
    ; Write DLL path to target process
    invoke WriteProcessMemory, hProcess, pRemoteMem, lpDllPath, dwDllLen, addr dwWritten
    cmp eax, 0
    je @@error_free_mem
    
    ; Create thread using NtCreateThreadEx
    ; Arguments: (handle, access, objattr, process, startaddr, param, flags, zero, zero, zero, attr)
    push 0                      ; AttributeList
    push 0                      ; AttributeList
    push 0                      ; AttributeList
    push 0                      ; CreateSuspended = FALSE
    push 0                      ; ZeroBits
    push 0                      ; StackCommit
    push 0                      ; StackReserve
    push pRemoteMem             ; Parameter
    push pLoadLibrary           ; StartAddress
    push hProcess               ; ProcessHandle
    lea eax, hThread
    push eax                    ; ThreadHandle
    call pNtCreateThreadEx
    mov ntStatus, eax
    cmp ntStatus, 0
    jne @@error_free_mem
    
    ; Wait for thread to complete
    invoke WaitForSingleObject, hThread, INFINITE
    
    ; Cleanup
    invoke CloseHandle, hThread
    invoke VirtualFreeEx, hProcess, pRemoteMem, 0, MEM_RELEASE
    
    mov eax, TRUE
    ret
    
@@error_free_mem:
    invoke VirtualFreeEx, hProcess, pRemoteMem, 0, MEM_RELEASE
@@error:
    mov eax, FALSE
    ret
InjectDll_NtCreateThreadEx ENDP

;----------------------------------------------------------------------------
; Inject DLL using QueueUserAPC
;----------------------------------------------------------------------------
InjectDll_QueueUserAPC PROC hProcess:DWORD, lpDllPath:DWORD, pLoadLibrary:DWORD
    LOCAL pRemoteMem :DWORD
    LOCAL hThread :DWORD
    LOCAL dwWritten :DWORD
    LOCAL dwDllLen :DWORD
    LOCAL hSnapshot :DWORD
    LOCAL te32 :THREADENTRY32
    LOCAL bFound :DWORD
    LOCAL dwResult :DWORD
    
    ; Get DLL path length
    invoke lstrlen, lpDllPath
    inc eax
    mov dwDllLen, eax
    
    ; Allocate memory in target process
    invoke VirtualAllocEx, hProcess, NULL, dwDllLen, MEM_COMMIT, PAGE_READWRITE
    cmp eax, NULL
    je @@error
    mov pRemoteMem, eax
    
    ; Write DLL path to target process
    invoke WriteProcessMemory, hProcess, pRemoteMem, lpDllPath, dwDllLen, addr dwWritten
    cmp eax, 0
    je @@error_free_mem
    
    ; Find a thread in the target process to queue APC to
    invoke CreateToolhelp32Snapshot, TH32CS_SNAPTHREAD, 0
    cmp eax, -1
    je @@error_free_mem
    mov hSnapshot, eax
    
    mov te32.dwSize, SIZEOF THREADENTRY32
    invoke Thread32First, hSnapshot, addr te32
    cmp eax, 0
    je @@error_close_snapshot
    
    mov bFound, FALSE
@@find_thread:
    mov eax, te32.th32OwnerProcessID
    cmp eax, dwCursorPidGlobal
    jne @@next_thread
    
    ; Found a thread in our target process
    invoke OpenThread, THREAD_SET_CONTEXT OR THREAD_QUERY_INFORMATION, FALSE, te32.th32ThreadID
    cmp eax, NULL
    je @@next_thread
    mov hThread, eax
    mov bFound, TRUE
    jmp @@found_thread
    
@@next_thread:
    invoke Thread32Next, hSnapshot, addr te32
    cmp eax, 0
    jne @@find_thread
    
@@found_thread:
    invoke CloseHandle, hSnapshot
    cmp bFound, FALSE
    je @@error_free_mem
    
    ; Queue APC to the thread
    invoke QueueUserAPC, pLoadLibrary, hThread, pRemoteMem
    cmp eax, 0
    je @@error_close_thread
    
    ; Trigger APC by alerting the thread
    invoke ResumeThread, hThread
    
    ; Give it a moment to execute
    invoke Sleep, 100
    
    ; Cleanup
    invoke CloseHandle, hThread
    invoke VirtualFreeEx, hProcess, pRemoteMem, 0, MEM_RELEASE
    
    mov eax, TRUE
    ret
    
@@error_close_thread:
    invoke CloseHandle, hThread
@@error_close_snapshot:
    invoke CloseHandle, hSnapshot
@@error_free_mem:
    invoke VirtualFreeEx, hProcess, pRemoteMem, 0, MEM_RELEASE
@@error:
    mov eax, FALSE
    ret
InjectDll_QueueUserAPC ENDP

;----------------------------------------------------------------------------
; Main injection chain - tries all methods until one succeeds
;----------------------------------------------------------------------------
InjectDll PROC hProcess:DWORD, lpDllPath:DWORD, dwTargetPid:DWORD
    LOCAL hKernel32 :DWORD
    LOCAL pLoadLibrary :DWORD
    LOCAL hNtdll :DWORD
    LOCAL dwResult :DWORD
    
    ; Get kernel32 handle and LoadLibraryA address
    invoke GetModuleHandle, addr szKernel32
    mov hKernel32, eax
    invoke GetProcAddress, hKernel32, addr szLoadLibrary
    mov pLoadLibrary, eax
    
    ; Try CreateRemoteThread first
    invoke WriteConsole, hStdOutGlobal, addr szTryingCRT, SIZEOF szTryingCRT, addr dwResult, NULL
    invoke InjectDll_CreateRemoteThread, hProcess, lpDllPath, pLoadLibrary
    cmp eax, TRUE
    je @@success
    
    ; Try NtCreateThreadEx second
    invoke WriteConsole, hStdOutGlobal, addr szTryingNTE, SIZEOF szTryingNTE, addr dwResult, NULL
    invoke InjectDll_NtCreateThreadEx, hProcess, lpDllPath, pLoadLibrary
    cmp eax, TRUE
    je @@success
    
    ; Try QueueUserAPC third
    invoke WriteConsole, hStdOutGlobal, addr szTryingAPC, SIZEOF szTryingAPC, addr dwResult, NULL
    invoke InjectDll_QueueUserAPC, hProcess, lpDllPath, pLoadLibrary
    cmp eax, TRUE
    je @@success
    
    ; All methods failed
    mov eax, FALSE
    ret
    
@@success:
    mov eax, TRUE
    ret
InjectDll ENDP

;----------------------------------------------------------------------------
; Format and display message with PID
;----------------------------------------------------------------------------
DisplayMessageWithPid PROC hStdOut:DWORD, lpMessage:DWORD, dwPid:DWORD
    LOCAL szBuffer[256]:BYTE
    LOCAL dwWritten :DWORD
    LOCAL dwLen :DWORD
    
    ; Format message
    invoke wsprintf, addr szBuffer, lpMessage, dwPid
    mov dwLen, eax
    
    ; Write to console
    invoke WriteConsole, hStdOut, addr szBuffer, dwLen, addr dwWritten, NULL
    
    ret
DisplayMessageWithPid ENDP

;----------------------------------------------------------------------------
; Main entry point
;----------------------------------------------------------------------------
main PROC
    LOCAL hStdOut :DWORD
    LOCAL startupInfo :STARTUPINFO
    LOCAL processInfo :PROCESS_INFORMATION
    LOCAL dwCursorPid :DWORD
    LOCAL hCursorProcess :DWORD
    LOCAL dwSize :DWORD
    LOCAL dwResult :DWORD
    LOCAL dwExitCode :DWORD

    ; Get standard output handle
    invoke GetStdHandle, STD_OUTPUT_HANDLE
    mov hStdOut, eax
    mov hStdOutGlobal, eax      ; Store in global for injection procedures
    
    ; Display welcome message
    mov dwSize, SIZEOF szWelcome
    invoke WriteConsole, hStdOut, addr szWelcome, dwSize, addr dwResult, NULL

    ; Get DLL path
    call GetDllPath

    ; Display checking message
    mov dwSize, SIZEOF szChecking
    invoke WriteConsole, hStdOut, addr szChecking, dwSize, addr dwResult, NULL

    ; Check for existing Cursor process
    call FindCursorProcess
    mov dwCursorPid, eax
    mov dwCursorPidGlobal, eax  ; Store in global for injection procedures
    cmp eax, 0
    jne @@attach_existing

    ; No existing process, check if Cursor executable exists
    invoke GetFileAttributes, addr szCursorPath
    cmp eax, -1
    je @@error_notfound

    ; Launch new Cursor instance
    mov dwSize, SIZEOF szLaunching
    invoke WriteConsole, hStdOut, addr szLaunching, dwSize, addr dwResult, NULL

    ; Initialize structures
    lea eax, startupInfo
    push SIZEOF STARTUPINFO
    push eax
    call RtlZeroMemory
    lea eax, processInfo
    push SIZEOF PROCESS_INFORMATION
    push eax
    call RtlZeroMemory
    mov eax, SIZEOF STARTUPINFO
    mov startupInfo.cb, eax

    ; Launch Cursor in suspended state
    lea eax, startupInfo
    push eax
    lea eax, processInfo
    push eax
    push NULL
    push NULL
    push FALSE
    push CREATE_SUSPENDED
    push NULL
    push NULL
    push NULL
    lea eax, szCursorPath
    push eax
    call CreateProcess
    cmp eax, 0
    je @@error_launch

    ; Get process handle and PID
    mov eax, processInfo.hProcess
    mov hCursorProcess, eax
    mov eax, processInfo.dwProcessId
    mov dwCursorPid, eax
    mov dwCursorPidGlobal, eax  ; Update global with actual PID

    jmp @@inject_dll

@@attach_existing:
    ; Display found message with PID
    mov dwSize, SIZEOF szFoundExisting
    invoke DisplayMessageWithPid, hStdOut, addr szFoundExisting, dwCursorPid

    ; Open existing process
    invoke OpenProcess, PROCESS_ALL_ACCESS, FALSE, dwCursorPid
    cmp eax, NULL
    je @@error_find
    mov hCursorProcess, eax

@@inject_dll:
    ; Display injecting message
    mov dwSize, SIZEOF szInjecting
    invoke WriteConsole, hStdOut, addr szInjecting, dwSize, addr dwResult, NULL

    ; Inject DLL
    lea eax, szDllPath
    push eax
    mov eax, dwCursorPid
    push eax
    mov eax, hCursorProcess
    push eax
    call InjectDll
    cmp eax, FALSE
    je @@error_inject

    ; Close process handle if we opened it
    mov eax, dwCursorPid
    cmp eax, processInfo.dwProcessId
    je @@skip_close  ; Don't close if it's our launched process
    mov eax, hCursorProcess
    push eax
    call CloseHandle
@@skip_close:

    ; Display success
    mov dwSize, SIZEOF szSuccess
    invoke WriteConsole, hStdOut, addr szSuccess, dwSize, addr dwResult, NULL

    jmp @@exit

@@error_notfound:
    mov dwSize, SIZEOF szError
    invoke WriteConsole, hStdOut, addr szError, dwSize, addr dwResult, NULL
    mov dwSize, SIZEOF szErrorNotFound
    invoke WriteConsole, hStdOut, addr szErrorNotFound, dwSize, addr dwResult, NULL
    mov dwExitCode, 1
    jmp @@exit

@@error_launch:
    mov dwSize, SIZEOF szError
    invoke WriteConsole, hStdOut, addr szError, dwSize, addr dwResult, NULL
    mov dwSize, SIZEOF szErrorLaunch
    invoke WriteConsole, hStdOut, addr szErrorLaunch, dwSize, addr dwResult, NULL
    mov dwExitCode, 1
    jmp @@exit

@@error_find:
    mov dwSize, SIZEOF szError
    invoke WriteConsole, hStdOut, addr szError, dwSize, addr dwResult, NULL
    mov dwSize, SIZEOF szErrorFind
    invoke WriteConsole, hStdOut, addr szErrorFind, dwSize, addr dwResult, NULL
    mov dwExitCode, 1
    jmp @@exit

@@error_inject:
    mov dwSize, SIZEOF szError
    invoke WriteConsole, hStdOut, addr szError, dwSize, addr dwResult, NULL
    mov dwSize, SIZEOF szErrorInject
    invoke WriteConsole, hStdOut, addr szErrorInject, dwSize, addr dwResult, NULL
    mov dwExitCode, 1
    jmp @@exit

@@exit:
    mov eax, dwExitCode
    push eax
    call ExitProcess
main ENDP

END main
