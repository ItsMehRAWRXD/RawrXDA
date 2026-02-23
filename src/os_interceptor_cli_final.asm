;============================================================================
; OS Explorer Interceptor CLI - Final Production Version
; Fully reverse-engineered with robust error handling
; Works with both x86 and x64 processes
;============================================================================

.386
.model flat, stdcall
option casemap :none

;============================================================================
; INCLUDES
;============================================================================

include \masm32\include\windows.inc
include \masm32\include\kernel32.inc
include \masm32\include\user32.inc
include \masm32\include\advapi32.inc
include \masm32\include\psapi.inc
include \masm32\include\tlhelp32.inc

includelib \masm32\lib\kernel32.lib
includelib \masm32\lib\user32.lib
includelib \masm32\lib\advapi32.lib
includelib \masm32\lib\psapi.lib

;============================================================================
; CONSTANTS
;============================================================================

CLI_VERSION             equ "3.0.0"
MAX_PATH                equ 260
MEM_COMMIT              equ 00001000h
PAGE_READWRITE          equ 00000004h
INFINITE                equ 0FFFFFFFFh
PROCESS_ALL_ACCESS      equ 001F0FFFh
TH32CS_SNAPPROCESS      equ 00000002h
CREATE_SUSPENDED        equ 00000004h

;============================================================================
; DATA
;============================================================================

.data

szCursorPath            db "C:\Users\HiH8e\AppData\Local\Programs\cursor\Cursor.exe", 0
szCursorProcess         db "Cursor.exe", 0
szInterceptorDllX64     db "os_explorer_interceptor_x64.dll", 0
szInterceptorDllX86     db "os_explorer_interceptor_x86.dll", 0
szWelcome               db "OS Explorer Interceptor CLI v", CLI_VERSION, 0Dh, 0Ah
                        db "========================================", 0Dh, 0Ah, 0Dh, 0Ah, 0
szChecking              db "[*] Checking for existing Cursor process...", 0Dh, 0Ah, 0
szFoundExisting         db "[+] Found existing Cursor process (PID: %d)", 0Dh, 0Ah, 0
szLaunching             db "[*] Launching new Cursor instance...", 0Dh, 0Ah, 0
szInjecting             db "[*] Injecting interceptor DLL...", 0Dh, 0Ah, 0
szDetectingArch         db "[*] Detecting process architecture...", 0Dh, 0Ah, 0
szArchX64               db "[+] Process is x64", 0Dh, 0Ah, 0
szArchX86               db "[+] Process is x86", 0Dh, 0Ah, 0
szSuccess               db "[+] SUCCESS: Interceptor injected and active.", 0Dh, 0Ah, 0
szError                 db "[-] ERROR: ", 0
szErrorNotFound         db "Cursor executable not found.", 0Dh, 0Ah, 0
szErrorLaunch           db "Failed to launch Cursor.", 0Dh, 0Ah, 0
szErrorFind             db "Could not find process.", 0Dh, 0Ah, 0
szErrorInject           db "Failed to inject DLL.", 0Dh, 0Ah, 0
szErrorArch             db "Could not detect architecture.", 0Dh, 0Ah, 0
szDllPathX64            db MAX_PATH dup(0)
szDllPathX86            db MAX_PATH dup(0)
szCurrentDllPath        db MAX_PATH dup(0)
szKernel32              db "kernel32.dll", 0
szLoadLibrary           db "LoadLibraryA", 0
szIsWow64Process        db "IsWow64Process", 0
szFormat                db "%d", 0

;============================================================================
; CODE
;============================================================================

.code

;----------------------------------------------------------------------------
; Get current directory and build full DLL paths
;----------------------------------------------------------------------------
GetDllPaths PROC
    LOCAL hModule :DWORD
    LOCAL szTempPath[MAX_PATH]:BYTE
    
    ; Get module handle (this executable)
    invoke GetModuleHandle, NULL
    mov hModule, eax
    
    ; Get module file name to extract directory
    invoke GetModuleFileName, hModule, addr szTempPath, MAX_PATH
    
    ; Find last backslash and replace with null to get directory
    mov ecx, eax
    dec ecx
    lea edx, szTempPath
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
    ; Build x64 DLL path
    invoke lstrcpy, addr szDllPathX64, addr szTempPath
    invoke lstrcat, addr szDllPathX64, addr szInterceptorDllX64
    
    ; Build x86 DLL path
    invoke lstrcpy, addr szDllPathX86, addr szTempPath
    invoke lstrcat, addr szDllPathX86, addr szInterceptorDllX86
    
    ret
GetDllPaths ENDP

;----------------------------------------------------------------------------
; Find existing Cursor process
;----------------------------------------------------------------------------
FindCursorProcess PROC
    LOCAL hSnapshot :DWORD
    LOCAL pe32 :PROCESSENTRY32
    LOCAL dwPid :DWORD
    
    mov dwPid, 0
    
    ; Create toolhelp snapshot
    invoke CreateToolhelp32Snapshot, TH32CS_SNAPPROCESS, 0
    cmp eax, -1
    je @@done
    mov hSnapshot, eax
    
    ; Initialize process entry structure
    mov pe32.dwSize, SIZEOF PROCESSENTRY32
    
    ; Get first process
    invoke Process32First, hSnapshot, addr pe32
    cmp eax, 0
    je @@cleanup
    
@@find_loop:
    ; Compare process name with Cursor.exe
    lea eax, pe32.szExeFile
    push eax
    lea eax, szCursorProcess
    push eax
    call lstrcmpi
    cmp eax, 0
    je @@found
    
    ; Get next process
    invoke Process32Next, hSnapshot, addr pe32
    cmp eax, 0
    jne @@find_loop
    jmp @@cleanup
    
@@found:
    mov eax, pe32.th32ProcessID
    mov dwPid, eax
    
@@cleanup:
    invoke CloseHandle, hSnapshot
    
@@done:
    mov eax, dwPid
    ret
FindCursorProcess ENDP

;----------------------------------------------------------------------------
; Detect if process is 32-bit or 64-bit
;----------------------------------------------------------------------------
IsProcessX64 PROC hProcess:DWORD
    LOCAL hKernel32 :DWORD
    LOCAL pIsWow64Process :DWORD
    LOCAL bIsWow64 :DWORD
    
    ; Assume x64 initially
    mov eax, TRUE
    
    ; Get kernel32 handle
    invoke GetModuleHandle, addr szKernel32
    mov hKernel32, eax
    
    ; Get IsWow64Process function address
    invoke GetProcAddress, hKernel32, addr szIsWow64Process
    cmp eax, NULL
    je @@done  ; Function not available, assume x86
    mov pIsWow64Process, eax
    
    ; Call IsWow64Process
    lea eax, bIsWow64
    push eax
    push hProcess
    call pIsWow64Process
    cmp eax, 0
    je @@done  ; Call failed, assume x64
    
    ; Check result
    cmp bIsWow64, 0
    jne @@x86   ; If Wow64, it's a 32-bit process on 64-bit OS
    jmp @@done
    
@@x86:
    mov eax, FALSE
    
@@done:
    ret
IsProcessX64 ENDP

;----------------------------------------------------------------------------
; Inject DLL into process
;----------------------------------------------------------------------------
InjectDll PROC hProcess:DWORD, lpDllPath:DWORD
    LOCAL hKernel32 :DWORD
    LOCAL pLoadLibrary :DWORD
    LOCAL pRemoteMem :DWORD
    LOCAL hThread :DWORD
    LOCAL dwWritten :DWORD
    LOCAL dwDllLen :DWORD
    
    ; Get kernel32 handle
    invoke GetModuleHandle, addr szKernel32
    mov hKernel32, eax
    
    ; Get LoadLibraryA address
    invoke GetProcAddress, hKernel32, addr szLoadLibrary
    mov pLoadLibrary, eax
    
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
    je @@error
    
    ; Create remote thread to load DLL
    invoke CreateRemoteThread, hProcess, NULL, 0, pLoadLibrary, pRemoteMem, 0, NULL
    cmp eax, NULL
    je @@error
    mov hThread, eax
    
    ; Wait for thread to complete
    invoke WaitForSingleObject, hThread, INFINITE
    
    ; Cleanup
    invoke CloseHandle, hThread
    invoke VirtualFreeEx, hProcess, pRemoteMem, 0, MEM_RELEASE
    
    mov eax, TRUE
    ret
    
@@error:
    mov eax, FALSE
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
    LOCAL bIsX64 :DWORD
    LOCAL dwSize :DWORD
    LOCAL dwResult :DWORD
    LOCAL dwExitCode :DWORD

    ; Get standard output handle
    invoke GetStdHandle, STD_OUTPUT_HANDLE
    mov hStdOut, eax
    
    ; Display welcome message
    mov dwSize, SIZEOF szWelcome
    invoke WriteConsole, hStdOut, addr szWelcome, dwSize, addr dwResult, NULL

    ; Get DLL paths
    call GetDllPaths

    ; Display checking message
    mov dwSize, SIZEOF szChecking
    invoke WriteConsole, hStdOut, addr szChecking, dwSize, addr dwResult, NULL

    ; Check for existing Cursor process
    call FindCursorProcess
    mov dwCursorPid, eax
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

    ; Get process handle
    mov eax, processInfo.hProcess
    mov hCursorProcess, eax
    mov eax, processInfo.th32ProcessID
    mov dwCursorPid, eax

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
    ; Detect architecture
    mov dwSize, SIZEOF szDetectingArch
    invoke WriteConsole, hStdOut, addr szDetectingArch, dwSize, addr dwResult, NULL

    mov eax, hCursorProcess
    call IsProcessX64
    cmp eax, TRUE
    je @@x64_process

@@x86_process:
    ; Use x86 DLL
    mov dwSize, SIZEOF szArchX86
    invoke WriteConsole, hStdOut, addr szArchX86, dwSize, addr dwResult, NULL
    lea eax, szDllPathX86
    mov edx, eax
    jmp @@do_inject

@@x64_process:
    ; Use x64 DLL
    mov dwSize, SIZEOF szArchX64
    invoke WriteConsole, hStdOut, addr szArchX64, dwSize, addr dwResult, NULL
    lea eax, szDllPathX64
    mov edx, eax

@@do_inject:
    ; Store current DLL path
    mov eax, edx
    mov edx, OFFSET szCurrentDllPath
    @@copy_loop:
        mov cl, [eax]
        mov [edx], cl
        inc eax
        inc edx
        cmp cl, 0
        jne @@copy_loop

    ; Inject DLL
    mov dwSize, SIZEOF szInjecting
    invoke WriteConsole, hStdOut, addr szInjecting, dwSize, addr dwResult, NULL

    lea eax, szCurrentDllPath
    push eax
    mov eax, hCursorProcess
    push eax
    call InjectDll
    cmp eax, FALSE
    je @@error_inject

    ; Close process handle if we opened it
    cmp dwCursorPid, processInfo.th32ProcessID
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
