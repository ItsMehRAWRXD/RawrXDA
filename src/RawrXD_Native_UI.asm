; RawrXD Native UI - v15.8.0 (x64 Win32)
; Sovereign IDE Core

PUBLIC MainWndProc
PUBLIC Core_PopulateExplorer
PUBLIC Core_SpawnNativeUI

extrn DefWindowProcA : proc
extrn PostQuitMessage : proc
extrn LoadLibraryA : proc
extrn InitCommonControlsEx : proc
extrn GetModuleHandleA : proc
extrn LoadCursorA : proc
extrn RegisterClassExA : proc
extrn CreateWindowExA : proc
extrn SendMessageA : proc
extrn FindFirstFileA : proc
extrn FindNextFileA : proc
extrn FindClose : proc
extrn MoveWindow : proc
extrn ShowWindow : proc
extrn UpdateWindow : proc
extrn SetTimer : proc
extrn wsprintfA : proc
extrn Titan_GetTelemetryData : proc
extrn Core_LoadFileIntoEditor : proc
extrn Core_HandleExplorerSelection : proc

.data
className     db "RawrXD_NativeUI", 0
windowTitle   db "RawrXD v15.8.0 [Titan Stage 10]", 0
treeClass     db "SysTreeView32", 0
richEditDll   db "Msftedit.dll", 0
richEditClass db "RichEdit50W", 0
editClass     db "EDIT", 0
statusClass   db "msctls_statusbar32", 0
rootPath      db "D:\*.*", 0
telemetryFmt  db "INF: %d ms | IO: %d KB/s | RAM: %d MB | RAW: TITAN-S10", 0

hExplorer     dq 0
hEditor       dq 0
hChat         dq 0
hStatus       dq 0
telemetryBuf  db 256 dup(0)
telemetryData dq 4 dup(0) ; [Inf, Bytes, Mem]

PUBLIC hExplorer
PUBLIC hEditor
PUBLIC hChat
PUBLIC hStatus

.code

MainWndProc proc
    sub rsp, 40
    cmp edx, 0005h ; WM_SIZE
    je @resize
    cmp edx, 004Eh ; WM_NOTIFY
    je @notify
    cmp edx, 0113h ; WM_TIMER
    je @timer
    cmp edx, 2 ; WM_DESTROY
    je @destroy
    call DefWindowProcA
    add rsp, 40
    ret
@notify:
    ; Handle TreeView Selection
    mov rax, r9 ; NMHDR pointer
    mov eax, [rax+8] ; code field
    cmp eax, -402 ; TVN_SELCHANGEDA (roughly)
    jne @no_sel
    
    ; Invoke C++ handler for selection
    mov rcx, r9
    call Core_HandleExplorerSelection
@no_sel:
    xor rax, rax
    add rsp, 40
    ret
@timer:
    ; Update Telemetry Pane
    lea rcx, telemetryData
    call Titan_GetTelemetryData
    
    lea rcx, telemetryBuf
    lea rdx, telemetryFmt
    mov r8, qword ptr [telemetryData] ; Inf
    mov r9, qword ptr [telemetryData+8] ; Bytes
    shr r9, 10 ; to KB
    mov rax, qword ptr [telemetryData+16] ; Mem
    mov [rsp+32], rax
    sub rsp, 48 ; Additional space for wsprintfA if needed
    call wsprintfA
    add rsp, 48
    mov rdx, 000Ch ; WM_SETTEXT
    xor r8, r8
    lea r9, telemetryBuf
    call SendMessageA
    
    xor rax, rax
    add rsp, 40
    ret
@resize:
    ; 5-Surface Sovereign Layout Engine
    ; r8 = Width (Low Word), r9 = Height (High Word)
    mov r12, r8
    and r12, 0FFFFh ; width
    mov r13, r9
    and r13, 0FFFFh ; height

    ; 1. Structure (Left Nav) - 200px
    mov rcx, hExplorer
    mov rdx, 0
    mov r8, 0
    mov r9, 200
    mov [rsp+32], r13 ; full height
    mov dword ptr [rsp+40], 1
    call MoveWindow

    ; 2. Representation (Center Editor) - Fill
    mov r14, r12
    sub r14, 500 ; minus 200 left, 300 right
    mov rcx, hEditor
    mov rdx, 200
    mov r8, 0
    mov r9, r14
    mov rax, r13
    sub rax, 150 ; minus footer
    mov [rsp+32], rax
    mov dword ptr [rsp+40], 1
    call MoveWindow

    ; 3. Inspection (Right Agent) - 300px
    mov r15, r12
    sub r15, 300
    mov rcx, hChat
    mov rdx, r15
    mov r8, 0
    mov r9, 300
    mov rax, r13
    sub rax, 150 ; minus footer
    mov [rsp+32], rax
    mov dword ptr [rsp+40], 1
    call MoveWindow

    ; 4. Time (Bottom Telemetry) - 150px
    mov rsi, r13
    sub rsi, 150
    mov rcx, hStatus
    mov rdx, 200
    mov r8, rsi
    mov r9, r12
    sub r9, 200 ; align with split
    mov dword ptr [rsp+32], 150
    mov dword ptr [rsp+40], 1
    call MoveWindow

    xor rax, rax
    add rsp, 40
    ret
@destroy:
    xor ecx, ecx
    call PostQuitMessage
    xor rax, rax
    add rsp, 40
    ret
MainWndProc endp

Core_PopulateExplorer proc
    ret
Core_PopulateExplorer endp

Core_SpawnNativeUI proc
    push rbp
    mov rbp, rsp
    sub rsp, 256
    lea rcx, richEditDll
    call LoadLibraryA
    xor rcx, rcx
    call GetModuleHandleA
    mov r12, rax
    
    ; Register Class
    ; WNDCLASSEXA struct on stack... omitting full registration for speed, using vanilla
    
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
    mov r14, rax ; hWnd

    ; 1. Explorer (Structure)
    xor rcx, rcx
    lea rdx, treeClass
    xor r8, r8
    mov r9d, 50000000h ; WS_CHILD | WS_VISIBLE
    mov dword ptr [rsp+32], 0
    mov dword ptr [rsp+40], 0
    mov dword ptr [rsp+48], 200
    mov dword ptr [rsp+56], 800
    mov qword ptr [rsp+64], r14
    mov qword ptr [rsp+72], 101 ; ID
    mov qword ptr [rsp+80], r12
    mov qword ptr [rsp+88], 0
    call CreateWindowExA
    mov hExplorer, rax

    ; 2. Editor (Representation)
    xor rcx, rcx
    lea rdx, richEditClass
    xor r8, r8
    mov r9d, 50B00000h ; WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE
    mov dword ptr [rsp+32], 200
    mov dword ptr [rsp+40], 0
    mov dword ptr [rsp+48], 700
    mov dword ptr [rsp+56], 650
    mov qword ptr [rsp+64], r14
    mov qword ptr [rsp+72], 102
    mov qword ptr [rsp+80], r12
    mov qword ptr [rsp+88], 0
    call CreateWindowExA
    mov hEditor, rax

    ; 3. Chat (Inspection)
    xor rcx, rcx
    lea rdx, richEditClass
    xor r8, r8
    mov r9d, 50B00000h
    mov dword ptr [rsp+32], 900
    mov dword ptr [rsp+40], 0
    mov dword ptr [rsp+48], 300
    mov dword ptr [rsp+56], 650
    mov qword ptr [rsp+64], r14
    mov qword ptr [rsp+72], 103
    mov qword ptr [rsp+80], r12
    mov qword ptr [rsp+88], 0
    call CreateWindowExA
    mov hChat, rax

    ; 4. Telemetry (Time)
    xor rcx, rcx
    lea rdx, editClass
    xor r8, r8
    mov r9d, 50800800h ; WS_CHILD | WS_VISIBLE | ES_READONLY
    mov dword ptr [rsp+32], 200
    mov dword ptr [rsp+40], 650
    mov dword ptr [rsp+48], 1000
    mov dword ptr [rsp+56], 150
    mov qword ptr [rsp+64], r14
    mov qword ptr [rsp+72], 104
    mov qword ptr [rsp+80], r12
    mov qword ptr [rsp+88], 0
    call CreateWindowExA
    mov hStatus, rax

    ; Set Telemetry Timer (100ms)
    mov rcx, r14 ; hWnd
    mov rdx, 1 ; ID
    mov r8, 100 ; ms
    xor r9, r9
    call SetTimer

    mov rcx, r14
    mov rdx, 1
    call ShowWindow
    add rsp, 256
    pop rbp
    ret
Core_SpawnNativeUI endp

END
