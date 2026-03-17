; RawrXD Agentic IDE - Debug Test (Pure MASM)
; Simple test to diagnose window creation issues

.686
.model flat, stdcall
option casemap:none

include \masm32\include\windows.inc
include \masm32\include\kernel32.inc
include \masm32\include\user32.inc
include \masm32\include\gdi32.inc

includelib \masm32\lib\kernel32.lib
includelib \masm32\lib\user32.lib
includelib \masm32\lib\gdi32.lib

.data
    szDebugTitle db "Debug",0
    szWindowCreated db "Window created successfully!",0
    szWindowDestroyed db "Window destroyed!",0
    szClassName db "TestWindow",0
    szTitle db "Test Window",0

.code

WinMain proc hInstance:DWORD, hPrevInstance:DWORD, lpCmdLine:DWORD, nCmdShow:DWORD
    LOCAL wc:WNDCLASSEX
    LOCAL hwnd:DWORD
    LOCAL msg:MSG
    
    ; Register window class
    mov wc.cbSize, sizeof WNDCLASSEX
    mov wc.style, CS_HREDRAW or CS_VREDRAW
    lea eax, WndProc
    mov wc.lpfnWndProc, eax
    mov eax, hInstance
    mov wc.hInstance, eax
    mov wc.hIcon, 0
    mov wc.hIconSm, 0
    invoke LoadCursor, NULL, IDC_ARROW
    mov wc.hCursor, eax
    invoke GetStockObject, WHITE_BRUSH
    mov wc.hbrBackground, eax
    mov wc.lpszMenuName, 0
    lea eax, szClassName
    mov wc.lpszClassName, eax
    
    invoke RegisterClassEx, addr wc
    test eax, eax
    jz @Failed
    
    ; Create window
    invoke CreateWindowEx, 0, addr szClassName, addr szTitle, WS_OVERLAPPEDWINDOW, 
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 600, NULL, NULL, hInstance, NULL
    mov hwnd, eax
    test eax, eax
    jz @Failed
    
    ; Show debug message
    invoke MessageBox, hwnd, addr szWindowCreated, addr szDebugTitle, MB_OK
    
    ; Show window
    invoke ShowWindow, hwnd, nCmdShow
    invoke UpdateWindow, hwnd
    
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
    
@Failed:
    mov eax, 1
    ret
WinMain endp

WndProc proc hwnd:DWORD, uMsg:DWORD, wParam:DWORD, lParam:DWORD
    .if uMsg == WM_DESTROY
        invoke MessageBox, hwnd, addr szWindowDestroyed, addr szDebugTitle, MB_OK
        invoke PostQuitMessage, 0
        xor eax, eax
    .else
        invoke DefWindowProc, hwnd, uMsg, wParam, lParam
    .endif
    ret
WndProc endp

end WinMain