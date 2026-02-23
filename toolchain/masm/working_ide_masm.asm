; Minimal MASM64 IDE window skeleton (WinMain + WindowProc)
; Uses Windows API directly, compliant with x64 calling conventions.

EXTERN ExitProcess:PROC
EXTERN GetModuleHandleA:PROC
EXTERN RegisterClassExA:PROC
EXTERN CreateWindowExA:PROC
EXTERN ShowWindow:PROC
EXTERN UpdateWindow:PROC
EXTERN GetMessageA:PROC
EXTERN TranslateMessage:PROC
EXTERN DispatchMessageA:PROC
EXTERN DefWindowProcA:PROC
EXTERN PostQuitMessage:PROC
EXTERN LoadIconA:PROC
EXTERN LoadCursorA:PROC
EXTERN GetStockObject:PROC

; Constants
WM_DESTROY EQU 0002h
CS_VREDRAW EQU 0001h
CS_HREDRAW EQU 0002h
WS_OVERLAPPEDWINDOW EQU 00CF0000h
SW_SHOW EQU 1
IDI_APPLICATION EQU 32512
IDC_ARROW EQU 32512
COLOR_WINDOW EQU 5

.data
szClassName db "ASM_IDE_CLASS", 0
szWindowTitle db "Assembly IDE (MASM64)", 0

; WNDCLASSEX (80 bytes) and MSG (48 bytes) buffers
wc db 80 dup(0)
msg db 48 dup(0)

hInstance dq 0
hWnd dq 0

.code

WindowProc PROC
    ; RCX=hwnd, RDX=uMsg, R8=wParam, R9=lParam
    cmp edx, WM_DESTROY
    jne @F
    ; PostQuitMessage(0)
    xor ecx, ecx
    call PostQuitMessage
    xor eax, eax
    ret
@@:
    ; return DefWindowProcA(hwnd, uMsg, wParam, lParam)
    ; RCX, RDX, R8, R9 already in place
    call DefWindowProcA
    ret
WindowProc ENDP

WinMain PROC
    ; Prologue with shadow space
    push rbp
    mov rbp, rsp
    sub rsp, 32

    ; Get HINSTANCE
    xor ecx, ecx
    call GetModuleHandleA
    mov [hInstance], rax

    ; Zero WNDCLASSEX (80 bytes)
    lea rdi, wc
    mov ecx, 80
    xor eax, eax
    rep stosb

    ; Fill WNDCLASSEX
    ; cbSize (DWORD) = 80
    mov dword ptr [wc + 0], 80
    ; style (DWORD) = CS_HREDRAW | CS_VREDRAW
    mov dword ptr [wc + 4], CS_HREDRAW + CS_VREDRAW
    ; lpfnWndProc (QWORD)
    lea rax, WindowProc
    mov qword ptr [wc + 8], rax
    ; cbClsExtra, cbWndExtra (DWORD)
    mov dword ptr [wc + 16], 0
    mov dword ptr [wc + 20], 0
    ; hInstance (QWORD)
    mov rax, [hInstance]
    mov qword ptr [wc + 24], rax
    ; hIcon
    xor ecx, ecx
    mov edx, IDI_APPLICATION
    call LoadIconA
    mov qword ptr [wc + 32], rax
    ; hCursor
    xor ecx, ecx
    mov edx, IDC_ARROW
    call LoadCursorA
    mov qword ptr [wc + 40], rax
    ; hbrBackground
    mov ecx, COLOR_WINDOW
    call GetStockObject
    mov qword ptr [wc + 48], rax
    ; lpszMenuName
    mov qword ptr [wc + 56], 0
    ; lpszClassName
    lea rax, szClassName
    mov qword ptr [wc + 64], rax
    ; hIconSm
    mov qword ptr [wc + 72], 0

    ; RegisterClassExA(&wc)
    lea rcx, wc
    call RegisterClassExA
    test eax, eax
    jz _error

    ; CreateWindowExA
    ; RCX=dwExStyle=0
    ; RDX=lpClassName
    ; R8=lpWindowName
    ; R9=dwStyle
    xor ecx, ecx
    lea rdx, szClassName
    lea r8, szWindowTitle
    mov r9d, WS_OVERLAPPEDWINDOW

    ; Reserve shadow space for API call
    sub rsp, 32
    ; Stack args beyond R9:
    ; x,y,width,height,hWndParent,hMenu,hInstance,lpParam
    mov dword ptr [rsp + 20h], 100        ; x
    mov dword ptr [rsp + 28h], 100        ; y
    mov dword ptr [rsp + 30h], 1200       ; width
    mov dword ptr [rsp + 38h], 800        ; height
    mov qword ptr [rsp + 40h], 0          ; hWndParent
    mov qword ptr [rsp + 48h], 0          ; hMenu
    mov rax, [hInstance]
    mov qword ptr [rsp + 50h], rax        ; hInstance
    mov qword ptr [rsp + 58h], 0          ; lpParam

    call CreateWindowExA
    add rsp, 32

    test rax, rax
    jz _error
    mov [hWnd], rax

    ; Show + Update
    mov rcx, [hWnd]
    mov edx, SW_SHOW
    call ShowWindow

    mov rcx, [hWnd]
    call UpdateWindow

    ; Message loop
_msg_loop:
    lea rcx, msg
    xor edx, edx
    xor r8d, r8d
    xor r9d, r9d
    call GetMessageA
    test eax, eax
    jle _exit

    lea rcx, msg
    call TranslateMessage

    lea rcx, msg
    call DispatchMessageA
    jmp _msg_loop

_exit:
    ; wParam at MSG+16
    mov rax, qword ptr [msg + 16]
    jmp _done

_error:
    xor eax, eax

_done:
    add rsp, 32
    pop rbp
    ret
WinMain ENDP

END
