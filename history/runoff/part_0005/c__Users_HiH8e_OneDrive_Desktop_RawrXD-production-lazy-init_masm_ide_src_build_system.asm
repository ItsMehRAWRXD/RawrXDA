; ============================================================================
; RawrXD IDE - Phase 2: Build System
; MASM compilation integration, error parsing, build progress tracking
; ============================================================================

.686
.model flat, stdcall
option casemap:none

include \masm32\include\windows.inc
include \masm32\include\kernel32.inc
include \masm32\include\user32.inc

includelib \masm32\lib\kernel32.lib
includelib \masm32\lib\user32.lib

; ============================================================================
; CONSTANTS
; ============================================================================

MAX_BUILD_OUTPUT    equ 16384
MAX_ERROR_MESSAGES  equ 100

BUILD_STATE_IDLE    equ 0
BUILD_STATE_RUNNING equ 1
BUILD_STATE_ERROR   equ 2
BUILD_STATE_SUCCESS equ 3

BUILD_TYPE_DEBUG    equ 1
BUILD_TYPE_RELEASE  equ 2

; ============================================================================
; STRUCTURES
; ============================================================================

BUILD_ERROR struct
    fileName        db 64 dup(?)
    lineNumber      dd ?
    errorCode       dd ?
    errorMessage    db 256 dup(?)
    isWarning       db ?
BUILD_ERROR ends

BUILD_PROJECT struct
    projectName     db 64 dup(?)
    projectPath     db MAX_PATH dup(?)
    mainFile        db MAX_PATH dup(?)
    outputFile      db MAX_PATH dup(?)
    buildType       dd ?
    buildState      dd ?
    errorCount      dd ?
    warningCount    dd ?
    startTime       dd ?
    endTime         dd ?
BUILD_PROJECT ends

; ============================================================================
; DATA SECTION
; ============================================================================

.data
    public currentProject
    public buildErrors
    public buildOutput
    public outputPosition
    
    currentProject      BUILD_PROJECT <>
    buildErrors         BUILD_ERROR MAX_ERROR_MESSAGES dup(<>)
    buildOutput         db MAX_BUILD_OUTPUT dup(0)
    outputPosition      dd 0
    
    hBuildThread        dd 0
    buildThreadID       dd 0
    
    ; MASM paths
    masmPath            db "C:\MASM32\BIN\ML.EXE", 0
    linkPath            db "C:\MASM32\BIN\LINK.EXE", 0
    
    ; Build flags
    debugFlags          db "/c /coff /Zi /Fo", 0
    releaseFlags        db "/c /coff /Fo", 0
    linkFlags           db "/SUBSYSTEM:WINDOWS /LIBPATH:C:\MASM32\LIB", 0
    
    szDefaultProject    db "Default Project", 0
    szBuildInProgress   db "Build already in progress!", 0
    szBuildSystem       db "Build System", 0
    szBuildStarted      db "Build started...", 0
    szBuildThreadFailed db "Failed to create build thread!", 0
    szAssemblingFiles   db "Step 1: Assembling source files...", 0
    szLinkingFiles      db "Step 2: Linking object files...", 0
    szBuildSuccess      db "Build completed successfully!", 0
    szBuildFailed       db "Build failed with errors!", 0
    szBuildTime         db "Build time: %d ms", 0
    szPipeFailed        db "Failed to create pipe!", 0
    szCommandFailed     db "Failed to execute command!", 0
    szNewLine           db 13, 10, 0
    szError             db "error", 0
    szWarning           db "warning", 0
    szBuilding          db "Building...", 0
    szBuildSuccessStatus db "Build completed successfully", 0
    szBuildFailedStatus db "Build failed with errors", 0
    szReady             db "Ready", 0
    szBuildStopped      db "Build stopped by user", 0
    szMasmCommand       db "%s %s ""%s""", 0
    szLinkCommand       db "%s %s ""%s.obj"" /OUT:""%s.exe""", 0
    szOutputFormat      db "%s", 13, 10, 0
    szErrorFormat       db "%s (%d): %s", 13, 10, 0

; ============================================================================
; EXTERNAL DECLARATIONS
; ============================================================================

extrn g_hMainWindow:DWORD
extrn g_hStatusBar:DWORD
extrn currentFilePath:BYTE

; ============================================================================
; CODE SECTION
; ============================================================================

.code

; Internal Prototypes
ClearBuildOutput PROTO
SetupDefaultProject PROTO
UpdateBuildUI PROTO :DWORD
BuildThreadProc PROTO :DWORD
ParseBuildOutput PROTO :DWORD
AddBuildMessage PROTO :DWORD

public InitializeBuildSystem
public BuildProject
public StopBuild
public GetBuildErrors
public CleanupBuildSystem
public AddBuildMessage

; ============================================================================
; InitializeBuildSystem - Setup build system
; ============================================================================
InitializeBuildSystem proc
    ; Initialize build output
    call ClearBuildOutput
    
    ; Setup default project settings
    call SetupDefaultProject
    
    ret
InitializeBuildSystem endp

; ============================================================================
; SetupDefaultProject - Setup default project settings
; ============================================================================
SetupDefaultProject proc
    ; Set default project name
    invoke lstrcpy, offset currentProject.projectName, offset szDefaultProject
    
    ; Set default build type
    mov currentProject.buildType, BUILD_TYPE_DEBUG
    mov currentProject.buildState, BUILD_STATE_IDLE
    mov currentProject.errorCount, 0
    mov currentProject.warningCount, 0
    
    ret
SetupDefaultProject endp

; ============================================================================
; BuildProject - Main build function
; ============================================================================
BuildProject proc lpProjectPath:DWORD, buildType:DWORD
    LOCAL hThread:DWORD
    LOCAL threadID:DWORD
    
    ; Check if build is already running
    mov eax, currentProject.buildState
    cmp eax, BUILD_STATE_RUNNING
    jne @NotRunning
    
    ; Build already running
    invoke MessageBox, g_hMainWindow, offset szBuildInProgress, 
                       offset szBuildSystem, MB_OK or MB_ICONWARNING
    ret
    
@NotRunning:
    ; Update project info
    invoke lstrcpy, offset currentProject.projectPath, lpProjectPath
    mov eax, buildType
    mov currentProject.buildType, eax
    mov currentProject.buildState, BUILD_STATE_RUNNING
    mov currentProject.errorCount, 0
    mov currentProject.warningCount, 0
    
    ; Get start time
    invoke GetTickCount
    mov currentProject.startTime, eax
    
    ; Clear previous build output
    call ClearBuildOutput
    
    ; Add build started message
    call AddBuildMessage, offset szBuildStarted
    
    ; Create build thread
    invoke CreateThread, NULL, 0, offset BuildThreadProc, 
                         buildType, 0, addr threadID
    mov hThread, eax
    
    .if hThread != NULL
        ; Store thread handle
        mov eax, hThread
        mov hBuildThread, eax
        mov eax, threadID
        mov buildThreadID, eax
        
        ; Update UI
        call UpdateBuildUI, 1
    .else
        call AddBuildMessage, offset szBuildThreadFailed
        mov currentProject.buildState, BUILD_STATE_ERROR
    .endif
    
    ret
BuildProject endp

; ============================================================================
; BuildThreadProc - Build execution thread
; ============================================================================
BuildThreadProc proc lpParam:DWORD
    LOCAL buildType:DWORD
    LOCAL commandLine:db 1024 dup(0)
    LOCAL workingDir:db MAX_PATH dup(0)
    
    mov eax, lpParam
    mov buildType, eax
    
    ; Step 1: Assemble main file
    call AddBuildMessage, offset szAssemblingFiles
    
    ; Build assembly command
    invoke wsprintf, offset commandLine, offset szMasmCommand, 
                     offset masmPath, offset debugFlags, offset currentFilePath
    
    ; Execute assembly command
    call ExecuteBuildCommand, offset commandLine, offset workingDir
    
    ; Check if assembly succeeded
    mov eax, currentProject.errorCount
    cmp eax, 0
    jg @BuildFailed
    
    ; Step 2: Link object files
    call AddBuildMessage, offset szLinkingFiles
    
    ; Build link command
    invoke wsprintf, offset commandLine, offset szLinkCommand, 
                     offset linkPath, offset linkFlags, offset currentFilePath, 
                     offset currentFilePath
    
    ; Execute link command
    call ExecuteBuildCommand, offset commandLine, offset workingDir
    
    ; Check if linking succeeded
    mov eax, currentProject.errorCount
    cmp eax, 0
    jg @BuildFailed
    
    ; Build succeeded
    call AddBuildMessage, offset szBuildSuccess
    mov currentProject.buildState, BUILD_STATE_SUCCESS
    
    ; Get end time and calculate duration
    invoke GetTickCount
    mov currentProject.endTime, eax
    sub eax, currentProject.startTime
    invoke wsprintf, offset commandLine, offset szBuildTime, eax
    call AddBuildMessage, offset commandLine
    
    jmp @BuildComplete
    
@BuildFailed:
    call AddBuildMessage, offset szBuildFailed
    mov currentProject.buildState, BUILD_STATE_ERROR
    
@BuildComplete:
    ; Update UI
    call UpdateBuildUI, 0
    
    ; Exit thread
    invoke ExitThread, 0
    ret
BuildThreadProc endp

; ============================================================================
; ExecuteBuildCommand - Execute individual build command
; ============================================================================
ExecuteBuildCommand proc lpCommandLine:DWORD, lpWorkingDir:DWORD
    LOCAL si:STARTUPINFO
    LOCAL pi:PROCESS_INFORMATION
    LOCAL hReadPipe:DWORD
    LOCAL hWritePipe:DWORD
    LOCAL sa:SECURITY_ATTRIBUTES
    LOCAL dwRead:DWORD
    LOCAL buffer:db 4096 dup(0)
    LOCAL exitCode:DWORD
    
    ; Setup security attributes for pipe
    mov sa.nLength, sizeof SECURITY_ATTRIBUTES
    mov sa.bInheritHandle, 1
    mov sa.lpSecurityDescriptor, NULL
    
    ; Create pipe for capturing output
    invoke CreatePipe, addr hReadPipe, addr hWritePipe, addr sa, 0
    .if eax == 0
        call AddBuildMessage, offset szPipeFailed
        ret
    .endif
    
    ; Setup startup info
    invoke RtlZeroMemory, addr si, sizeof STARTUPINFO
    mov si.cb, sizeof STARTUPINFO
    mov si.dwFlags, STARTF_USESTDHANDLES or STARTF_USESHOWWINDOW
    mov si.wShowWindow, SW_HIDE
    mov eax, hWritePipe
    mov si.hStdOutput, eax
    mov si.hStdError, eax
    
    ; Create process
    invoke CreateProcess, NULL, lpCommandLine, NULL, NULL, 1, 
                         CREATE_NO_WINDOW, NULL, lpWorkingDir, addr si, addr pi
    
    .if eax != 0
        ; Close write end of pipe
        invoke CloseHandle, hWritePipe
        
        ; Read output from pipe
        @@ReadLoop:
            invoke ReadFile, hReadPipe, addr buffer, 4095, addr dwRead, NULL
            .if eax == 0 || dwRead == 0
                jmp @ReadDone
            .endif
            
            ; Null terminate
            mov buffer[dwRead], 0
            
            ; Process output
            call ParseBuildOutput, addr buffer
            
            ; Add to build output
            call AddBuildMessage, addr buffer
            jmp @@ReadLoop
        
        @ReadDone:
        ; Wait for process to complete
        invoke WaitForSingleObject, pi.hProcess, INFINITE
        
        ; Get exit code
        invoke GetExitCodeProcess, pi.hProcess, addr exitCode
        
        ; Check if command succeeded
        .if exitCode != 0
            inc currentProject.errorCount
        .endif
        
        ; Close process handles
        invoke CloseHandle, pi.hProcess
        invoke CloseHandle, pi.hThread
    .else
        call AddBuildMessage, offset szCommandFailed
        inc currentProject.errorCount
        
        ; Close pipe handles
        invoke CloseHandle, hWritePipe
        invoke CloseHandle, hReadPipe
    .endif
    
    ret
ExecuteBuildCommand endp

; ============================================================================
; ParseBuildOutput - Parse build output for errors/warnings
; ============================================================================
ParseBuildOutput proc lpOutput:DWORD
    LOCAL pLine:DWORD
    LOCAL lineBuffer:db 512 dup(0)
    
    mov pLine, lpOutput
    
    ; Process each line
    @@ProcessLoop:
        ; Find end of line
        invoke lstrlen, pLine
        .if eax == 0
            jmp @ProcessDone
        .endif
        
        ; Copy line to buffer
        invoke lstrcpyn, offset lineBuffer, pLine, 511
        
        ; Check for error patterns
        call CheckForErrors, offset lineBuffer
        
        ; Find next line
        invoke strstr, pLine, offset szNewLine
        .if eax != NULL
            mov pLine, eax
            inc pLine  ; Skip newline
        .else
            jmp @ProcessDone
        .endif
        jmp @@ProcessLoop
    
    @ProcessDone:
    ret
ParseBuildOutput endp

; ============================================================================
; CheckForErrors - Check line for error/warning patterns
; ============================================================================
CheckForErrors proc lpLine:DWORD
    ; Check for MASM error patterns
    invoke stristr, lpLine, offset szError
    .if eax != NULL
        ; Found error
        call AddBuildError, lpLine, 0
        ret
    .endif
    
    ; Check for MASM warning patterns
    invoke stristr, lpLine, offset szWarning
    .if eax != NULL
        ; Found warning
        call AddBuildError, lpLine, 1
        ret
    .endif
    
    ret
CheckForErrors endp

; ============================================================================
; AddBuildError - Add error to error list
; ============================================================================
AddBuildError proc lpErrorLine:DWORD, isWarning:DWORD
    LOCAL errorCount:DWORD
    
    ; Get current error count
    mov eax, currentProject.errorCount
    add eax, currentProject.warningCount
    mov errorCount, eax
    
    ; Check if we have room for more errors
    cmp errorCount, MAX_ERROR_MESSAGES
    jge @NoRoom
    
    ; Add error to list
    mov eax, errorCount
    imul eax, sizeof BUILD_ERROR
    lea edi, buildErrors[eax]
    
    ; Parse error line
    call ParseErrorLine, edi, lpErrorLine, isWarning
    
    ; Increment appropriate counter
    .if isWarning == 0
        inc currentProject.errorCount
    .else
        inc currentProject.warningCount
    .endif
    
    @NoRoom:
    ret
AddBuildError endp

; ============================================================================
; ParseErrorLine - Parse error line for details
; ============================================================================
ParseErrorLine proc lpError:DWORD, lpLine:DWORD, isWarning:DWORD
    ; For now, just copy the line
    invoke lstrcpy, addr [lpError].BUILD_ERROR.errorMessage, lpLine
    mov eax, isWarning
    mov [lpError].BUILD_ERROR.isWarning, al
    
    ret
ParseErrorLine endp

; ============================================================================
; AddBuildMessage - Add message to build output
; ============================================================================
AddBuildMessage proc lpMessage:DWORD
    LOCAL currentPos:DWORD
    LOCAL messageLen:DWORD
    
    ; Get current position in output buffer
    mov eax, outputPosition
    mov currentPos, eax
    
    ; Check if we have room
    cmp currentPos, MAX_BUILD_OUTPUT - 256
    jge @NoRoom
    
    ; Add timestamp and message
    invoke wsprintf, addr buildOutput[currentPos], offset szOutputFormat, 
                     lpMessage
    
    ; Update position
    invoke lstrlen, addr buildOutput[currentPos]
    add outputPosition, eax
    
    @NoRoom:
    ret
AddBuildMessage endp

; ============================================================================
; ClearBuildOutput - Clear build output buffer
; ============================================================================
ClearBuildOutput proc
    invoke RtlZeroMemory, offset buildOutput, MAX_BUILD_OUTPUT
    mov outputPosition, 0
    ret
ClearBuildOutput endp

; ============================================================================
; UpdateBuildUI - Update build-related UI elements
; ============================================================================
UpdateBuildUI proc isBuilding:DWORD
    .if isBuilding == 1
        ; Building in progress
        invoke SetWindowText, g_hStatusBar, offset szBuilding
    .else
        ; Build completed
        mov eax, currentProject.buildState
        .if eax == BUILD_STATE_SUCCESS
            invoke SetWindowText, g_hStatusBar, offset szBuildSuccessStatus
        .elseif eax == BUILD_STATE_ERROR
            invoke SetWindowText, g_hStatusBar, offset szBuildFailedStatus
        .else
            invoke SetWindowText, g_hStatusBar, offset szReady
        .endif
    .endif
    
    ret
UpdateBuildUI endp

; ============================================================================
; StopBuild - Stop current build
; ============================================================================
StopBuild proc
    .if hBuildThread != 0
        ; Terminate build thread
        invoke TerminateThread, hBuildThread, 1
        invoke CloseHandle, hBuildThread
        mov hBuildThread, 0
        
        call AddBuildMessage, offset szBuildStopped
        mov currentProject.buildState, BUILD_STATE_IDLE
        
        call UpdateBuildUI, 0
    .endif
    
    ret
StopBuild endp

; ============================================================================
; GetBuildErrors - Retrieve build errors
; ============================================================================
GetBuildErrors proc lpErrorBuffer:DWORD, maxErrors:DWORD
    LOCAL i:DWORD
    LOCAL errorCount:DWORD
    
    mov errorCount, 0
    mov i, 0
    
    ; Copy errors to buffer
    @@CopyLoop:
        cmp i, currentProject.errorCount
        jge @CopyDone
        cmp i, maxErrors
        jge @CopyDone
        
        ; Format error message
        mov eax, i
        imul eax, sizeof BUILD_ERROR
        lea esi, buildErrors[eax]
        
        invoke wsprintf, lpErrorBuffer, offset szErrorFormat, 
                         addr [esi].BUILD_ERROR.fileName, 
                         [esi].BUILD_ERROR.lineNumber, 
                         addr [esi].BUILD_ERROR.errorMessage
        
        ; Move buffer pointer
        invoke lstrlen, lpErrorBuffer
        add lpErrorBuffer, eax
        
        inc errorCount
        inc i
        jmp @@CopyLoop
    
    @CopyDone:
    mov eax, errorCount
    ret
GetBuildErrors endp

; ============================================================================
; CleanupBuildSystem - Release build system resources
; ============================================================================
CleanupBuildSystem proc
    ; Stop any running build
    call StopBuild
    
    ; Clear build output
    call ClearBuildOutput
    
    ret
CleanupBuildSystem endp

; ============================================================================
; CODE SECTION CONTINUED
; ============================================================================

.code

end