; RawrXD Native UI - v15.8.0 (x64 Win32)
; Phase 4: Explorer Data Binding & Native File I/O
; Sovereign IDE Core

PUBLIC MainWndProc proc
    ret
MainWndProc endp

    push rbp
    mov rbp, rsp
    sub rsp, 32
    ; Simplified for test
    add rsp, 32
    pop rbp
    ret

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
    mov dword ptr [rsp+52], 512     ; CS_HREDRAW | CS_VREDRAW
    mov qword ptr [rsp+56], r12
    mov qword ptr [rsp+72], r13
    mov qword ptr [rsp+80], 6       
    lea rax, className
    mov qword ptr [rsp+96], rax
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
    
    ; Create Explorer (TreeView)
    xor rcx, rcx
    lea rdx, treeClass
    xor r8, r8
    mov r9d, 50000000h              
    or r9d, 00800000h               
    mov dword ptr [rsp+32], 0
    mov dword ptr [rsp+40], 0
    mov dword ptr [rsp+48], 250
    mov dword ptr [rsp+56], 750     
    mov qword ptr [rsp+64], r14
    mov qword ptr [rsp+72], 101
    mov qword ptr [rsp+80], r12
    mov qword ptr [rsp+88], 0
    call CreateWindowExA
    mov hExplorer, rax

    ; Populate Explorer
    call Core_PopulateExplorer

    ; Create Editor (RichEdit)
    xor rcx, rcx
    lea rdx, richEditClass
    xor r8, r8
    mov r9d, 50000000h              
    or r9d, 00800000h               
    or r9d, 00200000h               
    or r9d, 00000004h               
    mov dword ptr [rsp+32], 250
    mov dword ptr [rsp+40], 0
    mov dword ptr [rsp+48], 650
    mov dword ptr [rsp+56], 750
    mov qword ptr [rsp+64], r14
    mov qword ptr [rsp+72], 102
    mov qword ptr [rsp+80], r12
    mov qword ptr [rsp+88], 0
    call CreateWindowExA
    mov hEditor, rax
    
    ; Create Chat (Edit)
    xor rcx, rcx
    lea rdx, editClass
    xor r8, r8
    mov r9d, 50000000h
    or r9d, 00800000h
    or r9d, 00000004h
    mov dword ptr [rsp+32], 900
    mov dword ptr [rsp+40], 0
    mov dword ptr [rsp+48], 300
    mov dword ptr [rsp+56], 750
    mov qword ptr [rsp+64], r14
    mov qword ptr [rsp+72], 103
    mov qword ptr [rsp+80], r12
    mov qword ptr [rsp+88], 0
    call CreateWindowExA
    mov hChat, rax

    mov rcx, r14
    mov rdx, 1
    call ShowWindow
    mov rcx, r14
    call UpdateWindow
@msg_loop:
    lea rcx, [rsp+32]
    xor rdx, rdx
    xor r8, r8
    xor r9, r9
    call GetMessageA
    test rax, rax
    jz @exit_loop
    lea rcx, [rsp+32]
    call TranslateMessage
    lea rcx, [rsp+32]
    call DispatchMessageA
    jmp @msg_loop
@exit_loop:
    add rsp, 256
    pop rbp
    ret
Core_SpawnNativeUI endp

End

