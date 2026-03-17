;======================================================================
; RawrXD IDE - Build System Module
; Compilation, linking, error reporting, progress tracking
;======================================================================
INCLUDE rawrxd_includes.inc

.DATA
; Build configuration
g_buildConfig[256]      DB 256 DUP(0)
g_buildTarget[256]      DB 256 DUP(0)
g_buildMode[32]         DB "Debug", 0
g_compilerPath[260]     DB 260 DUP(0)
g_linkerPath[260]       DB 260 DUP(0)

; Build state
g_isBuilding            DQ 0
g_buildStartTime        DQ 0
g_buildEndTime          DQ 0
g_errorCount            DQ 0
g_warningCount          DQ 0
g_lastBuildStatus       DQ 0

; Build process handles
g_hBuildProcess         DQ ?
g_hBuildThread          DQ ?
g_hBuildStdout          DQ ?
g_hBuildStderr          DQ ?

; Build output buffer
g_buildOutput[8192]     DB 8192 DUP(0)
g_buildOutputSize       DQ 0

; Compiler error pattern
szErrorPattern          DB "error", 0
szWarningPattern        DB "warning", 0
szNotePattern           DB "note", 0

.CODE

;----------------------------------------------------------------------
; RawrXD_Build_Initialize - Initialize build system
;----------------------------------------------------------------------
RawrXD_Build_Initialize PROC pszCompiler:QWORD, pszLinker:QWORD
    ; Store compiler and linker paths
    INVOKE lstrcpyA, ADDR g_compilerPath, pszCompiler
    INVOKE lstrcpyA, ADDR g_linkerPath, pszLinker
    
    xor eax, eax
    ret
    
RawrXD_Build_Initialize ENDP

;----------------------------------------------------------------------
; RawrXD_Build_SetMode - Set build mode (Debug/Release)
;----------------------------------------------------------------------
RawrXD_Build_SetMode PROC pszMode:QWORD
    INVOKE lstrcpyA, ADDR g_buildMode, pszMode
    ret
RawrXD_Build_SetMode ENDP

;----------------------------------------------------------------------
; RawrXD_Build_Compile - Compile current project
;----------------------------------------------------------------------
RawrXD_Build_Compile PROC pszProjectPath:QWORD
    LOCAL szCommand[512]:BYTE
    LOCAL startInfo:STARTUPINFOA
    LOCAL procInfo:PROCESS_INFORMATION
    LOCAL lpEnvironment:QWORD
    
    ; Check if already building
    cmp g_isBuilding, 1
    je @@already_building
    
    ; Mark as building
    mov g_isBuilding, 1
    
    ; Update status
    INVOKE RawrXD_StatusBar_SetBuilding
    INVOKE RawrXD_Output_Build, OFFSET szBuildStarted
    
    ; Get current time
    INVOKE GetTickCount
    mov g_buildStartTime, rax
    
    ; Build command line
    INVOKE lstrcpyA, ADDR szCommand, ADDR g_compilerPath
    INVOKE lstrcatA, ADDR szCommand, " -c "
    INVOKE lstrcatA, ADDR szCommand, pszProjectPath
    
    ; Initialize startup info
    INVOKE RtlZeroMemory, ADDR startInfo, SIZEOF STARTUPINFOA
    mov startInfo.cb, SIZEOF STARTUPINFOA
    mov startInfo.dwFlags, STARTF_USESTDHANDLES
    
    ; Create pipes for output capture
    INVOKE RawrXD_Build_CreatePipes
    
    mov startInfo.hStdOutput, g_hBuildStdout
    mov startInfo.hStdError, g_hBuildStderr
    
    ; Create compiler process
    INVOKE CreateProcessA,
        ADDR g_compilerPath,
        ADDR szCommand,
        NULL,
        NULL,
        TRUE,  ; Inherit handles
        CREATE_NO_WINDOW,
        NULL,
        pszProjectPath,
        ADDR startInfo,
        ADDR procInfo
    
    test eax, eax
    jz @@compile_error
    
    ; Store process handle
    mov g_hBuildProcess, procInfo.hProcess
    
    ; Create monitoring thread
    INVOKE CreateThread,
        NULL,
        0,
        RawrXD_Build_MonitorProcess,
        procInfo.hProcess,
        0,
        NULL
    
    mov g_hBuildThread, rax
    
    xor eax, eax
    ret
    
@@already_building:
    INVOKE RawrXD_Output_Warning, OFFSET szBuildInProgress
    mov eax, -1
    ret
    
@@compile_error:
    mov g_isBuilding, 0
    INVOKE RawrXD_Output_Error, OFFSET szCompileFailure
    mov eax, -1
    ret
    
RawrXD_Build_Compile ENDP

;----------------------------------------------------------------------
; RawrXD_Build_CompileAndRun - Compile and execute
;----------------------------------------------------------------------
RawrXD_Build_CompileAndRun PROC pszProjectPath:QWORD
    ; Compile first
    INVOKE RawrXD_Build_Compile, pszProjectPath
    test eax, eax
    jnz @@fail
    
    ; Wait for compilation to finish
    INVOKE WaitForSingleObject, g_hBuildProcess, INFINITE
    
    ; Run if successful
    cmp g_lastBuildStatus, 0
    jne @@fail
    
    ; Execute output executable
    INVOKE RawrXD_Build_ExecuteTarget, pszProjectPath
    
    xor eax, eax
    ret
    
@@fail:
    mov eax, -1
    ret
    
RawrXD_Build_CompileAndRun ENDP

;----------------------------------------------------------------------
; RawrXD_Build_CreatePipes - Create pipes for process output
;----------------------------------------------------------------------
RawrXD_Build_CreatePipes PROC
    LOCAL hReadPipe:QWORD
    LOCAL hWritePipe:QWORD
    LOCAL saAttr:SECURITY_ATTRIBUTES
    
    ; Create security attributes for pipes
    mov saAttr.nLength, SIZEOF SECURITY_ATTRIBUTES
    mov saAttr.bInheritHandle, TRUE
    mov saAttr.lpSecurityDescriptor, NULL
    
    ; Create stdout pipe
    INVOKE CreatePipe, ADDR hReadPipe, ADDR hWritePipe, ADDR saAttr, 0
    test eax, eax
    jz @@fail
    
    mov g_hBuildStdout, hWritePipe
    
    ; Create stderr pipe (same as stdout)
    INVOKE CreatePipe, ADDR hReadPipe, ADDR hWritePipe, ADDR saAttr, 0
    test eax, eax
    jz @@fail
    
    mov g_hBuildStderr, hWritePipe
    
    xor eax, eax
    ret
    
@@fail:
    mov eax, -1
    ret
    
RawrXD_Build_CreatePipes ENDP

;----------------------------------------------------------------------
; RawrXD_Build_MonitorProcess - Monitor build process (thread)
;----------------------------------------------------------------------
RawrXD_Build_MonitorProcess PROC hProcess:QWORD
    LOCAL exitCode:QWORD
    LOCAL output[1024]:BYTE
    LOCAL bytesRead:QWORD
    
    ; Wait for process to complete
    INVOKE WaitForSingleObject, hProcess, INFINITE
    
    ; Get exit code
    INVOKE GetExitCodeProcess, hProcess, ADDR exitCode
    mov g_lastBuildStatus, exitCode
    
    ; Read remaining output
    INVOKE RawrXD_Build_ReadOutput
    
    ; Parse output for errors/warnings
    INVOKE RawrXD_Build_ParseOutput, ADDR g_buildOutput
    
    ; Update UI
    cmp exitCode, 0
    je @@success
    
    INVOKE RawrXD_Output_Error, OFFSET szBuildFailed
    INVOKE RawrXD_StatusBar_SetError, OFFSET szBuildFailed
    jmp @@done
    
@@success:
    INVOKE RawrXD_Output_Build, OFFSET szBuildSuccess
    INVOKE RawrXD_StatusBar_SetReady
    
@@done:
    ; Mark not building
    mov g_isBuilding, 0
    
    ; Close process handle
    INVOKE CloseHandle, hProcess
    
    xor eax, eax
    ret
    
RawrXD_Build_MonitorProcess ENDP

;----------------------------------------------------------------------
; RawrXD_Build_ReadOutput - Read process output from pipes
;----------------------------------------------------------------------
RawrXD_Build_ReadOutput PROC
    LOCAL buffer[1024]:BYTE
    LOCAL bytesRead:QWORD
    
    ; Read from stdout
    INVOKE ReadFile, g_hBuildStdout, ADDR buffer, 1024, ADDR bytesRead, NULL
    test eax, eax
    jz @@done
    
    ; Append to output buffer
    mov rax, g_buildOutputSize
    mov rcx, ADDR buffer
    mov rdx, ADDR g_buildOutput
    add rdx, rax
    mov r8, bytesRead
    
    rep movsb
    add g_buildOutputSize, bytesRead
    
@@done:
    ret
    
RawrXD_Build_ReadOutput ENDP

;----------------------------------------------------------------------
; RawrXD_Build_ParseOutput - Parse compiler output for errors
;----------------------------------------------------------------------
RawrXD_Build_ParseOutput PROC pszOutput:QWORD
    LOCAL pPos:QWORD
    LOCAL szLine[512]:BYTE
    LOCAL lineIdx:QWORD
    
    mov pPos, pszOutput
    mov g_errorCount, 0
    mov g_warningCount, 0
    
@@scan_loop:
    ; Extract line
    INVOKE RawrXD_Build_ExtractLine, pPos, ADDR szLine
    
    ; Check for error pattern
    INVOKE strstrA, ADDR szLine, OFFSET szErrorPattern
    test eax, eax
    jz @@check_warn
    
    ; Found error
    inc g_errorCount
    INVOKE RawrXD_Output_Error, ADDR szLine
    jmp @@next_line
    
@@check_warn:
    INVOKE strstrA, ADDR szLine, OFFSET szWarningPattern
    test eax, eax
    jz @@next_line
    
    ; Found warning
    inc g_warningCount
    INVOKE RawrXD_Output_Warning, ADDR szLine
    
@@next_line:
    ; Move to next line
    mov pPos, rax
    test eax, eax
    jnz @@scan_loop
    
    ret
    
RawrXD_Build_ParseOutput ENDP

;----------------------------------------------------------------------
; RawrXD_Build_ExtractLine - Extract next line from output
;----------------------------------------------------------------------
RawrXD_Build_ExtractLine PROC pszInput:QWORD, pszLine:QWORD
    LOCAL idx:QWORD
    
    mov idx, 0
    
@@copy_loop:
    mov al, [pszInput + idx]
    
    ; Check for line ending
    cmp al, 13  ; CR
    je @@end_line
    cmp al, 10  ; LF
    je @@end_line
    cmp al, 0   ; Null
    je @@end_line
    
    ; Copy character
    mov [pszLine + idx], al
    inc idx
    
    ; Check buffer overflow
    cmp idx, 512
    jge @@end_line
    
    jmp @@copy_loop
    
@@end_line:
    ; Null terminate line
    mov byte [pszLine + idx], 0
    
    ; Return next position (skip line ending)
    mov rax, pszInput
    add rax, idx
    
    ; Skip CR/LF
    mov al, [rax]
    cmp al, 13
    jne @@check_lf
    inc rax
@@check_lf:
    cmp byte [rax], 10
    jne @@ret
    inc rax
    
@@ret:
    ret
    
RawrXD_Build_ExtractLine ENDP

;----------------------------------------------------------------------
; RawrXD_Build_ExecuteTarget - Run compiled executable
;----------------------------------------------------------------------
RawrXD_Build_ExecuteTarget PROC pszProjectPath:QWORD
    LOCAL szExePath[260]:BYTE
    LOCAL startInfo:STARTUPINFOA
    LOCAL procInfo:PROCESS_INFORMATION
    
    ; Build output executable path
    INVOKE lstrcpyA, ADDR szExePath, pszProjectPath
    INVOKE lstrcatA, ADDR szExePath, "\build\output.exe"
    
    ; Check if file exists
    INVOKE GetFileAttributesA, ADDR szExePath
    cmp eax, INVALID_FILE_ATTRIBUTES
    je @@not_found
    
    ; Initialize startup info
    INVOKE RtlZeroMemory, ADDR startInfo, SIZEOF STARTUPINFOA
    mov startInfo.cb, SIZEOF STARTUPINFOA
    
    ; Create executable process
    INVOKE CreateProcessA,
        ADDR szExePath,
        NULL,
        NULL,
        NULL,
        FALSE,
        0,
        NULL,
        pszProjectPath,
        ADDR startInfo,
        ADDR procInfo
    
    test eax, eax
    jz @@exec_error
    
    ; Close handles
    INVOKE CloseHandle, procInfo.hProcess
    INVOKE CloseHandle, procInfo.hThread
    
    INVOKE RawrXD_Output_Build, OFFSET szExecuting
    
    xor eax, eax
    ret
    
@@not_found:
    INVOKE RawrXD_Output_Error, OFFSET szExeNotFound
    mov eax, -1
    ret
    
@@exec_error:
    INVOKE RawrXD_Output_Error, OFFSET szExecFailed
    mov eax, -1
    ret
    
RawrXD_Build_ExecuteTarget ENDP

;----------------------------------------------------------------------
; RawrXD_Build_Stop - Stop current build
;----------------------------------------------------------------------
RawrXD_Build_Stop PROC
    cmp g_isBuilding, 0
    je @@not_building
    
    ; Terminate process
    INVOKE TerminateProcess, g_hBuildProcess, 1
    
    ; Wait for thread
    INVOKE WaitForSingleObject, g_hBuildThread, 1000
    
    ; Close handles
    INVOKE CloseHandle, g_hBuildProcess
    INVOKE CloseHandle, g_hBuildThread
    
    mov g_isBuilding, 0
    
    INVOKE RawrXD_Output_Warning, OFFSET szBuildStopped
    INVOKE RawrXD_StatusBar_SetReady
    
    xor eax, eax
    ret
    
@@not_building:
    mov eax, -1
    ret
    
RawrXD_Build_Stop ENDP

;----------------------------------------------------------------------
; RawrXD_Build_GetStats - Get build statistics
;----------------------------------------------------------------------
RawrXD_Build_GetStats PROC pErrors:QWORD, pWarnings:QWORD
    mov rax, pErrors
    mov rcx, g_errorCount
    mov [rax], rcx
    
    mov rax, pWarnings
    mov rcx, g_warningCount
    mov [rax], rcx
    
    ret
    
RawrXD_Build_GetStats ENDP

;----------------------------------------------------------------------
; RawrXD_Build_IsBuilding - Check if build in progress
;----------------------------------------------------------------------
RawrXD_Build_IsBuilding PROC
    mov eax, g_isBuilding
    ret
RawrXD_Build_IsBuilding ENDP

; String literals
szBuildStarted          DB "Build started...", 0
szBuildSuccess          DB "Build successful!", 0
szBuildFailed           DB "Build failed!", 0
szBuildStopped          DB "Build stopped", 0
szBuildInProgress       DB "Build already in progress", 0
szCompileFailure        DB "Failed to start compiler", 0
szExecuting             DB "Executing...", 0
szExeNotFound           DB "Executable not found", 0
szExecFailed            DB "Failed to execute", 0

END
