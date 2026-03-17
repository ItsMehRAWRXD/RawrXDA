; RawrXD Native UI - v15.8.0 (x64 Win32)
; Phase 4: Explorer Data Binding & Native File I/O
; Sovereign IDE Core

PUBLIC MainWndProc

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

; Constants
WM_USER             equ 0400h
EN_CHANGE           equ 0300h
WM_NOTIFY           equ 004Eh
TV_FIRST            equ 1100h
TVM_INSERTITEMA     equ (TV_FIRST + 0)
TVM_DELETEALLITEMS  equ (TV_FIRST + 1)
TVN_FIRST           equ -400
TVN_SELCHANGEDA     equ (TVN_FIRST - 1)
TVM_GETITEMA        equ (TV_FIRST + 12)
TVIF_TEXT           equ 0001h
TVIF_PARAM          equ 0004h

.data
className       db "RawrXD_Native_UI", 0
windowTitle     db "RawrXD v15.8.0 - SOVEREIGN IDE (Native x64)", 0
treeClass       db "SysTreeView32", 0
richEditDll     db "Msftedit.dll", 0
richEditClass   db "RichEdit50W", 0
editClass       db "EDIT", 0
statusClass     db "msctls_statusbar32", 0
rootPath        db "D:\*.*", 0
filePathPrefix  db "D:\", 0
tempFilePath    db 260 dup(0), 0

; Global state
PUBLIC hExplorer
PUBLIC hEditor
PUBLIC hChat
PUBLIC hStatus
PUBLIC hAccel
hExplorer       dq 0
hEditor         dq 0
hChat           dq 0
hStatus         dq 0
hAccel          dq 0

extrn Core_LoadFileIntoEditor : proc

; TVINSERTSTRUCT placeholder structure (Simplified)
align 8
tvInsertStruct:
    dq 0            ; hParent (NULL for root)
    dq 0            ; hInsertAfter (TVI_LAST)
    dd TVIF_TEXT    ; mask (TVIF_TEXT)
    dd 0            ; padding
    dq 0            ; hItem
    dd 0            ; state
    dd 0            ; stateMask
    dq 0            ; pszText
    dd 0            ; cchTextMax
    dd 0            ; iImage
    dd 0            ; iSelectedImage
    dd 0            ; cChildren
    dq 0            ; lParam

; TVITEMA for getting selected text
align 8
tvItem:
    dd TVIF_TEXT    ; mask
    dd 0            ; padding
    dq 0            ; hItem
    dd 0            ; state
    dd 0            ; stateMask
    dq 0            ; pszText
    dd 260          ; cchTextMax
    dd 0            ; iImage
    dd 0            ; iSelectedImage
    dd 0            ; cChildren
    dq 0            ; lParam
itemTextBuffer db 260 dup(0)

; NMTREEVIEWA structure (Simplified for TVN_SELCHANGED)
; NMHDR: hwndFrom(8), idFrom(8), code(4)
; action(4), itemOld(TVITEM), itemNew(TVITEM), ptDrag(POINT)
; We only care about code and itemNew.hItem

; WIN32_FIND_DATAA structure
align 8
findData:
    dd 0            ; dwFileAttributes
    dq 0            ; ftCreationTime
    dq 0            ; ftLastAccessTime
    dq 0            ; ftLastWriteTime
    dd 0            ; nFileSizeHigh
    dd 0            ; nFileSizeLow
    dd 0            ; dwReserved0
    dd 0            ; dwReserved1
    db 260 dup(0)   ; cFileName
    db 14 dup(0)    ; cAlternateFileName

.code

;----------------------------------------------------------------------------
; Core_PopulateExplorer: Native Directory Scanner
;----------------------------------------------------------------------------
Core_PopulateExplorer proc
    push rbp
    mov rbp, rsp
    sub rsp, 40

    ; 1. Clear current items
    mov rcx, hExplorer
    mov rdx, TVM_DELETEALLITEMS
    xor r8, r8
    xor r9, r9
    call SendMessageA

    ; 2. Start FindFirstFile
    lea rcx, rootPath
    lea rdx, findData
    call FindFirstFileA
    mov r12, rax                    ; Handle

    cmp rax, -1
    je @exit_find

@next_file:
    ; Skip "." and ".."
    lea rax, findData + 44          ; Offset to cFileName
    cmp byte ptr [rax], '.'
    je @skip_it

    ; 3. Insert into TreeView
    lea r8, tvInsertStruct
    mov dword ptr [r8+16], 1        ; mask = TVIF_TEXT
    lea rax, findData + 44
    mov qword ptr [r8+48], rax      ; pszText
    
    mov rcx, hExplorer
    mov rdx, TVM_INSERTITEMA
    xor r8, r8
    lea r9, tvInsertStruct
    call SendMessageA

@skip_it:
    mov rcx, r12
    lea rdx, findData
    call FindNextFileA
    test rax, rax
    jnz @next_file

    mov rcx, r12
    call FindClose

@exit_find:
    add rsp, 40
    pop rbp
    ret
Core_PopulateExplorer endp

MWP_FIX PROC
    sub rsp, 48                     
    cmp edx, 2                      ; WM_DESTROY
    je @destroy
    cmp edx, 5                      ; WM_SIZE
    je @resize
    cmp edx, 111h                   ; WM_COMMAND
    je @command
    cmp edx, 004Eh                   ; WM_NOTIFY
    je @notify
    call DefWindowProcA
    jmp @exit

@notify:
    ; r9 = NMHDR*
    mov eax, dword ptr [r9+16]      ; code
    cmp eax, TVN_SELCHANGEDA
    jne @exit_notify
    
    ; Selected item is in r9+32+24 (hItem of itemNew)
    mov rax, qword ptr [r9+56]      ; NMHDR(24)+action(8? no action is 4)+itemOld(TVITEM - 48?)
    ; Wait, NMTREEVIEW has NMHDR(24), UINT action(4), TVITEM itemOld(56 - x64), TVITEM itemNew(56 - x64)
    ; Standard NMHDR for x64 is 24 bytes (HWND 8, UINT_PTR 8, UINT 8)
    ; NMHDR: hwndFrom(8), idFrom(8), code(4), padding(4)
    ; NMTREEVIEW: nmhdr, action, itemOld, itemNew
    ; Offset of itemNew in NMTREEVIEW: NMHDR(24) + action(4) + padding(4) + itemOld(TVITEM)
    ; TVITEM x64 is 56 bytes.
    ; So itemNew starts at 24+4+4+56 = 88.
    ; hItem in itemNew is at offset 8.
    mov rax, qword ptr [r9+96]      ; hItem of itemNew
    test rax, rax
    jz @exit_notify
    
    ; Get item text
    mov rcx, hExplorer
    mov rdx, TVM_GETITEMA
    xor r8, r8
    lea r10, tvItem
    mov qword ptr [r10+16], rax     ; hItem
    lea rax, itemTextBuffer
    mov qword ptr [r10+32], rax     ; pszText
    mov r9, r10
    call SendMessageA
    
    ; Construct path: D:\ + itemTextBuffer
    lea rsi, filePathPrefix
    lea rdi, tempFilePath
@copy_prefix:
    mov al, byte ptr [rsi]
    mov byte ptr [rdi], al
    inc rsi
    inc rdi
    test al, al
    jnz @copy_prefix
    dec rdi                         ; Back to \0
    
    lea rsi, itemTextBuffer
@copy_name:
    mov al, byte ptr [rsi]
    mov byte ptr [rdi], al
    inc rsi
    inc rdi
    test al, al
    jnz @copy_name
    
    ; Call C++ bridge to load file
    lea rcx, tempFilePath
    call Core_LoadFileIntoEditor

@exit_notify:
    xor rax, rax
    jmp @exit

@command:
    cmp r8w, 1003                   ; IDM_EXIT
    je @do_exit
    jmp @next_cmd
@do_exit:
    xor ecx, ecx
    call PostQuitMessage
@next_cmd:
    xor rax, rax
    jmp @exit

; --- Telemetry Surface ---
DrawTelemetryFrame PROC
    push rbp
    mov rbp, rsp
    sub rsp, 256
    
    ; Setup fake telemetry strings for testing
    lea rcx, [rsp+32]
    lea rdx, szGPUTelemetry
    call OutputDebugStringA
    
    add rsp, 256
    pop rbp
    ret
DrawTelemetryFrame ENDP

.data
szGPUTelemetry  db "[TELEMETRY] NVMe: 3.2GB/s | GPU: 42% | TITAN: 9.8 t/s", 13, 10, 0
szNavTitle      db "STRUCTURE", 0
szWorkTitle     db "REPRESENTATION", 0
szInspectTitle  db "INSPECTION", 0
szTimeTitle     db "TIME (TELEMETRY)", 0

@resize:
    push rbx
    push r12
    push r13
    push r14
    push r15
    movzx r12, r8w           ; r12 = Total Width
    mov r13, r8
    shr r13, 16              ; r13 = Total Height
    
    ; Layout Constants: 5-Surface Architecture
    ; [LEFT_RAIL: 200] [CENTER_WORK: DYNAMIC] [RIGHT_INSPECT: 350]
    ; [BOTTOM_TIME: 180]

    mov r14, 200             ; Navigation Width
    mov r15, 350             ; Inspector Width
    
    mov rax, r12
    sub rax, r14
    sub rax, r15
    mov rbx, rax             ; Editor Width
    
    mov rsi, r13
    sub rsi, 180             ; Content Height (Substract Bottom Pane)
    
    ; 1. STRUCTURE (Explorer)
    mov rcx, hExplorer
    test rcx, rcx
    jz @skip_e
    xor rdx, rdx
    xor r8, r8
    mov r9, r14
    push 1
    push rsi
    sub rsp, 20h
    call MoveWindow
    add rsp, 30h
@skip_e:

    ; 2. REPRESENTATION (Editor)
    mov rcx, hEditor
    test rcx, rcx
    jz @skip_ed
    mov rdx, r14
    xor r8, r8
    mov r9, rbx
    push 1
    push rsi
    sub rsp, 20h
    call MoveWindow
    add rsp, 30h
@skip_ed:

    ; 3. INSPECTION (Agent/Inspector)
    mov rcx, hChat
    test rcx, rcx
    jz @skip_c
    mov rdx, r14
    add rdx, rbx
    xor r8, r8
    mov r9, r15
    push 1
    push rsi
    sub rsp, 20h
    call MoveWindow
    add rsp, 30h
@skip_c:

    ; 4. TIME (Telemetry - Bottom)
    mov rcx, hStatus
    test rcx, rcx
    jz @no_status_fix
    xor rdx, rdx
    mov r8, rsi
    mov r9, r12
    push 1
    push 180
    sub rsp, 20h
    call MoveWindow
    add rsp, 30h
    call DrawTelemetryFrame
@no_status_fix:
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    xor rax, rax
    jmp @exit

@destroy:
    xor ecx, ecx
    call PostQuitMessage
    xor rax, rax
@exit:
    add rsp, 48
    ret
MWP_FIX ENDP

DrawTelemetryFrame PROC
    push rbp
    mov rbp, rsp
    sub rsp, 32
    ; Simplified for test
    add rsp, 32
    pop rbp
    ret
DrawTelemetryFrame ENDP

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
