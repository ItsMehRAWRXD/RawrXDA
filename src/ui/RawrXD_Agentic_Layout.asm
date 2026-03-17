; RawrXD Native Core - v16.1.0-AGENTIC-LAYOUT
; Pivoting from Classic 3-Pane to 4-Surface Agentic Architecture
; Surface 1: Explorer (Left) | Surface 2: Editor (Center) | Surface 3: AI Analyzer (Right) | Surface 4: Telemetry (Bottom)

extrn CreateThread : proc
extrn ExitThread : proc
extrn CloseHandle : proc
extrn GetModuleHandleA : proc
extrn RegisterClassExA : proc
extrn CreateWindowExA : proc
extrn ShowWindow : proc
extrn GetMessageA : proc
extrn TranslateMessage : proc
extrn DispatchMessageA : proc
extrn DefWindowProcA : proc
extrn MoveWindow : proc
extrn GetClientRect : proc

.data
className       db "RawrXD_Agentic_UI", 0
windowTitle     db "RawrXD v16.1.0 - SOVEREIGN AGENTIC IDE (4-SURFACE)", 0
treeClass       db "SysTreeView32", 0
richEditClass   db "RichEdit50W", 0
editClass       db "EDIT", 0
statusClass     db "msctls_statusbar32", 0

; Handles
hMain           dq 0
hExplorer       dq 0 ; Surface 1 (Left)
hEditor         dq 0 ; Surface 2 (Center)
hAnalyzer       dq 0 ; Surface 3 (Right)
hTelemetry      dq 0 ; Surface 4 (Bottom)

; Layout Offsets
leftWidth       dq 200
rightWidth      dq 350
bottomHeight    dq 150

.code

MainWndProc proc
    push rbp
    mov rbp, rsp
    sub rsp, 64
    cmp edx, 5 ; WM_SIZE
    je @resize
    cmp edx, 2 ; WM_DESTROY
    je @destroy
    call DefWindowProcA
    jmp @ret

@resize:
    lea rdx, [rbp-32] ; RECT
    mov rcx, hMain
    call GetClientRect
    mov r12d, dword ptr [rbp-24] ; Total Width
    mov r13d, dword ptr [rbp-20] ; Total Height

    ; 1. Explorer (Left)
    mov rcx, hExplorer
    xor rdx, rdx
    xor r8, r8
    mov r9, leftWidth
    mov rax, r13
    sub rax, bottomHeight
    mov qword ptr [rsp+32], rax
    mov dword ptr [rsp+40], 1
    call MoveWindow

    ; 2. Analyzer (Right)
    mov rcx, hAnalyzer
    mov rdx, r12
    sub rdx, rightWidth
    xor r8, r8
    mov r9, rightWidth
    mov rax, r13
    sub rax, bottomHeight
    mov qword ptr [rsp+32], rax
    mov dword ptr [rsp+40], 1
    call MoveWindow

    ; 3. Editor (Center)
    mov rcx, hEditor
    mov rdx, leftWidth
    xor r8, r8
    mov rax, r12
    sub rax, leftWidth
    sub rax, rightWidth
    mov r9, rax ; Remaining middle width
    mov rax, r13
    sub rax, bottomHeight
    mov qword ptr [rsp+32], rax
    mov dword ptr [rsp+40], 1
    call MoveWindow

    ; 4. Telemetry (Bottom)
    mov rcx, hTelemetry
    xor rdx, rdx
    mov r8, r13
    sub r8, bottomHeight
    mov r9, r12
    mov qword ptr [rsp+32], bottomHeight
    mov dword ptr [rsp+40], 1
    call MoveWindow
    
    xor rax, rax
    jmp @ret

@destroy:
    xor ecx, ecx
    call PostQuitMessage
    xor rax, rax
@ret:
    add rsp, 64
    pop rbp
    ret
MainWndProc endp

Core_UIThreadWorker proc
    sub rsp, 256
    xor rcx, rcx
    call GetModuleHandleA
    mov rbx, rax

    ; Register & Create Main
    ; ... (Simplified for transition)

    ; Surface 1 (Explorer)
    ; Surface 2 (Editor)
    ; Surface 3 (Analyzer) - The Agent's Space
    ; Surface 4 (Telemetry) - The Hardware's Space
    
    ; Mock initialization of handles
    ; hExplorer = ...
    ; hEditor = ...
    ; hAnalyzer = ...
    ; hTelemetry = ...

    xor ecx, ecx
    call ExitThread
    ret
Core_UIThreadWorker endp

Core_SpawnNativeUI proc
    sub rsp, 48
    xor rcx, rcx
    xor rdx, rdx
    lea r8, Core_UIThreadWorker
    xor r9, r9
    call CreateThread
    mov rax, 1
    add rsp, 48
    ret
Core_SpawnNativeUI endp

End
