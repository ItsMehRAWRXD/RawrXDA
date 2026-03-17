; RawrXD_Sovereign.asm
; v16.0.0-SOVEREIGN: The Single-File Native Entry Point
; Links: RawrXD_Native_Core.dll, RawrXD_Native_UI.dll, D3D12FlashAttention.dll
; Purpose: Orchestrate the Great Handover from PowerShell to Native x64

EXTERN CreateRawrXDTriPane : PROC
EXTERN ResizeRawrXDLanes : PROC
EXTERN RunNativeMessageLoop : PROC
EXTERN GetModuleHandleA : PROC
EXTERN CreateWindowExA : PROC
EXTERN RegisterClassExA : PROC
EXTERN DefWindowProcA : PROC
EXTERN LoadCursorA : PROC
EXTERN PostQuitMessage : PROC
EXTERN ShowWindow : PROC
EXTERN UpdateWindow : PROC

.data
    szClassName DB "RawrXDSovereignClass", 0
    szAppName   DB "RawrXD Sovereign v16.0.0-SOVEREIGN", 0
    hInst       DQ 0
    hWndMain    DQ 0

.code
RawrXD_Main PROC
    sub rsp, 40             ; Shadow space

    ; 1. Get Instance
    xor rcx, rcx
    call GetModuleHandleA
    mov hInst, rax

    ; 2. Register Window Class
    ; (Simplified for bootstrap: using stack for WNDCLASSEX)
    sub rsp, 80             ; WNDCLASSEX size is 80 bytes
    mov dword ptr [rsp], 80 ; cbSize
    mov dword ptr [rsp+4], 3 ; style (CS_HREDRAW | CS_VREDRAW)
    lea rax, MainWndProc
    mov [rsp+8], rax        ; lpfnWndProc
    mov dword ptr [rsp+16], 0 ; cbClsExtra
    mov dword ptr [rsp+20], 0 ; cbWndExtra
    mov rax, hInst
    mov [rsp+24], rax       ; hInstance
    mov qword ptr [rsp+32], 0 ; hIcon
    xor rcx, rcx
    mov rdx, 32512          ; IDC_ARROW
    call LoadCursorA
    mov [rsp+40], rax       ; hCursor
    mov qword ptr [rsp+48], 0 ; hbrBackground
    mov qword ptr [rsp+56], 0 ; lpszMenuName
    lea rax, szClassName
    mov [rsp+64], rax       ; lpszClassName
    mov qword ptr [rsp+72], 0 ; hIconSm

    mov rcx, rsp
    call RegisterClassExA
    add rsp, 80

    ; 3. Create Main Window
    xor rcx, rcx            ; dwExStyle
    lea rdx, szClassName    ; lpClassName
    lea r8, szAppName       ; lpWindowName
    mov r9d, 0CF0000h       ; dwStyle (WS_OVERLAPPEDWINDOW)
    mov dword ptr [rsp+32], 200 ; x
    mov dword ptr [rsp+40], 200 ; y
    mov dword ptr [rsp+48], 1280; nWidth
    mov dword ptr [rsp+56], 720 ; nHeight
    mov qword ptr [rsp+64], 0   ; hWndParent
    mov qword ptr [rsp+72], 0   ; hMenu
    mov rax, hInst
    mov [rsp+80], rax       ; hInstance
    mov qword ptr [rsp+88], 0   ; lpParam
    call CreateWindowExA
    mov hWndMain, rax

    ; 4. Initialize Native UI (Cross-Link)
    mov rcx, hWndMain
    call CreateRawrXDTriPane

    ; 5. Show Window
    mov rcx, hWndMain
    mov rdx, 5              ; SW_SHOW
    call ShowWindow
    mov rcx, hWndMain
    call UpdateWindow

    ; 6. Message Loop
    call RunNativeMessageLoop

    xor rcx, rcx
    add rsp, 40
    ret
RawrXD_Main ENDP

MainWndProc PROC
    ; rcx: hWnd, rdx: uMsg, r8: wParam, r9: lParam
    cmp rdx, 2              ; WM_DESTROY
    je .msg_destroy
    cmp rdx, 5              ; WM_SIZE
    je .msg_size
    
    jmp DefWindowProcA

.msg_destroy:
    xor rcx, rcx
    call PostQuitMessage
    xor rax, rax
    ret

.msg_size:
    ; Forward to UI Engine for layout adjustment
    mov rcx, rcx            ; hWnd
    call ResizeRawrXDLanes
    xor rax, rax
    ret
MainWndProc ENDP

END
