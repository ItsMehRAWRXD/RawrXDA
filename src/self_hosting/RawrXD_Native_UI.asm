; ==============================================================================
; RawrXD Native UI - Draggable Splitters & 3-Pane Layout
; Module: RawrXD_Native_UI.asm
; Purpose: Bare-metal x64 UI engine replacing .NET WinForms
; ==============================================================================

EXTERN RegisterClassExA:PROC
EXTERN CreateWindowExA:PROC
EXTERN ShowWindow:PROC
EXTERN UpdateWindow:PROC
EXTERN GetMessageA:PROC
EXTERN TranslateMessage:PROC
EXTERN DispatchMessageA:PROC
EXTERN PostQuitMessage:PROC
EXTERN DefWindowProcA:PROC
EXTERN SendMessageA:PROC
EXTERN LoadCursorA:PROC
EXTERN LoadIconA:PROC
EXTERN MoveWindow:PROC
EXTERN SetCapture:PROC
EXTERN ReleaseCapture:PROC

EXTERN Dock_Initialize:PROC
EXTERN Dock_GetHostWindow:PROC
EXTERN Dock_UpdateSize:PROC
EXTERN Dock_RegisterPanels:PROC

.data
    szClassName         db "RawrXD_Native_Class", 0
    szWindowTitle       db "RawrXD native IDE v15.2.0-TRIPANE", 0
    szEditClass         db "EDIT", 0
    szTreeViewClass     db "SysTreeView32", 0

    hwndMain            dq 0
    hwndDockHost        dq 0
    hwndExplorer        dq 0
    PUBLIC hwndEditor
    hwndEditor          dq 0
    hwndChat            dq 0

    leftPanelWidth      dd 200
    rightPanelWidth     dd 300
    splitterWidth       dd 4
    clientWidth         dd 1200
    clientHeight        dd 800
    
    isDragging1         db 0
    isDragging2         db 0

.code

; ------------------------------------------------------------------------------
; Core_InitializeUI
; ------------------------------------------------------------------------------
Core_InitializeUI PROC
    push r12
    push r13
    push rbp
    mov rbp, rsp
    sub rsp, 100h

    mov rcx, 0
    mov rdx, 32512      ; IDC_ARROW
    call LoadCursorA
    mov r12, rax

    mov rcx, 0
    mov rdx, 32512      ; IDI_APPLICATION
    call LoadIconA
    mov r13, rax

    ; WNDCLASSEX
    mov rcx, 0
    call Dock_Initialize

    mov dword ptr [rsp+20h], 80         ; cbSize (80 bytes in x64)
    mov dword ptr [rsp+24h], 3          ; style (CS_HREDRAW | CS_VREDRAW)
    lea rax, MainWndProc
    mov qword ptr [rsp+28h], rax        ; lpfnWndProc
    mov dword ptr [rsp+30h], 0          ; cbClsExtra
    mov dword ptr [rsp+34h], 0          ; cbWndExtra
    mov qword ptr [rsp+38h], 0          ; hInstance
    mov qword ptr [rsp+40h], r13        ; hIcon
    mov qword ptr [rsp+48h], r12        ; hCursor
    mov qword ptr [rsp+50h], 16         ; hbrBackground (COLOR_BTNFACE + 1)
    mov qword ptr [rsp+58h], 0          ; lpszMenuName
    lea rax, szClassName
    mov qword ptr [rsp+60h], rax        ; lpszClassName
    mov qword ptr [rsp+68h], r13        ; hIconSm

    lea rcx, [rsp+20h]
    call RegisterClassExA

    mov rcx, 0
    lea rdx, szClassName
    lea r8, szWindowTitle
    mov r9, 02CF0000h                   ; WS_CLIPCHILDREN | WS_OVERLAPPEDWINDOW
    
    mov dword ptr [rsp+20h], 80000000h
    mov dword ptr [rsp+28h], 80000000h
    mov eax, clientWidth
    mov dword ptr [rsp+30h], eax
    mov eax, clientHeight
    mov dword ptr [rsp+38h], eax
    mov qword ptr [rsp+40h], 0
    mov qword ptr [rsp+48h], 0
    mov qword ptr [rsp+50h], 0
    mov qword ptr [rsp+58h], 0
    call CreateWindowExA
    
    mov hwndMain, rax

    ; Create DockHost child
    mov rcx, hwndMain
    call Dock_GetHostWindow
    mov hwndDockHost, rax
    
    ; Explorer Pane
    mov rcx, 0
    lea rdx, szTreeViewClass
    mov r8, 0
    mov r9, 50800000h
    mov dword ptr [rsp+20h], 0
    mov dword ptr [rsp+28h], 0
    mov eax, leftPanelWidth
    mov dword ptr [rsp+30h], eax
    mov dword ptr [rsp+38h], 200
    mov rax, hwndDockHost
    mov qword ptr [rsp+40h], rax
    mov qword ptr [rsp+48h], 1001
    mov qword ptr [rsp+50h], 0
    mov qword ptr [rsp+58h], 0
    call CreateWindowExA
    mov hwndExplorer, rax

    ; Populate Explorer TreeView
    mov rcx, hwndExplorer
    mov rdx, 0
    EXTERN Core_PopulateTreeView:PROC
    call Core_PopulateTreeView

    ; Editor Pane
    mov rcx, 0
    lea rdx, szEditClass
    mov r8, 0
    mov r9, 50A00104h                   ; WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE 
    mov dword ptr [rsp+20h], 0
    mov dword ptr [rsp+28h], 0
    mov dword ptr [rsp+30h], 200
    mov dword ptr [rsp+38h], 200
    mov rax, hwndDockHost
    mov qword ptr [rsp+40h], rax
    mov qword ptr [rsp+48h], 1002
    mov qword ptr [rsp+50h], 0
    mov qword ptr [rsp+58h], 0
    call CreateWindowExA
    mov hwndEditor, rax

    ; Chat Pane
    mov rcx, 0
    lea rdx, szEditClass
    mov r8, 0
    mov r9, 50A00104h
    mov dword ptr [rsp+20h], 0
    mov dword ptr [rsp+28h], 0
    mov dword ptr [rsp+30h], 200
    mov dword ptr [rsp+38h], 200
    mov rax, hwndDockHost
    mov qword ptr [rsp+40h], rax
    mov qword ptr [rsp+48h], 1003
    mov qword ptr [rsp+50h], 0
    mov qword ptr [rsp+58h], 0
    call CreateWindowExA
    mov hwndChat, rax

    mov rcx, hwndExplorer
    mov rdx, hwndEditor
    mov r8, hwndChat
    call Dock_RegisterPanels

    mov rcx, hwndMain
    mov rdx, 5                          ; SW_SHOW
    call ShowWindow

    mov rcx, hwndMain
    call UpdateWindow

    mov rax, 1
    add rsp, 100h
    pop rbp
    pop r13
    pop r12
    ret
Core_InitializeUI ENDP

; ------------------------------------------------------------------------------
; MainWndProc
; ------------------------------------------------------------------------------
MainWndProc PROC
    push rbp
    mov rbp, rsp
    sub rsp, 80h

    mov [rbp-8], rbx
    mov [rbp-10h], r12
    mov [rbp-18h], r13

    mov [rbp+10h], rcx
    mov [rbp+18h], rdx
    mov [rbp+20h], r8
    mov [rbp+28h], r9
    
    cmp rdx, 2          ; WM_DESTROY
    je _msg_destroy
    cmp rdx, 5          ; WM_SIZE
    je _msg_size
    cmp rdx, 004Eh      ; WM_NOTIFY
    je _msg_notify
    cmp rdx, 0200h      ; WM_MOUSEMOVE
    je _msg_mousemove
    cmp rdx, 0201h      ; WM_LBUTTONDOWN
    je _msg_lbuttondown
    cmp rdx, 0202h      ; WM_LBUTTONUP
    je _msg_lbuttonup
    
_msg_default:
    mov rcx, [rbp+10h]
    mov rdx, [rbp+18h]
    mov r8, [rbp+20h]
    mov r9, [rbp+28h]
    call DefWindowProcA
    jmp _proc_exit

_msg_notify:
    ; Check if code is NM_DBLCLK
    ; lParam (r9) points to NMHDR
    mov eax, dword ptr [r9+16]          ; NMHDR.code is at offset 16
    cmp eax, -3                         ; NM_DBLCLK = -3
    jne _msg_default

    ; For now, if double click, we just load a dummy test into the editor to prove it hooks
    EXTERN Core_LoadDummyEditorTest:PROC
    call Core_LoadDummyEditorTest
    
    xor rax, rax
    jmp _proc_exit

_msg_destroy:
    mov rcx, 0
    call PostQuitMessage
    xor rax, rax
    jmp _proc_exit

_msg_size:
    ; LOWORD(lParam)=Width, HIWORD(lParam)=Height
    mov rbx, [rbp+28h]
    movsx r12, bx
    shr rbx, 16
    movsx r13, bx
    mov clientWidth, r12d
    mov clientHeight, r13d
    
    ; Explorer
    mov rcx, hwndExplorer
    mov rdx, 0
    mov r8, 0
    mov r9d, leftPanelWidth
    mov dword ptr [rsp+20h], r13d
    mov qword ptr [rsp+28h], 1
    call MoveWindow
    
    ; Chat
    mov rcx, hwndChat
    mov rdx, r12
    sub edx, rightPanelWidth
    mov r8, 0
    mov r9d, rightPanelWidth
    mov dword ptr [rsp+20h], r13d
    mov qword ptr [rsp+28h], 1
    call MoveWindow
    
    ; Editor
    mov rcx, hwndEditor
    mov edx, leftPanelWidth
    add edx, splitterWidth
    mov r8, 0
    mov r9d, r12d
    sub r9d, leftPanelWidth
    sub r9d, rightPanelWidth
    sub r9d, splitterWidth
    sub r9d, splitterWidth
    mov dword ptr [rsp+20h], r13d
    mov qword ptr [rsp+28h], 1
    call MoveWindow
    
    xor rax, rax
    jmp _proc_exit
    
_msg_lbuttondown:
    mov ebx, dword ptr [rbp+28h]
    movsx r12d, bx          ; X
    
    mov eax, leftPanelWidth
    cmp r12d, eax
    jl _chk_split2
    add eax, splitterWidth
    cmp r12d, eax
    jg _chk_split2
    
    mov isDragging1, 1
    mov rcx, [rbp+10h]
    call SetCapture
    xor rax, rax
    jmp _proc_exit

_chk_split2:
    mov eax, clientWidth
    sub eax, rightPanelWidth
    sub eax, splitterWidth
    cmp r12d, eax
    jl _lbutton_end
    add eax, splitterWidth
    cmp r12d, eax
    jg _lbutton_end
    
    mov isDragging2, 1
    mov rcx, [rbp+10h]
    call SetCapture
    xor rax, rax
    jmp _proc_exit

_lbutton_end:
    jmp _msg_default

_msg_mousemove:
    mov al, isDragging1
    test al, al
    jnz _drag1
    
    mov al, isDragging2
    test al, al
    jnz _drag2
    
    jmp _msg_default

_drag1:
    mov ebx, dword ptr [rbp+28h]
    movsx r12d, bx
    cmp r12d, 50
    jge _drag1_nok_min
    mov r12d, 50
_drag1_nok_min:
    mov eax, clientWidth
    sub eax, rightPanelWidth
    sub eax, 100
    cmp r12d, eax
    jle _drag1_nok_max
    mov r12d, eax
_drag1_nok_max:
    mov leftPanelWidth, r12d
    
    ; SendMessageA(hWnd, WM_SIZE, 0, (clientHeight << 16) | clientWidth)
    mov rcx, [rbp+10h]
    mov rdx, 5
    mov r8, 0
    mov r9d, clientHeight
    shl r9d, 16
    mov eax, clientWidth
    and eax, 0FFFFh
    or r9d, eax
    call SendMessageA
    xor rax, rax
    jmp _proc_exit

_drag2:
    mov ebx, dword ptr [rbp+28h]
    movsx r12d, bx
    mov eax, clientWidth
    sub eax, r12d
    sub eax, splitterWidth
    cmp eax, 50
    jge _drag2_nok_min
    mov eax, 50
_drag2_nok_min:
    mov ebx, clientWidth
    sub ebx, leftPanelWidth
    sub ebx, 100
    cmp eax, ebx
    jle _drag2_nok_max
    mov eax, ebx
_drag2_nok_max:
    mov rightPanelWidth, eax

    mov rcx, [rbp+10h]
    mov rdx, 5
    mov r8, 0
    mov r9d, clientHeight
    shl r9d, 16
    mov eax, clientWidth
    and eax, 0FFFFh
    or r9d, eax
    call SendMessageA
    xor rax, rax
    jmp _proc_exit
    
_msg_lbuttonup:
    mov isDragging1, 0
    mov isDragging2, 0
    call ReleaseCapture
    xor rax, rax
    jmp _proc_exit

_proc_exit:
    mov rbx, [rbp-8]
    mov r12, [rbp-10h]
    mov r13, [rbp-18h]
    add rsp, 80h
    pop rbp
    ret
MainWndProc ENDP

; ------------------------------------------------------------------------------
; Core_RunMessageLoop
; ------------------------------------------------------------------------------
Core_RunMessageLoop PROC
    push rbp
    mov rbp, rsp
    sub rsp, 40h
    
message_loop:
    xor r9d, r9d
    xor r8d, r8d
    xor edx, edx
    lea rcx, [rsp+20h]
    call GetMessageA
    
    cmp eax, 0
    jle exit_loop
    
    lea rcx, [rsp+20h]
    call TranslateMessage
    lea rcx, [rsp+20h]
    call DispatchMessageA
    jmp message_loop
    
exit_loop:
    mov eax, dword ptr [rsp+20h+16]
    add rsp, 40h
    pop rbp
    ret
Core_RunMessageLoop ENDP

END
