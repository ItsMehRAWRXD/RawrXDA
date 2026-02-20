;============================================================================
; OS Explorer Interceptor CLI - Simplified Version for MASM32
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

;============================================================================
; DATA
;============================================================================

.data

szCursorPath            db "C:\Users\HiH8e\AppData\Local\Programs\cursor\Cursor.exe", 0
szInterceptorDll        db "os_explorer_interceptor.dll", 0
szWelcome               db "OS Explorer Interceptor CLI v", CLI_VERSION, 0Dh, 0Ah
                        db "=======================================", 0Dh, 0Ah, 0Dh, 0Ah
                        db "Launching Cursor with interceptor...", 0Dh, 0Ah, 0
szSuccess               db "Cursor launched successfully with interceptor injected.", 0Dh, 0Ah, 0
szError                 db "Error: ", 0
szErrorLaunch           db "Failed to launch Cursor.", 0Dh, 0Ah, 0
szErrorInject           db "Failed to inject interceptor DLL.", 0Dh, 0Ah, 0
szErrorFind             db "Could not find Cursor process.", 0Dh, 0Ah, 0
szErrorLoad             db "Could not load interceptor DLL.", 0Dh, 0Ah, 0
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
    LOCAL dwLen :DWORD
    
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
    cmp byte ptr [edx], '\'
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
    LOCAL si :STARTUPINFO
    LOCAL pi :PROCESS_INFORMATION
    LOCAL dwResult :DWORD
    
    ; Display welcome message
    invoke GetStdHandle, STD_OUTPUT_HANDLE
    invoke WriteConsole, eax, addr szWelcome, sizeof szWelcome, addr dwResult, NULL
    
    ; Get full DLL path
    call GetDllPath
    
    ; Launch Cursor
    invoke RtlZeroMemory, addr si, sizeof si
    invoke RtlZeroMemory, addr pi, sizeof pi
    mov si.cb, sizeof si
    
    invoke CreateProcess, addr szCursorPath, NULL, NULL, NULL, FALSE,
                         CREATE_SUSPENDED, NULL, NULL, addr si, addr pi
    cmp eax, 0
    je @@error_launch
    
    ; Inject DLL
    invoke InjectDll, pi.hProcess, addr szDllPath
    cmp eax, FALSE
    je @@error_inject
    
    ; Resume main thread
    invoke ResumeThread, pi.hThread
    
    ; Display success
    invoke GetStdHandle, STD_OUTPUT_HANDLE
    invoke WriteConsole, eax, addr szSuccess, sizeof szSuccess, addr dwResult, NULL
    
    ; Cleanup
    invoke CloseHandle, pi.hProcess
    invoke CloseHandle, pi.hThread
    
    jmp @@exit
    
@@error_launch:
    invoke GetStdHandle, STD_OUTPUT_HANDLE
    invoke WriteConsole, eax, addr szError, sizeof szError, addr dwResult, NULL
    invoke WriteConsole, eax, addr szErrorLaunch, sizeof szErrorLaunch, addr dwResult, NULL
    jmp @@exit
    
@@error_inject:
    invoke GetStdHandle, STD_OUTPUT_HANDLE
    invoke WriteConsole, eax, addr szError, sizeof szError, addr dwResult, NULL
    invoke WriteConsole, eax, addr szErrorInject, sizeof szErrorInject, addr dwResult, NULL
    invoke TerminateProcess, pi.hProcess, 1
    invoke CloseHandle, pi.hProcess
    invoke CloseHandle, pi.hThread
    
@@exit:
    invoke ExitProcess, 0
    ret
main ENDP

END main
