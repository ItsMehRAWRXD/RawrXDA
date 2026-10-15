; RawrXD_TextEditorGUI_hardened.asm
; Standalone Unicode Win32 editor foundation. Native EDIT owns text allocation,
; selection, caret, IME, undo and rendering. Not a replacement for the IDE ABI.
OPTION CASEMAP:NONE
EXTERN GetModuleHandleW:PROC
EXTERN RegisterClassExW:PROC
EXTERN CreateWindowExW:PROC
EXTERN DefWindowProcW:PROC
EXTERN ShowWindow:PROC
EXTERN UpdateWindow:PROC
EXTERN GetMessageW:PROC
EXTERN TranslateMessage:PROC
EXTERN DispatchMessageW:PROC
EXTERN PostQuitMessage:PROC
EXTERN MoveWindow:PROC
EXTERN GetClientRect:PROC
EXTERN GetWindowLongPtrW:PROC
EXTERN SetWindowLongPtrW:PROC
EXTERN DestroyWindow:PROC
EXTERN MessageBoxW:PROC
EXTERN ExitProcess:PROC
EXTERN SendMessageW:PROC
EXTERN GetLastError:PROC
EXTERN OutputDebugStringW:PROC

WM_CREATE EQU 1
WM_DESTROY EQU 2
WM_SIZE EQU 5
WM_SETFOCUS EQU 7
WM_COMMAND EQU 111h
WM_GETTEXTLENGTH EQU 0Eh
EM_SETLIMITTEXT EQU 0C5h
GWLP_USERDATA EQU -21
WS_OVERLAPPEDWINDOW EQU 00CF0000h
WS_VISIBLE EQU 10000000h
WS_CHILD EQU 40000000h
WS_VSCROLL EQU 00200000h
WS_HSCROLL EQU 00100000h
ES_MULTILINE EQU 4
ES_AUTOVSCROLL EQU 40h
ES_AUTOHSCROLL EQU 80h
ES_WANTRETURN EQU 1000h

.data
align 8
hInstance dq 0
hMain dq 0
hEdit dq 0
className dw 'R','a','w','r','X','D','H','a','r','d','e','n','e','d',0
titleText dw 'R','a','w','r','X','D',' ','H','a','r','d','e','n','e','d',' ','E','d','i','t','o','r',0
editClass dw 'E','D','I','T',0
errorText dw 'W','i','n','3','2',' ','e','d','i','t','o','r',' ','i','n','i','t',' ','f','a','i','l','e','d',0
logStart dw '[','R','a','w','r','X','D',']',' ','s','t','a','r','t',0
align 8
wc db 80 dup(0)
msg db 48 dup(0)
rect dd 4 dup(0)

.code
WndProc PROC FRAME
    push rbx
    .pushreg rbx
    sub rsp, 48
    .allocstack 48
    .endprolog
    mov rbx, rcx
    cmp edx, WM_CREATE
    je oncreate
    cmp edx, WM_SIZE
    je onsize
    cmp edx, WM_SETFOCUS
    je onfocus
    cmp edx, WM_DESTROY
    je ondestroy
defaultmsg:
    mov rcx, rbx
    call DefWindowProcW
    jmp wndexit
oncreate:
    xor ecx, ecx
    lea rdx, editClass
    xor r8d, r8d
    mov r9d, WS_CHILD or WS_VISIBLE or WS_VSCROLL or WS_HSCROLL or ES_MULTILINE or ES_AUTOVSCROLL or ES_AUTOHSCROLL or ES_WANTRETURN
    mov qword ptr [rsp+32], 0
    mov qword ptr [rsp+40], 0
    ; Stack arguments require 12 slots; use a dedicated 104-byte call frame.
    sub rsp, 64
    mov qword ptr [rsp+32], 0
    mov qword ptr [rsp+40], 0
    mov qword ptr [rsp+48], 800
    mov qword ptr [rsp+56], 600
    mov qword ptr [rsp+64], rbx
    mov qword ptr [rsp+72], 0
    mov rax, hInstance
    mov qword ptr [rsp+80], rax
    mov qword ptr [rsp+88], 0
    call CreateWindowExW
    add rsp, 64
    mov hEdit, rax
    test rax, rax
    jz createfail
    mov rcx, rax
    mov edx, EM_SETLIMITTEXT
    mov r8d, 7FFFFFFEh
    xor r9d, r9d
    call SendMessageW
    xor eax, eax
    jmp wndexit
createfail:
    mov rax, -1
    jmp wndexit
onsize:
    mov rcx, rbx
    lea rdx, rect
    call GetClientRect
    test eax, eax
    jz defaultmsg
    mov rcx, hEdit
    test rcx, rcx
    jz defaultmsg
    xor edx, edx
    xor r8d, r8d
    mov r9d, rect[8]
    sub r9d, rect[0]
    mov eax, rect[12]
    sub eax, rect[4]
    mov dword ptr [rsp+32], eax
    mov dword ptr [rsp+40], 1
    call MoveWindow
    xor eax, eax
    jmp wndexit
onfocus:
    ; Focus remains managed by the native EDIT control.
    xor eax, eax
    jmp wndexit
ondestroy:
    xor ecx, ecx
    call PostQuitMessage
    xor eax, eax
wndexit:
    add rsp, 48
    pop rbx
    ret
WndProc ENDP

main PROC FRAME
    sub rsp, 120
    .allocstack 120
    .endprolog
    lea rcx, logStart
    call OutputDebugStringW
    xor ecx, ecx
    call GetModuleHandleW
    test rax, rax
    jz fatal
    mov hInstance, rax
    lea r10, wc
    mov dword ptr [r10], 80
    mov dword ptr [r10+4], 3
    lea rax, WndProc
    mov qword ptr [r10+8], rax
    mov rax, hInstance
    mov qword ptr [r10+24], rax
    lea rax, className
    mov qword ptr [r10+64], rax
    lea rcx, wc
    call RegisterClassExW
    test ax, ax
    jz fatal
    xor ecx, ecx
    lea rdx, className
    lea r8, titleText
    mov r9d, WS_OVERLAPPEDWINDOW or WS_VISIBLE
    mov qword ptr [rsp+32], 100
    mov qword ptr [rsp+40], 100
    mov qword ptr [rsp+48], 1000
    mov qword ptr [rsp+56], 700
    mov qword ptr [rsp+64], 0
    mov qword ptr [rsp+72], 0
    mov rax, hInstance
    mov qword ptr [rsp+80], rax
    mov qword ptr [rsp+88], 0
    call CreateWindowExW
    test rax, rax
    jz fatal
    mov hMain, rax
    mov rcx, rax
    mov edx, 5
    call ShowWindow
    mov rcx, hMain
    call UpdateWindow
loopmsg:
    lea rcx, msg
    xor edx, edx
    xor r8d, r8d
    xor r9d, r9d
    call GetMessageW
    cmp eax, -1
    je fatal
    test eax, eax
    jz success
    lea rcx, msg
    call TranslateMessage
    lea rcx, msg
    call DispatchMessageW
    jmp loopmsg
success:
    mov ecx, dword ptr msg[16]
    call ExitProcess
fatal:
    xor ecx, ecx
    lea rdx, errorText
    lea r8, titleText
    mov r9d, 10h
    call MessageBoxW
    mov ecx, 1
    call ExitProcess
main ENDP
END
