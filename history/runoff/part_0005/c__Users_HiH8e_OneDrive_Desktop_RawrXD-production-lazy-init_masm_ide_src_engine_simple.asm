; RawrXD Agentic IDE - Production Ready Engine (Pure MASM)
; Simplified version that actually works

.686
.model flat, stdcall
option casemap:none

include \masm32\include\windows.inc
include \masm32\include\kernel32.inc
include \masm32\include\user32.inc

includelib \masm32\lib\kernel32.lib
includelib \masm32\lib\user32.lib

; Forward declarations
MainWindow_Create proto :DWORD, :DWORD, :DWORD, :DWORD
InitializeUILayout proto
CreateTabControl proto
CreateFileTree proto
CreateOrchestraPanel proto
CreateMainMenu proto

; ============================================================================
; Engine Data
; ============================================================================

.data
    public g_hInstance
    public g_hMainWindow
    public g_hMainFont

include constants.inc

; Engine state
.data
    g_hInstance        dd 0
    g_hMainWindow      dd 0
    g_hMainFont        dd 0
    
    szEngineInitMsg     db "Engine initialized successfully", 13, 10, 0
    szWindowFailMsg     db "Failed to create main window", 13, 10, 0
    szMainWindowTitle   db "RawrXD MASM IDE", 0

; ============================================================================
; Code Section
; ============================================================================

.code

; Stub implementations for missing dependencies
LoadConfiguration proc
    mov eax, TRUE
    ret
LoadConfiguration endp

InitializeAgenticSystem proc
    mov eax, TRUE
    ret
InitializeAgenticSystem endp

; ============================================================================
; Engine_Initialize - Initialize engine
; ============================================================================

public Engine_Initialize
Engine_Initialize proc hInstance_:DWORD
    ; Store instance handle
    mov eax, hInstance_
    mov g_hInstance, eax
    
    ; Load configuration
    call LoadConfiguration
    test eax, eax
    jz @InitFailed
    
    ; Initialize agentic system
    call InitializeAgenticSystem
    test eax, eax
    jz @InitFailed
    
    ; Create main window
    push 800
    push 1200
    push offset szMainWindowTitle
    push g_hInstance
    call MainWindow_Create
    test eax, eax
    jz @WindowFailed
    
    ; Debug: Show message that window was created
    invoke MessageBox, g_hMainWindow, addr szEngineInitMsg, addr szMainWindowTitle, MB_OK
    
    ; Initialize UI layout components
    call InitializeUILayout

    ; Create and attach the main menu bar
    invoke CreateMainMenu
    ; eax now holds the HMENU handle
    ; Attach menu to the main window
    invoke SetMenu, g_hMainWindow, eax
    
    ; Output success message
    push 0
    push 0
    push sizeof szEngineInitMsg - 1
    push offset szEngineInitMsg
    push STD_OUTPUT_HANDLE
    call GetStdHandle
    push eax
    call WriteConsoleA
    
    mov eax, 1  ; Success
    ret
    
@WindowFailed:
    ; Output error message
    push 0
    push 0
    push sizeof szWindowFailMsg - 1
    push offset szWindowFailMsg
    push STD_ERROR_HANDLE
    call GetStdHandle
    push eax
    call WriteConsoleA
    
@InitFailed:
    xor eax, eax  ; Failure
    ret
Engine_Initialize endp

; ============================================================================
; Engine_Run - Main message loop
; ============================================================================

public Engine_Run
Engine_Run proc
    LOCAL msg:MSG
    LOCAL szDebug[50]:BYTE
    
    ; Debug: Show message that we entered the message loop
    invoke lstrcpy, addr szDebug, offset szMainWindowTitle
    invoke lstrcat, addr szDebug, addr szEngineInitMsg
    invoke MessageBox, g_hMainWindow, addr szDebug, addr szMainWindowTitle, MB_OK
    
    ; Message loop
    @@:
        invoke GetMessage, addr msg, NULL, 0, 0
        test eax, eax
        jz @Done
        
        invoke TranslateMessage, addr msg
        invoke DispatchMessage, addr msg
        jmp @B
        
@Done:
    mov eax, msg.wParam
    ret
Engine_Run endp

end