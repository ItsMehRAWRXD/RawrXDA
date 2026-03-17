; RawrXD Agentic IDE - Debug Launch Version
; Adds MessageBox to verify window creation

.686
.model flat, stdcall
option casemap:none

include \masm32\include\windows.inc
include \masm32\include\kernel32.inc
include \masm32\include\user32.inc

includelib \masm32\lib\kernel32.lib
includelib \masm32\lib\user32.lib

.data
    szStartMsg db "RawrXD Win32 MASM IDE - Starting...", 13, 10, 0
    szFailMsg  db "Failed to initialize engine", 13, 10, 0
    szDebugMsg db "Window created successfully!", 0
    szDebugTitle db "Debug", 0

; External declarations for engine functions and globals
Engine_Initialize proto :DWORD
Engine_Run proto
extrn g_hInstance:DWORD
extrn g_hMainWindow:DWORD
extrn g_hMainFont:DWORD

; ============================================================================
; WinMain - Entry point (converted from C++)
; ============================================================================

.code
WinMain proc hInstance:DWORD, hPrevInstance:DWORD, lpCmdLine:DWORD, nCmdShow:DWORD
    LOCAL bInitialized:DWORD
    
    ; Output startup message (equivalent to std::cout)
    push 0                     ; lpReserved
    push 0                     ; lpNumberOfCharsWritten
    push sizeof szStartMsg - 1 ; nNumberOfCharsToWrite
    push offset szStartMsg     ; lpBuffer
    push STD_OUTPUT_HANDLE
    call GetStdHandle
    push eax                   ; hConsoleOutput
    call WriteConsoleA
    
    ; Initialize the engine (equivalent to engine.initialize(hInstance))
    invoke Engine_Initialize, hInstance
    mov bInitialized, eax
    
    .if bInitialized == 0
        ; Output error message (equivalent to std::cerr)
        push 0                     ; lpReserved
        push 0                     ; lpNumberOfCharsWritten
        push sizeof szFailMsg - 1  ; nNumberOfCharsToWrite
        push offset szFailMsg      ; lpBuffer
        push STD_ERROR_HANDLE
        call GetStdHandle
        push eax                   ; hConsoleOutput
        call WriteConsoleA
        
        mov eax, 1  ; Return error code
        ret
    .endif
    
    ; Debug: Show message that window was created
    invoke MessageBox, g_hMainWindow, addr szDebugMsg, addr szDebugTitle, MB_OK
    
    ; Run the main message loop (equivalent to engine.run())
    call Engine_Run
    mov eax, eax  ; Return the result
    
    ret
WinMain endp

end WinMain