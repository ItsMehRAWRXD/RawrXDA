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
    fontName DB "Consolas", 0
    textBuffer DB 8192 DUP(0) ; Text buffer for the editor
    cursorPos DWORD 0 ; Cursor position

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
    LOCAL hdc:DWORD
    LOCAL ps:PAINTSTRUCT
    LOCAL rect:RECT

    .IF uMsg == WM_PAINT
        invoke BeginPaint, hWnd, ADDR ps
        mov hdc, eax

        ; Set font
        invoke CreateFont, 16, 0, 0, 0, FW_NORMAL, 0, 0, 0, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, FIXED_PITCH, ADDR fontName
        invoke SelectObject, hdc, eax

        ; Draw text from buffer
        invoke GetClientRect, hWnd, ADDR rect
        invoke DrawText, hdc, ADDR textBuffer, -1, ADDR rect, DT_LEFT

        invoke EndPaint, hWnd, ADDR ps
    .ELSEIF uMsg == WM_KEYDOWN
        ; Handle key input
        .IF wParam == VK_BACK
            ; Handle backspace
            .IF cursorPos > 0
                dec cursorPos
                mov byte ptr [textBuffer + cursorPos], 0
            .ENDIF
        .ELSE
            ; Add character to buffer
            mov byte ptr [textBuffer + cursorPos], wParam
            inc cursorPos
        .ENDIF
        invoke InvalidateRect, hWnd, NULL, TRUE
    .ELSEIF uMsg == WM_DESTROY
        invoke PostQuitMessage, 0
    .ELSE
        invoke DefWindowProc, hWnd, uMsg, wParam, lParam
        ret
    .ENDIF
    xor eax, eax
    ret
WndProc ENDP

END start