; ============================================================================
; RawrXD IDE - Phase 2: Build System (Simplified)
; Basic MASM compilation integration
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
; DATA SECTION
; ============================================================================

.data
    public buildOutput
    public outputPosition
    
    buildOutput         db 8192 dup(0)
    outputPosition      dd 0
    
    ; MASM paths
    masmPath            db "C:\MASM32\BIN\ML.EXE", 0
    linkPath            db "C:\MASM32\BIN\LINK.EXE", 0

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

public InitializeBuildSystem
public BuildProject
public CleanupBuildSystem
public AddBuildMessage

; ============================================================================
; InitializeBuildSystem - Setup build system
; ============================================================================
InitializeBuildSystem proc
    ; Clear build output
    call ClearBuildOutput
    ret
InitializeBuildSystem endp

; ============================================================================
; BuildProject - Main build function
; ============================================================================
BuildProject proc
    LOCAL commandLine:db 1024 dup(0)
    
    ; Check if we have a file to build
    invoke lstrlen, offset currentFilePath
    .if eax == 0
        invoke MessageBox, g_hMainWindow, offset szNoFile, 
                           offset szBuildSystem, MB_OK or MB_ICONWARNING
        ret
    .endif
    
    ; Clear previous build output
    call ClearBuildOutput
    
    ; Add build started message
    call AddBuildMessage, offset szBuildStarted
    
    ; Build assembly command
    invoke wsprintf, offset commandLine, offset szMasmCommand, 
                     offset masmPath, offset currentFilePath
    
    ; Execute assembly command
    call ExecuteBuildCommand, offset commandLine
    
    ; Check if assembly succeeded
    .if eax == 0
        ; Build link command
        invoke wsprintf, offset commandLine, offset szLinkCommand, 
                         offset linkPath, offset currentFilePath
        
        ; Execute link command
        call ExecuteBuildCommand, offset commandLine
    .endif
    
    .if eax == 0
        call AddBuildMessage, offset szBuildSuccess
    .else
        call AddBuildMessage, offset szBuildFailed
    .endif
    
    ret
BuildProject endp

; ============================================================================
; ExecuteBuildCommand - Execute build command
; ============================================================================
ExecuteBuildCommand proc lpCommandLine:DWORD
    LOCAL si:STARTUPINFO
    LOCAL pi:PROCESS_INFORMATION
    LOCAL exitCode:DWORD
    
    ; Setup startup info
    invoke RtlZeroMemory, addr si, sizeof STARTUPINFO
    mov si.cb, sizeof STARTUPINFO
    mov si.dwFlags, STARTF_USESHOWWINDOW
    mov si.wShowWindow, SW_HIDE
    
    ; Create process
    invoke CreateProcess, NULL, lpCommandLine, NULL, NULL, FALSE, 
                         CREATE_NO_WINDOW, NULL, NULL, addr si, addr pi
    
    .if eax != 0
        ; Wait for process to complete
        invoke WaitForSingleObject, pi.hProcess, INFINITE
        
        ; Get exit code
        invoke GetExitCodeProcess, pi.hProcess, addr exitCode
        
        ; Close process handles
        invoke CloseHandle, pi.hProcess
        invoke CloseHandle, pi.hThread
        
        mov eax, exitCode
    .else
        call AddBuildMessage, offset szCommandFailed
        mov eax, 1
    .endif
    
    ret
ExecuteBuildCommand endp

; ============================================================================
; AddBuildMessage - Add message to build output
; ============================================================================
AddBuildMessage proc lpMessage:DWORD
    LOCAL currentPos:DWORD
    
    ; Get current position in output buffer
    mov eax, outputPosition
    mov currentPos, eax
    
    ; Check if we have room
    cmp currentPos, 8192 - 256
    jge @NoRoom
    
    ; Add message
    invoke wsprintf, addr buildOutput[currentPos], offset szOutputFormat, lpMessage
    
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
    invoke RtlZeroMemory, offset buildOutput, 8192
    mov outputPosition, 0
    ret
ClearBuildOutput endp

; ============================================================================
; CleanupBuildSystem - Release build system resources
; ============================================================================
CleanupBuildSystem proc
    ; Clear build output
    call ClearBuildOutput
    ret
CleanupBuildSystem endp

; ============================================================================
; DATA STRINGS
; ============================================================================

szNoFile            db "No file selected to build!", 0
szBuildSystem       db "Build System", 0
szBuildStarted      db "Build started...", 0
szBuildSuccess      db "Build completed successfully!", 0
szBuildFailed       db "Build failed!", 0
szCommandFailed     db "Failed to execute command!", 0
szMasmCommand       db "%s /c /coff /Fo output.obj %s", 0
szLinkCommand       db "%s /SUBSYSTEM:WINDOWS output.obj", 0
szOutputFormat      db "%s\r\n", 0

end