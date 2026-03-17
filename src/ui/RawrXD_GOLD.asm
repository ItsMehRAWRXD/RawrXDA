; RawrXD Native Core - v15.9.0-GOLD-STABLE-FIXED
; 100% Sovereign Architecture

extrn OutputDebugStringA : proc
extrn GetModuleHandleA : proc
extrn LoadLibraryA : proc
extrn LoadCursorA : proc
extrn RegisterClassExA : proc
extrn CreateWindowExA : proc
extrn ShowWindow : proc
extrn UpdateWindow : proc
extrn GetMessageA : proc
extrn TranslateMessage : proc
extrn DispatchMessageA : proc
extrn PostQuitMessage : proc
extrn DefWindowProcA : proc
extrn InitCommonControlsEx : proc
extrn MoveWindow : proc
extrn SendMessageA : proc
extrn CreateMenu : proc
extrn AppendMenuA : proc
extrn SetMenu : proc
extrn CreateStatusWindowA : proc
extrn CreateAcceleratorTableA : proc
extrn TranslateAcceleratorA : proc
extrn MessageBoxA : proc
extrn FindFirstFileA : proc
extrn FindNextFileA : proc
extrn FindClose : proc
extrn VirtualAlloc : proc

; Constants
WM_USER             equ 0400h
EN_CHANGE           equ 0300h
WM_NOTIFY           equ 004Eh
TV_FIRST            equ 1100h
TVM_INSERTITEMA     equ (TV_FIRST + 0)
TVM_DELETEALLITEMS  equ (TV_FIRST + 1)
PAGE_EXECUTE_READWRITE equ 40h

.data
className       db "RawrXD_Native_UI", 0
windowTitle     db "RawrXD v15.9.0 - GOLD SOVEREIGN (Native x64)", 0
treeClass       db "SysTreeView32", 0
richEditDll     db "Msftedit.dll", 0
richEditClass   db "RichEdit50W", 0
editClass       db "EDIT", 0
statusClass     db "msctls_statusbar32", 0
rootPath        db "D:\*.*", 0

; JIT State
jitBuffer       dq 0
jitSize         dq 4096

; Global state
hExplorer       dq 0
hEditor         dq 0
hChat           dq 0
hStatus         dq 0
hAccel          dq 0

align 8
tvInsertStruct:
    dq 0, 0     ; hParent, hInsertAfter
    dd 1, 0     ; mask (TVIF_TEXT), padding
    dq 0        ; hItem
    dd 0, 0     ; state, stateMask
    dq 0        ; pszText
    dd 0, 0, 0, 0 ; cchTextMax, iImage, iSelectedImage, cChildren
    dq 0        ; lParam

align 8
findData:
    dd 0        ; dwFileAttributes
    dq 0, 0, 0  ; times
    dd 0, 0     ; sizes
    dd 0, 0     ; reserved
    db 260 dup(0) ; cFileName
    db 14 dup(0) ; cAlt

.code

; --- JIT Engine ---
Core_AssembleBuffer proc
    push rbp
    mov rbp, rsp
    sub rsp, 32
    xor rcx, rcx
    mov rdx, jitSize
    mov r8d, 3000h
    mov r9d, PAGE_EXECUTE_READWRITE
    call VirtualAlloc
    test rax, rax
    jz @err
    mov jitBuffer, rax
    ; Emit identity opcode: mov rax, rcx; ret
    mov byte ptr [rax], 48h
    mov byte ptr [rax+1], 89h
    mov byte ptr [rax+2], 0c8h
    mov byte ptr [rax+3], 0c3h
@err:
    add rsp, 32
    pop rbp
    ret
Core_AssembleBuffer endp

Core_PopulateExplorer proc
    push rbp
    mov rbp, rsp
    sub rsp, 48
    mov rcx, hExplorer
    mov rdx, TVM_DELETEALLITEMS
    xor r8, r8
    xor r9, r9
    call SendMessageA
    lea rcx, rootPath
    lea rdx, findData
    call FindFirstFileA
    mov r12, rax
    inc rax
    jz @done
    dec rax
@next:
    lea r8, findData + 44
    cmp byte ptr [r8], '.'
    je @skip
    lea r13, tvInsertStruct
    mov qword ptr [r13+48], r8
    mov rcx, hExplorer
    mov rdx, TVM_INSERTITEMA
    xor r8, r8
    mov r9, r13
    call SendMessageA
@skip:
    mov rcx, r12
    lea rdx, findData
    call FindNextFileA
    test rax, rax
    jnz @next
    mov rcx, r12
    call FindClose
@done:
    add rsp, 48
    pop rbp
    ret
Core_PopulateExplorer endp

MainWndProc proc
    sub rsp, 48
    cmp edx, 2
    je @dest
    cmp edx, 5
    je @size
    call DefWindowProcA
    jmp @ret
@size:
    push rbx
    push r12
    push r13
    push r14
    push r15
    movzx r12, r8w
    mov r13, r8
    shr r13, 16
    mov r14, 250
    mov r15, 300
    mov rax, r12
    sub rax, r14
    sub rax, r15
    mov rbx, rax
    mov rcx, hExplorer
    xor rdx, rdx
    xor r8, r8
    mov r9, r14
    mov qword ptr [rsp+32+40], r13
    mov qword ptr [rsp+40+40], 1
    call MoveWindow
    mov rcx, hEditor
    mov rdx, r14
    xor r8, r8
    mov r9, rbx
    mov qword ptr [rsp+32+40], r13
    mov qword ptr [rsp+40+40], 1
    call MoveWindow
    mov rcx, hChat
    mov rdx, r12
    sub rdx, r15
    xor r8, r8
    mov r9, r15
    mov qword ptr [rsp+32+40], r13
    mov qword ptr [rsp+40+40], 1
    call MoveWindow
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    xor rax, rax
    jmp @ret
@dest:
    xor ecx, ecx
    call PostQuitMessage
    xor rax, rax
@ret:
    add rsp, 48
    ret
MainWndProc endp

Core_SpawnNativeUI proc
    push rbp
    mov rbp, rsp
    sub rsp, 256
    lea rcx, richEditDll
    call LoadLibraryA
    mov dword ptr [rsp+160], 8
    mov dword ptr [rsp+164], 2
    lea rcx, [rsp+160]
    call InitCommonControlsEx
    xor rcx, rcx
    call GetModuleHandleA
    mov r12, rax
    xor rcx, rcx
    mov rdx, 32512
    call LoadCursorA
    mov r13, rax
    mov dword ptr [rsp+32], 80
    mov dword ptr [rsp+36], 3
    lea rax, MainWndProc
    mov qword ptr [rsp+40], rax
    mov dword ptr [rsp+48], 0
    mov dword ptr [rsp+52], 0
    mov qword ptr [rsp+56], r12
    mov qword ptr [rsp+64], 0
    mov qword ptr [rsp+72], r13
    mov qword ptr [rsp+80], 6
    mov qword ptr [rsp+88], 0
    lea rax, className
    mov qword ptr [rsp+96], rax
    mov dword ptr [rsp+104], 0
    lea rcx, [rsp+32]
    call RegisterClassExA
    xor rcx, rcx
    lea rdx, className
    lea r8, windowTitle
    mov r9d, 0CF0000h
    mov dword ptr [rsp+32], 100
    mov dword ptr [rsp+40], 100
    mov dword ptr [rsp+48], 1200
    mov dword ptr [rsp+56], 800
    mov qword ptr [rsp+64], 0
    mov qword ptr [rsp+72], 0
    mov qword ptr [rsp+80], r12
    mov qword ptr [rsp+88], 0
    call CreateWindowExA
    mov r14, rax
    xor rcx, rcx
    lea rdx, treeClass
    xor r8, r8
    mov r9d, 50000000h
    or r9d, 00800000h
    mov dword ptr [rsp+32], 0
    mov dword ptr [rsp+40], 0
    mov dword ptr [rsp+48], 250
    mov dword ptr [rsp+56], 800
    mov qword ptr [rsp+64], r14
    mov qword ptr [rsp+72], 101
    mov qword ptr [rsp+80], r12
    mov qword ptr [rsp+88], 0
    call CreateWindowExA
    mov hExplorer, rax
    call Core_PopulateExplorer
    xor rcx, rcx
    lea rdx, richEditClass
    xor r8, r8
    mov r9d, 50000000h
    or r9d, 00a00004h
    mov dword ptr [rsp+32], 250
    mov dword ptr [rsp+40], 0
    mov dword ptr [rsp+48], 650
    mov dword ptr [rsp+56], 800
    mov qword ptr [rsp+64], r14
    mov qword ptr [rsp+72], 102
    mov qword ptr [rsp+80], r12
    mov qword ptr [rsp+88], 0
    call CreateWindowExA
    mov hEditor, rax
    xor rcx, rcx
    lea rdx, editClass
    xor r8, r8
    mov r9d, 50000000h
    or r9d, 00800004h
    mov dword ptr [rsp+32], 900
    mov dword ptr [rsp+40], 0
    mov dword ptr [rsp+48], 300
    mov dword ptr [rsp+56], 800
    mov qword ptr [rsp+64], r14
    mov qword ptr [rsp+72], 103
    mov qword ptr [rsp+80], r12
    mov qword ptr [rsp+88], 0
    call CreateWindowExA
    mov hChat, rax
    mov rcx, r14
    mov rdx, 1
    call ShowWindow
@msg:
    lea rcx, [rsp+32]
    xor rdx, rdx
    xor r8, r8
    xor r9, r9
    call GetMessageA
    test rax, rax
    jz @done
    lea rcx, [rsp+32]
    call TranslateMessage
    lea rcx, [rsp+32]
    call DispatchMessageA
    jmp @msg
@done:
    add rsp, 256
    pop rbp
    ret
Core_SpawnNativeUI endp
End
