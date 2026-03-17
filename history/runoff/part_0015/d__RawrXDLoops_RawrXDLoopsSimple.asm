.686
.model flat, stdcall
option casemap:none

include windows.inc
include kernel32.inc
include user32.inc
include gdi32.inc
include winmm.inc

includelib kernel32.lib
includelib user32.lib
includelib gdi32.lib
includelib winmm.lib

.data
szClassName db "RawrXDLoopsClass",0
szAppName db "RawrXDLoops DAW",0
szWindowTitle db "RawrXDLoops - Simple MASM DAW",0

.data?
hInstance HINSTANCE ?
hWnd HWND ?

.code

; Main entry point
WinMain proc hInst:HINSTANCE, hPrevInst:HINSTANCE, CmdLine:LPSTR, CmdShow:DWORD
    LOCAL wc:WNDCLASSEX
    LOCAL msg:MSG
    
    mov hInstance, hInst
    
    ; Register window class
    mov wc.cbSize, sizeof WNDCLASSEX
    mov wc.style, CS_HREDRAW or CS_VREDRAW
    mov wc.lpfnWndProc, offset WndProc
    mov wc.cbClsExtra, 0
    mov wc.cbWndExtra, 0
    mov eax, hInstance
    mov wc.hInstance, eax
    mov wc.hbrBackground, COLOR_WINDOW+1
    mov wc.lpszMenuName, 0
    mov wc.lpszClassName, offset szClassName
    
    invoke LoadIcon, NULL, IDI_APPLICATION
    mov wc.hIcon, eax
    mov wc.hIconSm, eax
    
    invoke LoadCursor, NULL, IDC_ARROW
    mov wc.hCursor, eax
    
    invoke RegisterClassEx, addr wc
    
    ; Create window
    invoke CreateWindowEx, NULL, addr szClassName, addr szWindowTitle,
                          WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                          800, 600, NULL, NULL, hInstance, NULL
    mov hWnd, eax
    
    ; Show window
    invoke ShowWindow, hWnd, SW_SHOW
    invoke UpdateWindow, hWnd
    
    ; Message loop
    .while TRUE
        invoke GetMessage, addr msg, NULL, 0, 0
        .break .if eax == 0
        
        invoke TranslateMessage, addr msg
        invoke DispatchMessage, addr msg
    .endw
    
    mov eax, msg.wParam
    ret
WinMain endp

; Window procedure
WndProc proc hWnd:HWND, uMsg:UINT, wParam:WPARAM, lParam:LPARAM
    LOCAL ps:PAINTSTRUCT
    LOCAL hdc:HDC
    
    .if uMsg == WM_DESTROY
        invoke PostQuitMessage, 0
        xor eax, eax
        ret
    .elseif uMsg == WM_PAINT
        invoke BeginPaint, hWnd, addr ps
        mov hdc, eax
        
        ; Draw simple interface
        invoke TextOut, hdc, 10, 10, addr szWindowTitle, sizeof szWindowTitle
        invoke TextOut, hdc, 10, 40, "Channel Rack - Press 1-5 to switch panes", 40
        invoke TextOut, hdc, 10, 70, "Space: Play/Pause, R: Hot-reload, L: Load AI", 45
        
        invoke EndPaint, hWnd, addr ps
        xor eax, eax
        ret
    .elseif uMsg == WM_KEYDOWN
        .if wParam == VK_SPACE
            ; Toggle play/pause
            invoke InvalidateRect, hWnd, NULL, TRUE
        .elseif wParam == 'R'
            ; Hot-reload
            invoke MessageBox, hWnd, "Hot-reload requested", "RawrXDLoops", MB_OK
        .elseif wParam == 'L'
            ; Load AI model
            invoke MessageBox, hWnd, "AI model load requested", "RawrXDLoops", MB_OK
        .endif
    .endif
    
    invoke DefWindowProc, hWnd, uMsg, wParam, lParam
    ret
WndProc endp

end WinMain