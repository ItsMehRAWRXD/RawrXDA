.386
.model flat, stdcall
option casemap:none

include windows.inc
include kernel32.inc
include user32.inc
include gdi32.inc

includelib kernel32.lib
includelib user32.lib
includelib gdi32.lib

.data
szClassName db "RawrXDLoopsClass",0
szAppName db "RawrXDLoops DAW",0
szWindowTitle db "RawrXDLoops - Simple MASM DAW",0
szChannelRack db "Channel Rack - Press 1-5 to switch panes",0
szControls db "Space: Play/Pause, R: Hot-reload, L: Load AI",0

.data?
hInstance dd ?
hWnd dd ?

.code

start:
    invoke GetModuleHandle, NULL
    mov hInstance, eax
    
    invoke GetCommandLine
    
    invoke WinMain, hInstance, NULL, eax, SW_SHOWDEFAULT
    
    invoke ExitProcess, eax

; Main entry point
WinMain proc hInst:DWORD, hPrevInst:DWORD, CmdLine:DWORD, CmdShow:DWORD
    LOCAL wc:WNDCLASSEX
    LOCAL msg:MSG
    
    mov eax, hInst
    mov hInstance, eax
    
    ; Register window class
    mov wc.cbSize, SIZEOF WNDCLASSEX
    mov wc.style, CS_HREDRAW or CS_VREDRAW
    mov wc.lpfnWndProc, OFFSET WndProc
    mov wc.cbClsExtra, 0
    mov wc.cbWndExtra, 0
    mov eax, hInstance
    mov wc.hInstance, eax
    mov wc.hbrBackground, COLOR_WINDOW+1
    mov wc.lpszMenuName, NULL
    mov wc.lpszClassName, OFFSET szClassName
    
    invoke LoadIcon, NULL, IDI_APPLICATION
    mov wc.hIcon, eax
    mov wc.hIconSm, eax
    
    invoke LoadCursor, NULL, IDC_ARROW
    mov wc.hCursor, eax
    
    invoke RegisterClassEx, ADDR wc
    
    ; Create window
    invoke CreateWindowEx, NULL, ADDR szClassName, ADDR szWindowTitle,
                          WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                          800, 600, NULL, NULL, hInstance, NULL
    mov hWnd, eax
    
    ; Show window
    invoke ShowWindow, hWnd, SW_SHOW
    invoke UpdateWindow, hWnd
    
    ; Message loop
    .WHILE TRUE
        invoke GetMessage, ADDR msg, NULL, 0, 0
        .BREAK .IF (!eax)
        
        invoke TranslateMessage, ADDR msg
        invoke DispatchMessage, ADDR msg
    .ENDW
    
    mov eax, msg.wParam
    ret
WinMain endp

; Window procedure
WndProc proc hWin:DWORD, uMsg:DWORD, wParam:DWORD, lParam:DWORD
    LOCAL ps:PAINTSTRUCT
    LOCAL hdc:DWORD
    
    .IF uMsg == WM_DESTROY
        invoke PostQuitMessage, 0
        xor eax, eax
        ret
    .ELSEIF uMsg == WM_PAINT
        invoke BeginPaint, hWin, ADDR ps
        mov hdc, eax
        
        ; Draw simple interface
        invoke TextOut, hdc, 10, 10, ADDR szWindowTitle, LENGTHOF szWindowTitle
        invoke TextOut, hdc, 10, 40, ADDR szChannelRack, LENGTHOF szChannelRack
        invoke TextOut, hdc, 10, 70, ADDR szControls, LENGTHOF szControls
        
        invoke EndPaint, hWin, ADDR ps
        xor eax, eax
        ret
    .ELSEIF uMsg == WM_KEYDOWN
        .IF wParam == VK_SPACE
            ; Toggle play/pause
            invoke InvalidateRect, hWin, NULL, TRUE
        .ELSEIF wParam == 82  ; 'R'
            ; Hot-reload
            invoke MessageBox, hWin, ADDR szWindowTitle, ADDR szWindowTitle, MB_OK
        .ELSEIF wParam == 76  ; 'L'
            ; Load AI model
            invoke MessageBox, hWin, ADDR szWindowTitle, ADDR szWindowTitle, MB_OK
        .ENDIF
    .ENDIF
    
    invoke DefWindowProc, hWin, uMsg, wParam, lParam
    ret
WndProc endp

end start