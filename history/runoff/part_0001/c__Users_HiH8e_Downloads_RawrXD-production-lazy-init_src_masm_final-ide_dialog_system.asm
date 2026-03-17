; ============================================================================
; Dialog System Implementation for RawrXD Pure MASM IDE
; ============================================================================
; Phase 3 Critical Blocker #1 - Blocks 15+ components
; ============================================================================
; Provides modal dialog support for Windows Common Controls
; ============================================================================

include windows.inc
include kernel32.inc
include user32.inc
include comctl32.inc

.DATA

szDialogClass DB "#32770", 0

; ============================================================================
; Dialog System Global Variables
; ============================================================================

dialog_system_initialized    BYTE 0
active_dialog_count          DWORD 0
modal_dialog_stack           QWORD 16 DUP(0)  ; Stack of active modal dialogs

; ============================================================================
; Dialog Entry Structure
; ============================================================================
DIALOG_ENTRY STRUCT
    hwnd            QWORD ?     ; Dialog window handle
    parent_hwnd     QWORD ?     ; Parent window handle
    on_command      QWORD ?     ; Command callback function pointer
    on_init         QWORD ?     ; WM_INITDIALOG callback
    user_data       QWORD ?     ; Custom user data pointer
    result          DWORD ?     ; Dialog result (IDOK, IDCANCEL, etc.)
    is_modal        BYTE ?      ; 1 = modal, 0 = modeless
    padding         BYTE 7 DUP(?)
DIALOG_ENTRY ENDS

; ============================================================================
; Dialog Registry
; ============================================================================
dialog_registry              QWORD 32 DUP(0)  ; Array of DIALOG_ENTRY pointers
max_dialogs                  EQU 32

.CODE

; ============================================================================
; DialogSystemInit: Initialize dialog subsystem
; Parameters:
;   rcx = parent HWND (main window handle)
; Returns:
;   rax = success (0 = fail, 1 = success)
; ============================================================================
DialogSystemInit PROC
    ; Check if already initialized
    cmp [dialog_system_initialized], 1
    je init_already_done
    
    ; Initialize dialog registry
    mov rdi, offset dialog_registry
    mov rcx, max_dialogs
    xor rax, rax
    rep stosq
    
    ; Initialize modal dialog stack
    mov rdi, offset modal_dialog_stack
    mov rcx, 16
    xor rax, rax
    rep stosq
    
    ; Set initialized flag
    mov [dialog_system_initialized], 1
    mov [active_dialog_count], 0
    
init_already_done:
    mov rax, 1  ; Success
    ret
DialogSystemInit ENDP

; ============================================================================
; CreateModalDialog: Create and run modal dialog
; Parameters:
;   rcx = parent HWND
;   rdx = dialog title (string pointer)
;   r8d = dialog width
;   r9d = dialog height
;   stack: [rsp+40] = on_command callback (optional)
;   stack: [rsp+48] = on_init callback (optional)
;   stack: [rsp+56] = user_data (optional)
; Returns:
;   rax = dialog result (IDOK=1, IDCANCEL=2, etc.)
; ============================================================================
CreateModalDialog PROC FRAME
    ; Save non-volatile registers
    push rbx
    push rsi
    push rdi
    .PUSHREG rbx
    .PUSHREG rsi
    .PUSHREG rdi
    
    ; Allocate stack space for local variables
    sub rsp, 40h
    .ALLOCSTACK 40h
    
    .ENDPROLOG
    
    ; Save parameters
    mov [rsp+28h], rcx  ; parent_hwnd
    mov [rsp+30h], rdx  ; title
    mov [rsp+38h], r8d  ; width
    mov [rsp+3Ch], r9d  ; height
    
    ; Check if dialog system is initialized
    cmp [dialog_system_initialized], 1
    jne dialog_system_not_ready
    
    ; Allocate DIALOG_ENTRY structure
    mov rcx, sizeof(DIALOG_ENTRY)
    call malloc
    test rax, rax
    jz allocation_failed
    
    mov rbx, rax  ; rbx = dialog_entry pointer
    
    ; Initialize dialog entry
    mov rcx, [rsp+28h]  ; parent_hwnd
    mov [rbx+DIALOG_ENTRY.parent_hwnd], rcx
    mov [rbx+DIALOG_ENTRY.result], 0
    mov [rbx+DIALOG_ENTRY.is_modal], 1
    
    ; Set callbacks if provided
    mov rcx, [rsp+68h]  ; on_command callback
    test rcx, rcx
    jz no_command_callback
    mov [rbx+DIALOG_ENTRY.on_command], rcx
    
no_command_callback:
    mov rcx, [rsp+70h]  ; on_init callback
    test rcx, rcx
    jz no_init_callback
    mov [rbx+DIALOG_ENTRY.on_init], rcx
    
no_init_callback:
    mov rcx, [rsp+78h]  ; user_data
    test rcx, rcx
    jz no_user_data
    mov [rbx+DIALOG_ENTRY.user_data], rcx
    
no_user_data:
    ; Create dialog window
    mov rcx, [rsp+28h]  ; parent_hwnd
    mov rdx, [rsp+30h]  ; title
    mov r8d, [rsp+38h]  ; width
    mov r9d, [rsp+3Ch]  ; height
    mov r10, rbx        ; dialog_entry for WM_INITDIALOG
    call CreateDialogWindow
    test rax, rax
    jz dialog_creation_failed
    
    mov [rbx+DIALOG_ENTRY.hwnd], rax
    
    ; Add to dialog registry
    call AddDialogToRegistry
    test rax, rax
    jz registry_add_failed
    
    ; Push to modal stack
    mov rcx, rbx
    call PushModalDialog
    
    ; Show dialog
    mov rcx, [rbx+DIALOG_ENTRY.hwnd]
    mov rdx, SW_SHOW
    call ShowWindow
    
    ; Update parent window (disable if modal)
    mov rcx, [rbx+DIALOG_ENTRY.parent_hwnd]
    mov rdx, FALSE
    call EnableWindow
    
    ; Run modal message loop
    mov rcx, rbx
    call RunModalDialogLoop
    
    ; Get result before cleanup
    mov eax, [rbx+DIALOG_ENTRY.result]
    mov [rsp+20h], eax  ; Save result
    
    ; Cleanup dialog
    mov rcx, rbx
    call RemoveDialogFromRegistry
    
    ; Free dialog entry
    mov rcx, rbx
    call free
    
    ; Restore parent window
    mov rcx, [rsp+28h]  ; parent_hwnd
    mov rdx, TRUE
    call EnableWindow
    
    ; Return result
    mov eax, [rsp+20h]
    jmp dialog_complete
    
dialog_system_not_ready:
    xor eax, eax
    jmp dialog_complete
    
allocation_failed:
    xor eax, eax
    jmp dialog_complete
    
dialog_creation_failed:
    mov rcx, rbx
    call free
    xor eax, eax
    jmp dialog_complete
    
registry_add_failed:
    mov rcx, [rbx+DIALOG_ENTRY.hwnd]
    call DestroyWindow
    mov rcx, rbx
    call free
    xor eax, eax
    
dialog_complete:
    ; Cleanup stack and return
    add rsp, 40h
    pop rdi
    pop rsi
    pop rbx
    ret
CreateModalDialog ENDP

; ============================================================================
; CreateDialogWindow: Create dialog window
; Parameters:
;   rcx = parent HWND
;   rdx = title string
;   r8d = width
;   r9d = height
;   r10 = dialog_entry pointer
; Returns:
;   rax = dialog HWND or NULL
; ============================================================================
CreateDialogWindow PROC
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    sub rsp, 80         ; Local variables and shadow space (aligned)

    mov r12, rcx        ; r12 = parent HWND
    mov r13, rdx        ; r13 = title string
    mov r14d, r8d       ; r14d = width
    mov r15, r10        ; r15 = dialog_entry pointer
    mov r10d, r9d       ; r10d = height

    ; Get parent window rect
    lea rdx, [rsp + 40] ; RECT structure
    mov rcx, r12
    call GetWindowRect
    
    ; Calculate center position
    ; RECT: left(0), top(4), right(8), bottom(12)
    mov eax, [rsp + 48] ; parent right
    sub eax, [rsp + 40] ; parent left
    sub eax, r14d       ; dialog width
    shr eax, 1          ; divide by 2
    add eax, [rsp + 40] ; add parent left
    
    mov edx, [rsp + 52] ; parent bottom
    sub edx, [rsp + 44] ; parent top
    sub edx, r10d       ; dialog height
    shr edx, 1          ; divide by 2
    add edx, [rsp + 44] ; add parent top
    
    ; Create dialog window
    ; CreateWindowExA(exStyle, class, title, style, x, y, w, h, parent, menu, inst, param)
    sub rsp, 64         ; 8 stack args (64 bytes)
    
    mov qword ptr [rsp + 20h], rax ; x
    mov qword ptr [rsp + 28h], rdx ; y
    mov r8, r14
    mov qword ptr [rsp + 30h], r8  ; width
    mov r8, r10
    mov qword ptr [rsp + 38h], r8  ; height
    mov qword ptr [rsp + 40h], r12 ; parent_hwnd
    mov qword ptr [rsp + 48h], 0   ; hMenu
    mov qword ptr [rsp + 50h], 0   ; hInstance
    mov qword ptr [rsp + 58h], r15 ; lpParam (dialog_entry)
    
    xor rcx, rcx                   ; dwExStyle
    lea rdx, [szDialogClass]       ; lpClassName
    mov r8, r13                    ; lpWindowName
    mov r9d, WS_POPUP or WS_VISIBLE or WS_CAPTION or WS_SYSMENU ; dwStyle
    call CreateWindowExA
    
    add rsp, 64
    add rsp, 80
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
CreateDialogWindow ENDP
    lea rdx, szDialogClass ; lpClassName
    mov r8, r13         ; lpWindowName (title)
    mov r9, WS_OVERLAPPEDWINDOW or WS_CAPTION or WS_SYSMENU ; dwStyle
    
    call CreateWindowExA
    
    add rsp, 8*8 + 32   ; Clean up stack
    add rsp, 64
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
CreateDialogWindow ENDP

; ============================================================================
; RunModalDialogLoop: Modal dialog message loop
; Parameters:
;   rcx = dialog_entry pointer
; Returns:
;   rax = 1 (success)
; ============================================================================
RunModalDialogLoop PROC
    mov rbx, rcx  ; Save dialog_entry
    
modal_loop:
    ; Get message
    sub rsp, 30h
    lea rcx, [rsp+20h]  ; MSG structure
    mov rdx, 0
    mov r8, 0
    mov r9, 0
    mov r10, PM_REMOVE
    call GetMessageA
    
    test eax, eax
    jz modal_loop_exit  ; WM_QUIT
    
    ; Check if dialog is still valid
    mov rcx, [rbx+DIALOG_ENTRY.hwnd]
    test rcx, rcx
    jz modal_loop_exit
    
    ; Check if message is for this dialog
    mov rcx, [rsp+20h]  ; msg.hwnd
    cmp rcx, [rbx+DIALOG_ENTRY.hwnd]
    jne not_for_dialog
    
    ; Handle dialog-specific messages
    mov rcx, rbx
    lea rdx, [rsp+20h]  ; MSG pointer
    call HandleDialogMessage
    
not_for_dialog:
    ; Translate and dispatch
    lea rcx, [rsp+20h]
    call TranslateMessage
    lea rcx, [rsp+20h]
    call DispatchMessageA
    
    ; Check if dialog should close
    cmp [rbx+DIALOG_ENTRY.result], 0
    je modal_loop
    
modal_loop_exit:
    add rsp, 30h
    mov rax, 1
    ret
RunModalDialogLoop ENDP

; ============================================================================
; HandleDialogMessage: Process dialog-specific messages
; Parameters:
;   rcx = dialog_entry pointer
;   rdx = MSG pointer
; Returns:
;   rax = 1 if handled, 0 if not
; ============================================================================
HandleDialogMessage PROC
    mov rbx, rcx  ; dialog_entry
    mov rsi, rdx  ; MSG pointer
    
    mov eax, [rsi+MSG.message]
    
    ; Handle WM_INITDIALOG
    cmp eax, WM_INITDIALOG
    jne check_wm_command
    
    ; Call on_init callback if provided
    mov rcx, [rbx+DIALOG_ENTRY.on_init]
    test rcx, rcx
    jz init_done
    
    ; Call callback: rcx = hwnd, rdx = dialog_entry
    mov rcx, [rbx+DIALOG_ENTRY.hwnd]
    mov rdx, rbx
    call [rbx+DIALOG_ENTRY.on_init]
    
init_done:
    mov rax, 1
    ret
    
check_wm_command:
    cmp eax, WM_COMMAND
    jne check_wm_close
    
    ; Handle WM_COMMAND
    mov eax, [rsi+MSG.wParam]
    and eax, 0FFFFh  ; LOWORD = control ID
    
    ; Check for OK button
    cmp eax, IDOK
    jne check_cancel
    
    mov [rbx+DIALOG_ENTRY.result], IDOK
    mov rcx, [rbx+DIALOG_ENTRY.hwnd]
    call DestroyWindow
    mov rax, 1
    ret
    
check_cancel:
    cmp eax, IDCANCEL
    jne call_command_callback
    
    mov [rbx+DIALOG_ENTRY.result], IDCANCEL
    mov rcx, [rbx+DIALOG_ENTRY.hwnd]
    call DestroyWindow
    mov rax, 1
    ret
    
call_command_callback:
    ; Call on_command callback if provided
    mov rcx, [rbx+DIALOG_ENTRY.on_command]
    test rcx, rcx
    jz command_done
    
    ; Call callback: rcx = hwnd, rdx = control ID
    mov rcx, [rbx+DIALOG_ENTRY.hwnd]
    mov edx, eax  ; control ID
    call [rbx+DIALOG_ENTRY.on_command]
    
command_done:
    mov rax, 1
    ret
    
check_wm_close:
    cmp eax, WM_CLOSE
    jne not_handled
    
    mov [rbx+DIALOG_ENTRY.result], IDCANCEL
    mov rcx, [rbx+DIALOG_ENTRY.hwnd]
    call DestroyWindow
    mov rax, 1
    ret
    
not_handled:
    xor rax, rax
    ret
HandleDialogMessage ENDP

; ============================================================================
; AddDialogToRegistry: Add dialog to tracking registry
; Parameters:
;   rcx = dialog_entry pointer
; Returns:
;   rax = 1 if added, 0 if registry full
; ============================================================================
AddDialogToRegistry PROC
    mov rbx, rcx
    
    ; Find empty slot
    mov rcx, offset dialog_registry
    mov rdx, max_dialogs
    xor r8, r8
    
find_slot:
    cmp r8, rdx
    jge registry_full
    
    cmp qword ptr [rcx+r8*8], 0
    je found_slot
    
    inc r8
    jmp find_slot
    
found_slot:
    mov [rcx+r8*8], rbx
    inc [active_dialog_count]
    mov rax, 1
    ret
    
registry_full:
    xor rax, rax
    ret
AddDialogToRegistry ENDP

; ============================================================================
; RemoveDialogFromRegistry: Remove dialog from registry
; Parameters:
;   rcx = dialog_entry pointer
; Returns:
;   rax = 1 if removed, 0 if not found
; ============================================================================
RemoveDialogFromRegistry PROC
    mov rbx, rcx
    
    ; Find dialog in registry
    mov rcx, offset dialog_registry
    mov rdx, max_dialogs
    xor r8, r8
    
find_dialog:
    cmp r8, rdx
    jge not_found
    
    cmp [rcx+r8*8], rbx
    je found_dialog
    
    inc r8
    jmp find_dialog
    
found_dialog:
    mov qword ptr [rcx+r8*8], 0
    dec [active_dialog_count]
    mov rax, 1
    ret
    
not_found:
    xor rax, rax
    ret
RemoveDialogFromRegistry ENDP

; ============================================================================
; PushModalDialog: Add to modal stack
; Parameters:
;   rcx = dialog_entry pointer
; Returns:
;   rax = 1 if pushed, 0 if stack full
; ============================================================================
PushModalDialog PROC
    ; Find empty slot in stack
    mov rcx, offset modal_dialog_stack
    mov rdx, 16
    xor r8, r8
    
find_stack_slot:
    cmp r8, rdx
    jge stack_full
    
    cmp qword ptr [rcx+r8*8], 0
    je found_stack_slot
    
    inc r8
    jmp find_stack_slot
    
found_stack_slot:
    mov [rcx+r8*8], rbx
    mov rax, 1
    ret
    
stack_full:
    xor rax, rax
    ret
PushModalDialog ENDP

; ============================================================================
; PopModalDialog: Remove from modal stack
; Parameters:
;   rcx = dialog_entry pointer
; Returns:
;   rax = 1 if popped, 0 if not found
; ============================================================================
PopModalDialog PROC
    ; Find dialog in stack
    mov rcx, offset modal_dialog_stack
    mov rdx, 16
    xor r8, r8
    
find_in_stack:
    cmp r8, rdx
    jge not_in_stack
    
    cmp [rcx+r8*8], rbx
    je found_in_stack
    
    inc r8
    jmp find_in_stack
    
found_in_stack:
    mov qword ptr [rcx+r8*8], 0
    mov rax, 1
    ret
    
not_in_stack:
    xor rax, rax
    ret
PopModalDialog ENDP

; ============================================================================
; DialogSystemShutdown: Cleanup dialog system
; Parameters: None
; Returns: None
; ============================================================================
DialogSystemShutdown PROC
    ; Cleanup all active dialogs
    mov rcx, offset dialog_registry
    mov rdx, max_dialogs
    xor r8, r8
    
cleanup_loop:
    cmp r8, rdx
    jge cleanup_done
    
    cmp qword ptr [rcx+r8*8], 0
    je skip_dialog
    
    ; Destroy dialog window
    mov r9, [rcx+r8*8]
    mov r10, [r9+DIALOG_ENTRY.hwnd]
    test r10, r10
    jz skip_destroy
    
    push rcx
    push rdx
    push r8
    mov rcx, r10
    call DestroyWindow
    pop r8
    pop rdx
    pop rcx
    
skip_destroy:
    ; Free dialog entry
    push rcx
    push rdx
    push r8
    mov rcx, [rcx+r8*8]
    call free
    pop r8
    pop rdx
    pop rcx
    
    mov qword ptr [rcx+r8*8], 0
    
skip_dialog:
    inc r8
    jmp cleanup_loop
    
cleanup_done:
    mov [dialog_system_initialized], 0
    mov [active_dialog_count], 0
    ret
DialogSystemShutdown ENDP

END