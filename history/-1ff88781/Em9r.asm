; editor.asm - Custom Non-Monaco Editor in Pure Assembly
; Author: RawrXD Team
; Date: December 3, 2025

.686
.MODEL FLAT, STDCALL
.STACK 4096

INCLUDE windows.inc
INCLUDE kernel32.inc
INCLUDE user32.inc
INCLUDE masm32.inc

INCLUDELIB kernel32.lib
INCLUDELIB user32.lib
INCLUDELIB masm32.lib

.DATA
    appName DB "CustomEditor", 0
    className DB "EditorClass", 0
    windowTitle DB "Pure Assembly Editor", 0
    buffer DB 1024 DUP(0)

.CODE

start:
    ; Initialize the editor
    invoke GetModuleHandle, NULL
    mov hInstance, eax
    invoke RegisterClassEx, ADDR wndClass
    invoke CreateWindowEx, 0, ADDR className, ADDR windowTitle, WS_OVERLAPPEDWINDOW,
           CW_USEDEFAULT, CW_USEDEFAULT, 800, 600, NULL, NULL, hInstance, NULL
    mov hWnd, eax

    ; Show the window
    invoke ShowWindow, hWnd, SW_SHOWNORMAL
    invoke UpdateWindow, hWnd

    ; Message loop
    .WHILE TRUE
        invoke GetMessage, ADDR msg, NULL, 0, 0
        .BREAK .IF (!eax)
        invoke TranslateMessage, ADDR msg
        invoke DispatchMessage, ADDR msg
    .ENDW

    invoke ExitProcess, 0

WndProc PROC hWnd:DWORD, uMsg:DWORD, wParam:DWORD, lParam:DWORD
    .IF uMsg == WM_DESTROY
        invoke PostQuitMessage, 0
    .ELSE
        invoke DefWindowProc, hWnd, uMsg, wParam, lParam
        ret
    .ENDIF
    xor eax, eax
    ret
WndProc ENDP

END start