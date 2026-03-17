;============================================================================
; OS Explorer Interceptor CLI - Fixed for MASM32
; Minimal command-line interface for launching Cursor with interceptor
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

includelib \masm32\lib\kernel32.lib
includelib \masm32\lib\user32.lib
includelib \masm32\lib\advapi32.lib

;============================================================================
; CONSTANTS
;============================================================================

CLI_VERSION             equ "1.0.0"
MAX_PATH                equ 260
MEM_COMMIT              equ 00001000h
PAGE_READWRITE          equ 00000004h
INFINITE                equ 0FFFFFFFFh

;============================================================================
; DATA
;============================================================================

.data

szCursorPath            db "C:\Users\HiH8e\AppData\Local\Programs\cursor\Cursor.exe", 0
szInterceptorDll        db "os_explorer_interceptor.dll", 0
szWelcome               db "OS Explorer Interceptor CLI v", CLI_VERSION, 0Dh, 0Ah
                        db "=======================================", 0Dh, 0Ah, 0Dh, 0Ah
                        db "Checking Cursor path...", 0Dh, 0Ah, 0
szCursorNotFound        db "Cursor not found at: ", 0Dh, 0Ah, 0
szLaunching             db "Launching Cursor with interceptor...", 0Dh, 0Ah, 0
szSuccess               db "Cursor launched successfully with interceptor injected.", 0Dh, 0Ah, 0
szError                 db "Error: ", 0
szErrorLaunch           db "Failed to launch Cursor (CreateProcess failed).", 0Dh, 0Ah, 0
szErrorInject           db "Failed to inject interceptor DLL.", 0Dh, 0Ah, 0
szExitCode              db "Exit code: ", 0Dh, 0Ah, 0
szDllPath               db MAX_PATH dup(0)
szKernel32              db "kernel32.dll", 0
szLoadLibrary           db "LoadLibraryA", 0

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
; Main entry point
;----------------------------------------------------------------------------
main PROC
    LOCAL hStdOut :DWORD
    LOCAL startupInfo :STARTUPINFO
    LOCAL processInfo :PROCESS_INFORMATION
    LOCAL dwSize :DWORD
    LOCAL dwResult :DWORD
    LOCAL dwExitCode :DWORD

    ; Display welcome message
    invoke GetStdHandle, STD_OUTPUT_HANDLE
    mov hStdOut, eax
    mov dwSize, SIZEOF szWelcome
    lea eax, dwResult
    push eax
    push NULL
    push dwSize
    lea eax, szWelcome
    push eax
    push hStdOut
    call WriteConsole
    add esp, 20

    ; Check if Cursor exists
    invoke GetFileAttributes, OFFSET szCursorPath
    cmp eax, -1
    jne @@cursor_found
    
    ; Cursor not found
    mov dwSize, SIZEOF szCursorNotFound
    lea eax, dwResult
    push eax
    push NULL
    push dwSize
    lea eax, szCursorNotFound
    push eax
    push hStdOut
    call WriteConsole
    add esp, 20
    jmp @@exit
    
@@cursor_found:
    ; Display launching message
    mov dwSize, SIZEOF szLaunching
    lea eax, dwResult
    push eax
    push NULL
    push dwSize
    lea eax, szLaunching
    push eax
    push hStdOut
    call WriteConsole
    add esp, 20

    ; Get full DLL path
    call GetDllPath

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

    ; Inject DLL
    lea eax, szDllPath
    push eax
    mov eax, processInfo.hProcess
    push eax
    call InjectDll
    cmp eax, FALSE
    je @@error_inject

    ; Resume main thread
    mov eax, processInfo.hThread
    push eax
    call ResumeThread

    ; Display success
    mov dwSize, SIZEOF szSuccess
    lea eax, dwResult
    push eax
    push NULL
    push dwSize
    lea eax, szSuccess
    push eax
    push hStdOut
    call WriteConsole
    add esp, 20

    ; Cleanup
    mov eax, processInfo.hProcess
    push eax
    call CloseHandle
    mov eax, processInfo.hThread
    push eax
    call CloseHandle

    jmp @@exit

@@error_launch:
    ; Display error message
    mov dwSize, SIZEOF szError
    lea eax, dwResult
    push eax
    push NULL
    push dwSize
    lea eax, szError
    push eax
    push hStdOut
    call WriteConsole
    add esp, 20
    mov dwSize, SIZEOF szErrorLaunch
    lea eax, dwResult
    push eax
    push NULL
    push dwSize
    lea eax, szErrorLaunch
    push eax
    push hStdOut
    call WriteConsole
    add esp, 20
    
    ; Get last error
    invoke GetLastError
    mov dwExitCode, eax
    jmp @@exit

@@error_inject:
    ; Display error message
    mov dwSize, SIZEOF szError
    lea eax, dwResult
    push eax
    push NULL
    push dwSize
    lea eax, szError
    push eax
    push hStdOut
    call WriteConsole
    add esp, 20
    mov dwSize, SIZEOF szErrorInject
    lea eax, dwResult
    push eax
    push NULL
    push dwSize
    lea eax, szErrorInject
    push eax
    push hStdOut
    call WriteConsole
    add esp, 20
    
    ; Terminate process
    mov eax, processInfo.hProcess
    push 1
    push eax
    call TerminateProcess
    mov eax, processInfo.hProcess
    push eax
    call CloseHandle
    mov eax, processInfo.hThread
    push eax
    call CloseHandle
    
    ; Get last error
    invoke GetLastError
    mov dwExitCode, eax
    jmp @@exit

@@exit:
    ; Display exit code
    mov dwSize, SIZEOF szExitCode
    lea eax, dwResult
    push eax
    push NULL
    push dwSize
    lea eax, szExitCode
    push eax
    push hStdOut
    call WriteConsole
    add esp, 20
    
    ; Exit with code
    push dwExitCode
    call ExitProcess
    ret
main ENDP

END main
