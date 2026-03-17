; MASM Basic Template
; Template for creating a new MASM assembly file

.686
.model flat, stdcall
option casemap:none

; Include files
include \\masm32\\include\\masm32rt.inc
include \\masm32\\include\\kernel32.inc
include \\masm32\\include\\user32.inc
includelib \\masm32\\lib\\masm32.lib
includelib \\masm32\\lib\\kernel32.lib
includelib \\masm32\\lib\\user32.lib

.data
    ; Data section - variables and constants
    szCaption db "MASM Application",0
    szMessage db "Hello, World!",0
    
    ; Example variables
    dwCounter dd 0
    szBuffer db 256 dup(0)
    
.data?
    ; Uninitialized data
    hInstance dd ?
    hMainWindow dd ?
    
.code

; Main entry point
start:
    ; Initialize application
    invoke GetModuleHandle, NULL
    mov hInstance, eax
    
    ; Create main window
    invoke WinMain, hInstance, NULL, NULL, SW_SHOW
    
    ; Exit application
    invoke ExitProcess, eax

; Window procedure
WndProc proc hWnd:HWND, uMsg:UINT, wParam:WPARAM, lParam:LPARAM
    .if uMsg == WM_DESTROY
        invoke PostQuitMessage, 0
        xor eax, eax
    .else
        invoke DefWindowProc, hWnd, uMsg, wParam, lParam
        ret
    .endif
    ret
WndProc endp

; Main window function
WinMain proc hInst:HINSTANCE, hPrevInst:HINSTANCE, CmdLine:LPSTR, CmdShow:DWORD
    LOCAL wc:WNDCLASSEX
    LOCAL msg:MSG
    
    ; Register window class
    mov wc.cbSize, sizeof WNDCLASSEX
    mov wc.style, CS_HREDRAW or CS_VREDRAW
    mov wc.lpfnWndProc, offset WndProc
    mov wc.cbClsExtra, 0
    mov wc.cbWndExtra, 0
    mov eax, hInst
    mov wc.hInstance, eax
    mov wc.hbrBackground, COLOR_WINDOW+1
    mov wc.lpszMenuName, NULL
    mov wc.lpszClassName, offset szClassName
    
    invoke LoadIcon, NULL, IDI_APPLICATION
    mov wc.hIcon, eax
    mov wc.hIconSm, eax
    
    invoke LoadCursor, NULL, IDC_ARROW
    mov wc.hCursor, eax
    
    invoke RegisterClassEx, addr wc
    
    ; Create window
    invoke CreateWindowEx, NULL, addr szClassName, addr szCaption,
                          WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                          800, 600, NULL, NULL, hInst, NULL
    mov hMainWindow, eax
    
    ; Show window
    invoke ShowWindow, hMainWindow, CmdShow
    invoke UpdateWindow, hMainWindow
    
    ; Message loop
    .while TRUE
        invoke GetMessage, addr msg, NULL, 0, 0
        .break .if (!eax)
        invoke TranslateMessage, addr msg
        invoke DispatchMessage, addr msg
    .endw
    
    mov eax, msg.wParam
    ret
WinMain endp

; Utility functions

; String length function
StrLen proc pString:PTR BYTE
    mov eax, pString
    xor ecx, ecx
    .while byte ptr [eax] != 0
        inc eax
        inc ecx
    .endw
    mov eax, ecx
    ret
StrLen endp

; Copy string function
StrCopy proc pDest:PTR BYTE, pSrc:PTR BYTE
    mov esi, pSrc
    mov edi, pDest
    .while byte ptr [esi] != 0
        movsb
    .endw
    mov byte ptr [edi], 0
    ret
StrCopy endp

; Compare strings function
StrCmp proc pStr1:PTR BYTE, pStr2:PTR BYTE
    mov esi, pStr1
    mov edi, pStr2
    .while byte ptr [esi] != 0 && byte ptr [edi] != 0
        mov al, [esi]
        cmp al, [edi]
        jne not_equal
        inc esi
        inc edi
    .endw
    mov al, [esi]
    cmp al, [edi]
    je equal
not_equal:
    mov eax, 1
    ret
equal:
    xor eax, eax
    ret
StrCmp endp

end start