; ============================================================================
; phase2_test_main.asm
; Standalone Test Executable for Phase 2 Integration
;
; Purpose: Test all Phase 2 components in a minimal Win32 application
; - Menu System
; - Theme System
; - File Browser
; - Phase 2 Integration Layer
;
; Build:
;   ml64 /c /Zi phase2_test_main.asm
;   link /SUBSYSTEM:WINDOWS /OUT:phase2_test.exe ^
;        phase2_test_main.obj ^
;        phase2_integration.obj ^
;        menu_system.obj ^
;        masm_theme_system_complete.obj ^
;        masm_file_browser_complete.obj ^
;        kernel32.lib user32.lib gdi32.lib comctl32.lib ^
;        shell32.lib shlwapi.lib ole32.lib
; ============================================================================

; x64 MASM - includes first, then sections

include windows.inc
include kernel32.inc
include user32.inc

INCLUDELIB kernel32.lib
INCLUDELIB user32.lib
INCLUDELIB comctl32.lib

; ============================================================================
; EXTERNAL DECLARATIONS - Phase 2 Integration
; ============================================================================
EXTERN Phase2_Initialize: PROC
EXTERN Phase2_Cleanup: PROC
EXTERN Phase2_HandleCommand: PROC
EXTERN Phase2_HandleSize: PROC
EXTERN Phase2_HandlePaint: PROC

; ============================================================================
; CONSTANTS
; ============================================================================
WINDOW_CLASS_NAME  EQU "Phase2TestWindow"
WINDOW_TITLE       EQU "Phase 2 Integration Test - Menu + Theme + FileBrowser"

; ============================================================================
; DATA
; ============================================================================

.data

    szClassName     BYTE "Phase2TestWindow", 0
    szWindowTitle   BYTE "Phase 2 Integration Test - Menu + Theme + FileBrowser", 0
    hInstance       HINSTANCE ?
    hMainWindow     HWND ?

.code

; ============================================================================
; Window Procedure
; x64 calling convention: rcx=hWnd, edx=uMsg, r8=wParam, r9=lParam
; ============================================================================

WndProc PROC
    
    cmp edx, WM_CREATE
    je WndProc_OnCreate
    
    cmp edx, WM_COMMAND
    je WndProc_OnCommand
    
    cmp edx, WM_SIZE
    je WndProc_OnSize
    
    cmp edx, WM_PAINT
    je WndProc_OnPaint
    
    cmp edx, WM_DESTROY
    je WndProc_OnDestroy
    
    ; Default processing
    jmp DefWindowProcA
    
WndProc_OnCreate:
    ; Initialize Phase 2 systems
    ; rcx = hWnd (already set)
    call Phase2_Initialize
    test rax, rax
    jz WndProc_InitFailed
    
    xor eax, eax  ; Return 0 (success)
    ret
    
WndProc_InitFailed:
    ; Initialization failed, destroy window
    mov rcx, [rsp+8]  ; hWnd from stack (first param)
    call DestroyWindow
    mov eax, -1
    ret
    
WndProc_OnCommand:
    ; wParam contains command ID in low word
    movzx ecx, WORD PTR [rsp+16]  ; LOWORD(wParam)
    call Phase2_HandleCommand
    
    test rax, rax
    jnz WndProc_CommandHandled
    
    ; Not handled by Phase 2, use default
    jmp DefWindowProcA
    
WndProc_CommandHandled:
    xor eax, eax
    ret
    
WndProc_OnSize:
    ; lParam = MAKELPARAM(width, height)
    mov r9, [rsp+24]  ; lParam from stack
    movzx ecx, WORD PTR r9   ; width (LOWORD)
    shr r9, 16
    movzx edx, WORD PTR r9   ; height (HIWORD)
    
    call Phase2_HandleSize
    
    xor eax, eax
    ret
    
WndProc_OnPaint:
    ; rcx = hWnd (already set)
    call Phase2_HandlePaint
    
    xor eax, eax
    ret
    
WndProc_OnDestroy:
    ; Cleanup Phase 2 systems
    call Phase2_Cleanup
    
    ; Post quit message
    xor ecx, ecx
    call PostQuitMessage
    
    xor eax, eax
    ret
    
WndProc ENDP

; ============================================================================
; WinMain - Application Entry Point
; x64: Stack space allocated manually for WNDCLASSEXA and MSG
; ============================================================================

WinMain PROC
    ; Allocate stack space: WNDCLASSEXA (80 bytes) + MSG (48 bytes) + alignment
    sub rsp, 168
    
    ; Get instance handle
    xor ecx, ecx
    call GetModuleHandleA
    mov hInstance, rax
    
    ; Initialize common controls
    call InitCommonControls
    
    ; Register window class (wc at rsp+32)
    lea rbx, [rsp+32]  ; rbx = &wc
    mov DWORD PTR [rbx], 80  ; cbSize = SIZEOF WNDCLASSEXA
    mov DWORD PTR [rbx+4], CS_HREDRAW or CS_VREDRAW  ; style
    lea rax, WndProc
    mov QWORD PTR [rbx+8], rax  ; lpfnWndProc
    mov DWORD PTR [rbx+16], 0   ; cbClsExtra
    mov DWORD PTR [rbx+20], 0   ; cbWndExtra
    mov rax, hInstance
    mov QWORD PTR [rbx+24], rax  ; hInstance
    
    ; Load icon (use default)
    xor ecx, ecx
    mov edx, 32512  ; IDI_APPLICATION
    call LoadIconA
    mov QWORD PTR [rbx+32], rax  ; hIcon
    mov QWORD PTR [rbx+40], rax  ; hIconSm
    
    ; Load cursor (arrow)
    xor ecx, ecx
    mov edx, 32512  ; IDC_ARROW
    call LoadCursorA
    mov QWORD PTR [rbx+48], rax  ; hCursor
    
    ; Background brush (will be overridden by theme)
    mov QWORD PTR [rbx+56], 0  ; hbrBackground = NULL
    mov QWORD PTR [rbx+64], 0  ; lpszMenuName = NULL
    lea rax, szClassName
    mov QWORD PTR [rbx+72], rax  ; lpszClassName
    
    ; Register
    mov rcx, rbx  ; &wc
    call RegisterClassExA
    test rax, rax
    jz WinMain_Exit
    
    ; Create window
    xor ecx, ecx               ; dwExStyle
    lea rdx, szClassName       ; lpClassName
    lea r8, szWindowTitle      ; lpWindowName
    mov r9d, WS_OVERLAPPEDWINDOW or WS_VISIBLE  ; dwStyle
    
    ; Stack parameters for CreateWindowExA
    mov DWORD PTR [rsp+32], CW_USEDEFAULT  ; x
    mov DWORD PTR [rsp+40], CW_USEDEFAULT  ; y
    mov DWORD PTR [rsp+48], 1200           ; width
    mov DWORD PTR [rsp+56], 800            ; height
    mov QWORD PTR [rsp+64], 0              ; hWndParent
    mov QWORD PTR [rsp+72], 0              ; hMenu
    mov rax, hInstance
    mov QWORD PTR [rsp+80], rax            ; hInstance
    mov QWORD PTR [rsp+88], 0              ; lpParam
    
    call CreateWindowExA
    
    test rax, rax
    jz WinMain_Exit
    
    mov hMainWindow, rax
    
    ; Show window
    mov rcx, rax
    mov edx, SW_SHOW
    call ShowWindow
    
    ; Update window
    mov rcx, hMainWindow
    call UpdateWindow
    
    ; Message loop (msg at rsp+112)
    lea rdi, [rsp+112]  ; rdi = &msg
WinMain_MessageLoop:
    mov rcx, rdi      ; &msg
    xor edx, edx      ; hWnd = NULL
    xor r8d, r8d      ; wMsgFilterMin
    xor r9d, r9d      ; wMsgFilterMax
    call GetMessageA
    
    test eax, eax
    jz WinMain_Exit  ; WM_QUIT received
    
    mov rcx, rdi
    call TranslateMessage
    
    mov rcx, rdi
    call DispatchMessageA
    
    jmp WinMain_MessageLoop
    
WinMain_Exit:
    mov eax, DWORD PTR [rsp+120]  ; msg.wParam (exit code)
    add rsp, 168
    ret
    
WinMain ENDP

; ============================================================================
; Entry Point
; ============================================================================

mainCRTStartup PROC
    call WinMain
    
    ; Exit process
    mov ecx, eax
    call ExitProcess
    
mainCRTStartup ENDP

END mainCRTStartup
