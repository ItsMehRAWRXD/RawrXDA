; RawrXD Agentic IDE - Engine Implementation (Pure MASM)
; Fixed version with proper parameter handling

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

; Execution modes (equivalent to C++ enum)
EXECUTION_MODE_MANUAL_APPROVAL equ 0
EXECUTION_MODE_AUTO_EXECUTE    equ 1
EXECUTION_MODE_DRY_RUN         equ 2

; ============================================================================
; Engine Data
; ============================================================================

.data
    public g_hInstance
    public g_hMainWindow
    public g_hMainFont
    public hInstance

include constants.inc
include structures.inc
include macros.inc

; WISH_RESULT structure (equivalent to C++ struct)
WISH_RESULT struct
    bSuccess           dd ?
    szFinalOutput      db MAX_BUFFER_SIZE dup(?)
    dwExecutionTime    dd ?
    dwActionsExecuted  dd ?
WISH_RESULT ends

; Engine state
.data
    g_hInstance        dd 0
    g_hMainWindow      dd 0
    g_hMainFont        dd 0
    hInstance          dd 0
    g_bInitialized     dd 0
    
    szEngineInitMsg     db "Engine initialized successfully", 13, 10, 0
    szWindowFailMsg     db "Failed to create main window", 13, 10, 0
    szWishExecMsg       db "Executing wish: ", 0
    szWishSuccessMsg    db "Wish execution successful: ", 0
    szWishFailMsg       db "Wish execution failed", 13, 10, 0
    szMainWindowTitle   db "RawrXD MASM IDE", 0
    szNewLine           db 13, 10, 0

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

IDEAgentBridge_ExecuteWish proc pszWish:DWORD, pResult:DWORD
    mov eax, TRUE
    ret
IDEAgentBridge_ExecuteWish endp

; ============================================================================
; Engine_Initialize - Initialize engine (converted from C++)
; ============================================================================

public Engine_Initialize
Engine_Initialize proc hInstance_:DWORD
    ; Store instance handle
    mov eax, hInstance_
    mov g_hInstance, eax
    mov hInstance, eax
    
    ; Load configuration (equivalent to loadConfiguration())
    call LoadConfiguration
    test eax, eax
    jz @InitFailed
    
    ; Initialize agentic system (equivalent to initializeAgenticSystem())
    call InitializeAgenticSystem
    test eax, eax
    jz @InitFailed
    
    ; Create main window (equivalent to MainWindow mainWindow(hInstance_))
    push 800
    push 1200
    push offset szMainWindowTitle
    push g_hInstance
    call MainWindow_Create
    test eax, eax
    jz @WindowFailed
    
    ; Initialize UI layout components
    call InitializeUILayout
    ; Continue even if UI initialization partially fails
    
    ; Output success message (equivalent to std::cout)
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
    ; Output error message (equivalent to std::cerr)
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