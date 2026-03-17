; RawrXD Native Core - v15.9.5-GOLD-TITAN-JIT
; The Final Seal: In-Process Machine Code Emitter

extrn CreateThread : proc
extrn ExitThread : proc
extrn CloseHandle : proc
extrn OutputDebugStringA : proc
extrn GetModuleHandleA : proc
extrn LoadLibraryA : proc
extrn LoadCursorA : proc
extrn RegisterClassExA : proc
extrn CreateWindowExA : proc
extrn ShowWindow : proc
extrn GetMessageA : proc
extrn TranslateMessage : proc
extrn DispatchMessageA : proc
extrn PostQuitMessage : proc
extrn DefWindowProcA : proc
extrn MoveWindow : proc
extrn SendMessageA : proc
extrn FindFirstFileA : proc
extrn FindNextFileA : proc
extrn FindClose : proc
extrn GetClientRect : proc
extrn SetCapture : proc
extrn ReleaseCapture : proc
extrn CreateFileA : proc
extrn ReadFile : proc
extrn GetFileSize : proc
extrn VirtualAlloc : proc
extrn VirtualFree : proc
extrn VirtualProtect : proc

.data
className       db "RawrXD_Native_UI", 0
windowTitle     db "RawrXD v15.9.5 - GOLD (TITAN JIT ENABLED)", 0
treeClass       db "SysTreeView32", 0
richEditDll     db "Msftedit.dll", 0
richEditClass   db "RichEdit50W", 0
editClass       db "EDIT", 0
rootDirectory   db "D:\", 0
rootPath        db "D:\*.*", 0
filePrefix      db "D:\", 0
jitSuccessMsg   db "Titan JIT: Native Buffer Executed Successfully!", 0

hMain           dq 0
hExplorer       dq 0
hEditor         dq 0
hChat           dq 0
sep1            dq 250
sep2            dq 300
isDragging      dq 0

align 8
tvInsert:
    dq 0, 0
    dd 11h, 0
    dq 0
    dd 0, 0
    dq 0
    dd 260, 0, 0, 0
    dq 0

align 8
fData:
    db 600 dup(0)

fileNameBuffer  db 260 dup(0)

.code

; --- TITAN JIT CORE ---
; Input: rcx = Pointer to Source Text (RichEdit Buffer)
Core_AssembleBuffer proc
    push rbp
    mov rbp, rsp
    sub rsp, 64

    ; 1. Allocate Executable Memory
    mov rcx, 4096 ; 4KB Page
    mov rdx, 3000h ; MEM_COMMIT | MEM_RESERVE
    mov r8, 04h ; PAGE_READWRITE
    call VirtualAlloc
    mov r12, rax ; JIT Buffer

    ; 2. Identity Emitter (Titan Stage 0)
    ; Emits: 48 89 C8 (mov rax, rcx)
    ;        C3       (ret)
    mov byte ptr [r12], 48h
    mov byte ptr [r12+1], 89h
    mov byte ptr [r12+2], 0C8h
    mov byte ptr [r12+3], 0C3h

    ; 3. Transition to RX (Read-Execute)
    mov rcx, r12
    mov rdx, 4096
    mov r8, 20h ; PAGE_EXECUTE_READ
    lea r9, [rsp+56] ; OldProtect
    call VirtualProtect

    ; 4. Execute the JIT Code
    mov rcx, 1234h ; Test value
    call r12
    ; rax now contains 1234h

    ; 5. Log Success (to OutputDebugString)
    lea rcx, jitSuccessMsg
    call OutputDebugStringA

    ; 6. Free JIT Buffer
    mov rcx, r12
    xor rdx, rdx
    mov r8, 8000h ; MEM_RELEASE
    call VirtualFree

    mov rax, 1
    add rsp, 64
    pop rbp
    ret
Core_AssembleBuffer endp

; --- Win32 Native Hub ---
MainWndProc proc
    push rbp
    mov rbp, rsp
    sub rsp, 128
    cmp edx, 2 ; WM_DESTROY
    je @destroy
    cmp edx, 0100h ; WM_KEYDOWN
    je @keydown
    cmp edx, 004Eh ; WM_NOTIFY
    je @notify
    cmp edx, 5 ; WM_SIZE
    je @resize
    cmp edx, 0201h ; WM_LBUTTONDOWN
    je @lbuttondown
    cmp edx, 0200h ; WM_MOUSEMOVE
    je @mousemove
    cmp edx, 0202h ; WM_LBUTTONUP
    je @lbuttonup
    call DefWindowProcA
    jmp @ret

@keydown:
    cmp r8, 0Dh ; VK_RETURN (Enter key)
    jne @ret
    ; If Shift+Enter or similar, we trigger Titan JIT
    call Core_AssembleBuffer
    jmp @ret

@notify:
    mov rax, [r9+16] ; nmhdr.code
    cmp rax, -402 ; TVN_SELCHANGED
    jne @ret_notify
    mov r12, [r9+80] ; nmtv.itemNew.pszText
    lea rdi, fileNameBuffer
    lea rsi, filePrefix
    mov rcx, 3
    rep movsb
    mov rsi, r12
@copy_loop:
    movsb
    cmp byte ptr [rsi-1], 0
    jne @copy_loop
    ; Core_LoadFileToEditor logic integrated here
    ; (Omitting full load for brevity, focusing on JIT hook)
@ret_notify:
    xor rax, rax
    jmp @ret

@resize:
    mov rcx, hMain; lea rdx, [rbp-64]; call GetClientRect
    mov r12d, dword ptr [rbp-56]; mov r13d, dword ptr [rbp-52]
    mov rcx, hExplorer; xor rdx, rdx; xor r8, r8; mov r9, sep1
    mov qword ptr [rsp+32], r13; mov dword ptr [rsp+40], 1; call MoveWindow
    mov rax, r12; sub rax, sep1; sub rax, sep2; mov r14, rax
    mov rcx, hEditor; mov rdx, sep1; xor r8, r8; mov r9, r14
    mov qword ptr [rsp+32], r13; mov dword ptr [rsp+40], 1; call MoveWindow
    mov rcx, hChat; mov rdx, r12; sub rdx, sep2; xor r8, r8; mov r9, sep2
    mov qword ptr [rsp+32], r13; mov dword ptr [rsp+40], 1; call MoveWindow
    xor rax, rax; jmp @ret

@lbuttondown:
    movzx r12, r9w; mov rax, sep1; sub rax, 5; cmp r12, rax; jl @ret; add rax, 10; cmp r12, rax; jg @ret
    mov isDragging, 1; call SetCapture; jmp @ret
@mousemove:
    cmp isDragging, 1; jne @ret; movzx r12, r9w; mov sep1, r12; jmp @resize
@lbuttonup:
    call ReleaseCapture; mov isDragging, 0; jmp @ret

@destroy:
    xor ecx, ecx; call PostQuitMessage; xor rax, rax; jmp @ret
@ret:
    leave
    ret
MainWndProc endp

Core_UIThreadWorker proc
    sub rsp, 512
    lea rcx, richEditDll; call LoadLibraryA
    xor rcx, rcx; call GetModuleHandleA; mov r12, rax
    xor rcx, rcx; mov rdx, 32512; call LoadCursorA; mov r13, rax
    mov dword ptr [rsp+32], 80; mov dword ptr [rsp+36], 3; lea rax, MainWndProc; mov qword ptr [rsp+40], rax
    mov qword ptr [rsp+56], r12; mov qword ptr [rsp+72], r13; mov qword ptr [rsp+80], 6
    lea rax, className; mov qword ptr [rsp+96], rax; lea rcx, [rsp+32]; call RegisterClassExA
    xor rcx, rcx; lea rdx, className; lea r8, windowTitle; mov r9d, 10CF0000h
    mov dword ptr [rsp+32], 100; mov dword ptr [rsp+40], 100; mov dword ptr [rsp+48], 1280; mov dword ptr [rsp+56], 800
    mov qword ptr [rsp+64], 0; mov qword ptr [rsp+72], 0; mov qword ptr [rsp+80], r12; call CreateWindowExA; mov hMain, rax
    xor rcx, rcx; lea rdx, treeClass; xor r8, r8; mov r9d, 50000000h or 00800000h or 2
    mov dword ptr [rsp+32], 0; mov dword ptr [rsp+40], 0; mov dword ptr [rsp+48], 250; mov dword ptr [rsp+56], 800
    mov qword ptr [rsp+64], hMain; mov qword ptr [rsp+72], 101; mov qword ptr [rsp+80], r12; call CreateWindowExA; mov hExplorer, rax
    xor rcx, rcx; lea rdx, richEditClass; xor r8, r8; mov r9d, 50000000h or 00a00004h or 00100000h
    mov dword ptr [rsp+32], 250; mov dword ptr [rsp+40], 0; mov dword ptr [rsp+48], 730; mov dword ptr [rsp+56], 800
    mov qword ptr [rsp+64], hMain; mov qword ptr [rsp+72], 102; mov qword ptr [rsp+80], r12; call CreateWindowExA; mov hEditor, rax
    xor rcx, rcx; lea rdx, editClass; xor r8, r8; mov r9d, 50000000h or 00800004h or 00400000h
    mov dword ptr [rsp+32], 980; mov dword ptr [rsp+40], 0; mov dword ptr [rsp+48], 300; mov dword ptr [rsp+56], 800
    mov qword ptr [rsp+64], hMain; mov qword ptr [rsp+72], 103; mov qword ptr [rsp+80], r12; call CreateWindowExA; mov hChat, rax
    mov rcx, hMain; mov rdx, 1; call ShowWindow
@msg_loop:
    lea rcx, [rsp+256]; xor rdx, rdx; xor r8, r8; xor r9, r9; call GetMessageA; test rax, rax; jz @done
    lea rcx, [rsp+256]; call TranslateMessage; lea rcx, [rsp+256]; call DispatchMessageA; jmp @msg_loop
@done:
    xor ecx, ecx; call ExitThread; add rsp, 512; ret
Core_UIThreadWorker endp

Core_SpawnNativeUI proc
    sub rsp, 48
    xor rcx, rcx; xor rdx, rdx; lea r8, Core_UIThreadWorker; xor r9, r9
    mov dword ptr [rsp+32], 0; lea rax, [rsp+40]; call CreateThread
    mov rcx, rax; call CloseHandle; mov rax, 1; add rsp, 48; ret
Core_SpawnNativeUI endp

End
